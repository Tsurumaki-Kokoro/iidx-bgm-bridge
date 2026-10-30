"""Build the standalone importer and an allowlisted, audio-free release ZIP."""
import argparse
import hashlib
from importlib import metadata
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[1]

def package(version):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,79}',version): raise ValueError('Invalid version')
    build=ROOT/'build'; output=ROOT/'dist'; output.mkdir(exist_ok=True)
    subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onefile',
                    '--name','import-resources','--distpath',str(build),'--workpath',str(build/'freeze'),
                    '--specpath',str(build),'--paths',str(ROOT/'tools'),str(ROOT/'tools/import_resources.py')],check=True,cwd=ROOT)
    subprocess.run([str(build/'import-resources.exe'),'--help'],check=True)
    name=f'IIDX-BGM-Bridge-{version}-win64'
    with tempfile.TemporaryDirectory(dir=build,prefix='package-') as temp:
        tree=Path(temp)
        def copy(source,target):
            dest=tree/target;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/source,dest)
        copy('build/bgm_bridge.dll','modules/bgm_bridge.dll')
        copy('config/bgm_bridge.example.ini','modules/bgm_bridge.example.ini')
        copy('tools/host_builds.json','tools/host_builds.json')
        for profile in (ROOT/'profiles').glob('*.ini'): copy(profile.relative_to(ROOT),'modules/bgm_profiles/'+profile.name)
        for tool in ('import-resources','audio_codec','loudness_meter'): copy('build/'+tool+'.exe','tools/'+tool+'.exe')
        for document in ('README.md','README.zh-CN.md','THIRD_PARTY_NOTICES.md','LICENSE'): copy(document,document)
        for document in (ROOT/'docs').glob('*.md'): copy(document.relative_to(ROOT),'docs/'+document.name)
        for component,file in [('minhook','LICENSE.txt'),('spicesdk','LICENSE')]: copy(f'vendor/{component}/{file}',f'licenses/{component}-{file}')
        for dependency in ('numpy','pyinstaller','altgraph','packaging','pefile','pywin32-ctypes','pyinstaller-hooks-contrib','setuptools'):
            dist=metadata.distribution(dependency)
            for file in dist.files or []:
                if 'license' in str(file).lower() or 'copying' in str(file).lower():
                    path=Path(dist.locate_file(file))
                    if path.is_file() and path.suffix.lower() in ('','.txt','.md','.rst'):
                        dest=tree/'licenses'/dependency/Path(str(file)).name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
        python_license=Path(sys.base_prefix)/'LICENSE.txt'
        if not python_license.is_file(): raise FileNotFoundError('Python LICENSE.txt is required for packaging')
        shutil.copy2(python_license,tree/'licenses/Python-LICENSE.txt')
        (tree/'BUILD.txt').write_text(f'IIDX BGM Bridge {version}\nSource: https://github.com/Tsurumaki-Kokoro/iidx-bgm-bridge\nNo game audio is included.\n',encoding='utf-8')
        archive=output/(name+'.zip')
        with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
            for path in sorted(tree.rglob('*')):
                if path.is_file():
                    if path.suffix.lower() in {'.sd9','.wav','.pcm','.ifs','.mp4'}: raise ValueError('Audio/media in release')
                    z.write(path,path.relative_to(tree).as_posix())
        digest=hashlib.sha256(archive.read_bytes()).hexdigest()
        (output/(name+'.sha256')).write_text(f'{digest}  {archive.name}\n',encoding='ascii')
        print('Created',archive)
        return archive

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--version',required=True)
    package(ap.parse_args().version)
