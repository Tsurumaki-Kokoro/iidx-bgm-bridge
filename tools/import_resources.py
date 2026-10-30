"""Create private runtime assets from local IIDX 24/34 files; never download audio."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
from datetime import datetime
import numpy as np

FROZEN = getattr(sys, 'frozen', False)
ROOT = Path(sys.executable).resolve().parent.parent if FROZEN else Path(__file__).resolve().parents[1]
BIN = ROOT/'tools' if FROZEN else ROOT/'build'
DAYS = 'sun mon tue wed thu fri sat'.split()
GAME_HASH = '3755587f44785effe5008f74cfe7dccbdf423ad09feead48e00a90f78a42fe83'
ASSET_PATH = Path('data/sound/system/bgm_bridge/sinobuz24')

def require(condition, message):
    if not condition: raise ValueError(message)

def run(*args):
    result = subprocess.run([str(a) for a in args], capture_output=True, text=True)
    if result.returncode: raise RuntimeError(result.stdout+result.stderr)
    return result.stdout

def sd9(path):
    data = path.read_bytes()
    require(len(data)>=122 and data[:4]==b'SD9\0' and data[32:36]==b'RIFF' and data[102:106]==b'fact', f'Unsupported SD9 layout: {path.name}')
    require(struct.unpack_from('<I', data, 8)[0]+32==len(data), f'Invalid SD9 size: {path.name}')
    require(struct.unpack_from('<I',data,56)[0]==44100, '44.1 kHz required')
    return data

def decode(path, output):
    data = sd9(path); frames = struct.unpack_from('<I',data,110)[0]
    run(BIN/'audio_codec.exe','decode',path,output)
    pcm = np.fromfile(output,dtype='<i2')[:frames*2].astype(np.float64)/32768
    require(len(pcm)==frames*2, 'Short audio decode')
    return data, pcm

def measure(pcm, path):
    scale = min(1., .95/max(np.max(np.abs(pcm)),1e-10))
    (pcm*scale*32767).round().astype('<i2').tofile(path)
    result = json.loads(run(BIN/'loudness_meter.exe',path))
    return result['integrated_lufs']-20*np.log10(scale)

def patch(data, target, attenuation=None):
    result = bytearray(data); result[14:18]=target[14:18]; result[30:32]=target[30:32]
    if attenuation is not None: struct.pack_into('<h',result,18,attenuation)
    return result

def check_assets(root, profiles):
    count = 0
    for profile in sorted(profiles.glob('*.ini')):
        ini = configparser.ConfigParser(interpolation=None); ini.read(profile,encoding='utf-8')
        for day in DAYS:
            for target in ini['map'].values():
                virtual = target if target.startswith('/') else ini['profile']['resource_root']+'/'+target
                path = root/virtual.replace('{day}',day).lstrip('/')
                sd9(path); count += 1
    return count

def prepare(source, game, output):
    source,game,output = source.resolve(),game.resolve(),output.resolve()
    require(hashlib.sha256((game/'modules/bm2dx.dll').read_bytes()).hexdigest()==GAME_HASH,'Unsupported IIDX 34 build')
    for binary in ('audio_codec.exe','loudness_meter.exe'): require((BIN/binary).is_file(),f'Missing {binary}; build first')
    profiles = ROOT/'modules/bgm_profiles' if FROZEN else ROOT/'profiles'
    profile = configparser.ConfigParser(interpolation=None); profile.read(profiles/'sinobuz24_compat.ini',encoding='utf-8')
    mappings = list(profile['map'])
    # Validate every input before creating any generated assets.
    for day in DAYS:
        for name in [*(f'syssd_bgm_default_{day}_l{i}.sd9' for i in range(3)),f'syssd_bgm_cardout_{day}.sd9',f'syssd_music_decide_se_{day}.sd9']:
            sd9(source/name)
    for route in mappings: sd9(game/'data/sound/system'/route)
    output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='bgm-bridge-',dir=output) as temporary:
        stage = Path(temporary); dest=stage/ASSET_PATH; reports=[]
        for variant in (0,1):
            base=game/f'data/sound/system/{variant}'
            refdata,ref=decode(base/'syssd_bgm_default_l3.sd9',stage/'reference.pcm')
            ref *= (128-struct.unpack_from('<h',refdata,18)[0])/127*62/127
            reference=measure(ref,stage/'measure.pcm')
            for day in DAYS:
                headers=[];stems=[];weights=[]
                for layer in range(3):
                    data,pcm=decode(source/f'syssd_bgm_default_{day}_l{layer}.sd9',stage/f'layer{layer}.pcm')
                    headers.append(data);stems.append(pcm)
                    weights.append(((128-struct.unpack_from('<h',data,18)[0])/127)**2)
                require(len({(len(p),h[20:29]) for p,h in zip(stems,headers)})==1,'Stem lengths/loops differ')
                frames=len(stems[0])//2
                require(struct.unpack_from('<I',headers[0],20)[0]==0 and struct.unpack_from('<I',headers[0],24)[0] in (0,frames*4),'Zero-start full-length loops required')
                selected=sum(p*w for p,w in zip(stems,weights))*96/127
                gain=10**((reference-measure(selected,stage/'measure.pcm'))/20)
                def peak_for(w):
                    return max(np.max(np.abs(stems[0]*w[0])),np.max(np.abs(sum(p*v for p,v in zip(stems[:2],w[:2]))*110/127)),np.max(np.abs(sum(p*v for p,v in zip(stems,w))*96/127)))
                gain=min(gain,.89/max(peak_for(weights),1e-10))
                attenuation=[round(128-127*w*gain) for w in weights]
                quantized=[(128-a)/127 for a in attenuation]
                require(peak_for(quantized)<.94,'Quantized gain exceeds peak ceiling')
                actual=sum(p*w for p,w in zip(stems,quantized))*96/127
                native=dest/'layered'/day/str(variant);native.mkdir(parents=True)
                for layer,original in enumerate(headers):
                    target=sd9(base/f'syssd_bgm_default_l{layer}.sd9')
                    data=patch(original,target,attenuation[layer])
                    require(data[32:]==original[32:],'Audio payload changed')
                    (native/f'syssd_bgm_default_l{layer}.sd9').write_bytes(data)
                # The host uses level 62 for complete mixes, versus 96 for each native stem.
                # Encode the calibrated sum once; use SD9 gain to compensate the host level.
                (actual*32767).round().astype('<i2').tofile(stage/'mix.pcm')
                template=source/f'syssd_bgm_default_{day}_l2.sd9'
                run(BIN/'audio_codec.exe','encode',template,stage/'mix.pcm',stage/'mix.sd9')
                mix,decoded=decode(stage/'mix.sd9',stage/'roundtrip.pcm')
                error=decoded-actual
                snr=10*np.log10(np.mean(actual**2)/max(np.mean(error**2),1e-30))
                require(snr>20 and np.max(np.abs(decoded))<.99,'Full-mix roundtrip quality check failed')
                compat=dest/'compat'/day/str(variant);compat.mkdir(parents=True)
                for route in mappings:
                    v,name=route.split('/')
                    if int(v)!=variant: continue
                    target=sd9(base/name)
                    if name.endswith(('_l0.sd9','_l1.sd9')):
                        layer=int(name[-5]); data=patch(headers[layer],target,round(128-127*quantized[layer]*127/62))
                    elif name.startswith('syssd_bgm_default_'):
                        data=patch(mix,target,round(128-127*127/62))
                    else:
                        kind='syssd_music_decide_se' if 'decide' in name else 'syssd_bgm_cardout'
                        data=patch(sd9(source/f'{kind}_{day}.sd9'),target)
                        if 'decide' in name: data[18:20]=target[18:20]
                    (compat/name).write_bytes(data)
                reports.append(dict(day=day,variant=variant,attenuation=attenuation,reference_lufs=reference,selected_lufs=measure(actual,stage/'measure.pcm'),sample_peak=float(peak_for(quantized)),roundtrip_snr_db=float(snr),source_sha256=[hashlib.sha256(h).hexdigest() for h in headers]))
                print(f'{day} / {variant}: ready',flush=True)
        count=check_assets(stage,profiles)
        files=[dict(path=p.relative_to(dest).as_posix(),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(dest.rglob('*.sd9'))]
        (dest/'manifest.json').write_text(json.dumps(dict(schema=1,calibration=reports,files=files),indent=2)+'\n',encoding='utf-8')
        final=output/ASSET_PATH; final.parent.mkdir(parents=True,exist_ok=True)
        backup=None
        if final.exists():
            backup=final.with_name('sinobuz24.backup-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'));final.rename(backup)
        try: shutil.move(str(dest),str(final))
        except Exception:
            if backup is not None: backup.rename(final)
            raise
        print(f'Imported {len(files)} files; validated {count} mappings. Output: {final}')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-root',type=Path,help='IIDX 24 data/sound/system/0 directory')
    ap.add_argument('--game-root',type=Path,help='IIDX 34 root directory')
    ap.add_argument('--output-root',type=Path,help='Alternate output root (defaults to game root)')
    args=ap.parse_args(); interactive=args.source_root is None or args.game_root is None
    try:
        source=args.source_root or Path(input('IIDX 24 data/sound/system/0: ').strip().strip('"'))
        game=args.game_root or Path(input('IIDX 34 root: ').strip().strip('"'))
        from deploy import process_names
        require(not process_names().intersection({'spice.exe','spice64.exe','bm2dx.exe'}),'Close the game before importing')
        prepare(source,game,args.output_root or game)
    except (OSError,ValueError,RuntimeError) as error:
        print(f'Import failed: {error}',file=sys.stderr)
        if interactive: input('Press Enter to close...')
        return 1
    if interactive: input('Finished. Press Enter to close...')
    return 0

if __name__=='__main__': sys.exit(main())
