"""Prepare lossless-payload three-stem assets; requires numpy and build.cmd outputs."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import numpy as np

PROJECT = Path(__file__).resolve().parents[1]
DAYS = 'sun mon tue wed thu fri sat'.split()

def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True)

def decode(path, output):
    data = path.read_bytes()
    frames = struct.unpack_from('<I', data, 110)[0]
    run(PROJECT/'build/audio_codec.exe', 'decode', path, output)
    pcm = np.fromfile(output, dtype='<i2')[:frames*2].astype(np.float64)/32768
    if len(pcm) != frames*2:
        raise ValueError('short decode: '+str(path))
    return data, pcm

def measure(pcm, path):
    scale = min(1., .95/max(np.max(np.abs(pcm)), 1e-10))
    (pcm*scale*32767).round().astype('<i2').tofile(path)
    result = json.loads(run(PROJECT/'build/loudness_meter.exe', path))
    return result['integrated_lufs']-20*np.log10(scale)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source-root', type=Path, required=True)
    ap.add_argument('--game-root', type=Path, required=True)
    args = ap.parse_args()
    work = PROJECT/'build/asset-work'; work.mkdir(parents=True, exist_ok=True)
    destination = args.game_root/'data/sound/system/bgm_profiles/sinobuz24'
    report = []
    for variant in (0, 1):
        base = args.game_root/f'data/sound/system/{variant}'
        refdata, ref = decode(base/'syssd_bgm_default_l3.sd9', work/'reference.pcm')
        ref *= (128-struct.unpack_from('<h', refdata, 18)[0])/127 * 62/127
        reference = measure(ref, work/'measure.pcm')
        for day in DAYS:
            stems, headers, weights = [], [], []
            for layer in range(3):
                source = args.source_root/f'syssd_bgm_default_{day}_l{layer}.sd9'
                data, pcm = decode(source, work/f'layer{layer}.pcm')
                headers.append(data); stems.append(pcm)
                weights.append(((128-struct.unpack_from('<h', data, 18)[0])/127)**2)
            if len({(len(p), h[20:29]) for p,h in zip(stems,headers)}) != 1:
                raise ValueError('incompatible stem loops: '+day)
            selected = sum(p*w for p,w in zip(stems,weights))*96/127
            gain = 10**((reference-measure(selected, work/'measure.pcm'))/20)
            peak = max(np.max(np.abs(stems[0]*weights[0])),
                       np.max(np.abs(sum(p*w for p,w in zip(stems[:2],weights[:2]))*110/127)),
                       np.max(np.abs(selected)))
            gain = min(gain, .89/max(peak, 1e-10))
            attenuation = [round(128-127*w*gain) for w in weights]
            quantized = [(128-a)/127 for a in attenuation]
            actual = sum(p*w for p,w in zip(stems,quantized))*96/127
            folder = destination/day/str(variant); folder.mkdir(parents=True, exist_ok=True)
            for layer, original in enumerate(headers):
                data = bytearray(original)
                template = (base/f'syssd_bgm_default_l{layer}.sd9').read_bytes()
                data[14:18] = template[14:18]
                data[30:32] = template[30:32]
                struct.pack_into('<h', data, 18, attenuation[layer])
                path = folder/f'syssd_bgm_default_l{layer}.sd9'
                path.write_bytes(data)
                assert data[32:] == original[32:]
            report.append(dict(day=day, variant=variant, attenuation=attenuation,
                               reference_lufs=reference, selected_lufs=measure(actual,work/'measure.pcm'),
                               source_sha256=[hashlib.sha256(h).hexdigest() for h in headers]))
            print(day, variant, 'prepared', flush=True)
    (destination/'manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

if __name__ == '__main__':
    main()
