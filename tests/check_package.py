"""Check release layout, checksum and standalone importer after extracting a ZIP."""
import hashlib
import argparse
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--archive',type=Path)
args=parser.parse_args()
archives=[args.archive] if args.archive else list((ROOT/'dist').glob('*.zip'))
if not archives:raise RuntimeError('No release ZIP found')
for archive in archives:
    expected=archive.with_suffix('.sha256').read_text().split()[0]
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=expected:raise RuntimeError('Checksum mismatch')
    with zipfile.ZipFile(archive) as z:
        required={'modules/bgm_profiles/sparkle33.ini','tools/host_builds.json','modules/bgm_bridge.dll','modules/bgm_bridge.example.ini','modules/bgm_profiles/sinobuz24.ini','modules/bgm_profiles/sinobuz24_compat.ini','tools/import-resources.exe','tools/audio_codec.exe','tools/loudness_meter.exe','README.md','README.zh-CN.md','LICENSE','licenses/Python-LICENSE.txt'}
        legacy=['resort18','lincle19','tricoro20','spada21','pendual22','pendual22_future','copula23']
        required.update('modules/bgm_profiles/'+profile+'.ini' for profile in legacy)
        required.update('modules/bgm_profiles/'+profile+'_compat.ini' for profile in legacy[2:])
        if not required.issubset(z.namelist()):raise RuntimeError('Incomplete release layout')
        for name in z.namelist():
            if Path(name).suffix.lower() in {'.sd9','.wav','.pcm','.ifs','.mp4'}:raise RuntimeError('Game audio in release')
            if name.startswith('/') or '..' in Path(name).parts:raise RuntimeError('Unsafe ZIP path')
        with tempfile.TemporaryDirectory(prefix='bgm-package-',dir=ROOT/'build') as directory:
            z.extractall(directory)
            subprocess.run([str(Path(directory)/'tools/import-resources.exe'),'--help'],check=True)
    print('PASS: layout, no audio, checksum, extracted standalone importer:',archive.name)
