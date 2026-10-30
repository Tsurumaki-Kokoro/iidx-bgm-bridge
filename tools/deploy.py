"""Validate all installed assets before installing the built plugin and profiles."""
import argparse
import configparser
import ctypes
from ctypes import wintypes
import hashlib
from pathlib import Path
import shutil
import struct
from datetime import datetime
PROJECT = Path(__file__).resolve().parents[1]
DAYS = 'sun mon tue wed thu fri sat'.split()
GAME_HASH = '3755587f44785effe5008f74cfe7dccbdf423ad09feead48e00a90f78a42fe83'

def process_names():

    class Entry(ctypes.Structure):
        _fields_ = [('dwSize', wintypes.DWORD), ('cntUsage', wintypes.DWORD), ('th32ProcessID', wintypes.DWORD), ('th32DefaultHeapID', ctypes.c_size_t), ('th32ModuleID', wintypes.DWORD), ('cntThreads', wintypes.DWORD), ('th32ParentProcessID', wintypes.DWORD), ('pcPriClassBase', wintypes.LONG), ('dwFlags', wintypes.DWORD), ('szExeFile', wintypes.WCHAR * 260)]
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    api.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    api.Process32FirstW.argtypes = api.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(Entry)]
    api.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = api.CreateToolhelp32Snapshot(2, 0)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry = Entry()
        entry.dwSize = ctypes.sizeof(entry)
        if not api.Process32FirstW(handle, ctypes.byref(entry)):
            raise ctypes.WinError(ctypes.get_last_error())
        names = set()
        while True:
            names.add(entry.szExeFile.lower())
            if not api.Process32NextW(handle, ctypes.byref(entry)):
                if ctypes.get_last_error() != 18:
                    raise ctypes.WinError(ctypes.get_last_error())
                return names
    finally:
        api.CloseHandle(handle)

def validate(game, asset_root=None):
    asset_root = asset_root or game
    if not hashlib.sha256((game / 'modules/bm2dx.dll').read_bytes()).hexdigest() == GAME_HASH:
        raise ValueError('unsupported game build')
    count = 0
    for profile in sorted((PROJECT / 'profiles').glob('*.ini')):
        ini = configparser.ConfigParser(interpolation=None)
        ini.read(profile, encoding='utf-8')
        for day in DAYS:
            for source, target in ini['map'].items():
                path = target if target.startswith('/') else ini['profile']['resource_root'] + '/' + target
                asset = asset_root / path.replace('{day}', day).lstrip('/')
                if not asset.is_file():
                    raise ValueError('missing asset: ' + str(asset))
                if not asset.read_bytes()[:4] == b'SD9\x00':
                    raise ValueError('invalid SD9: ' + str(asset))
                count += 1
            if ini['playback']['strategy'] == 'layered_sync':
                for variant in (0, 1):
                    headers = [(asset_root / f'data/sound/system/bgm_bridge/sinobuz24/layered/{day}/{variant}/syssd_bgm_default_l{i}.sd9').read_bytes()[:122] for i in range(3)]
                    if not len({(h[20:29], h[110:114]) for h in headers}) == 1:
                        raise ValueError('stem loops differ')
                    for h in headers:
                        if not struct.unpack_from('<I', h, 56)[0] == 44100:
                            raise ValueError('Validation failed')
    library = ctypes.WinDLL(str(PROJECT / 'build/bgm_bridge.dll'))
    if not library.spice_sdk_entry_point:
        raise ValueError('Validation failed')
    ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(library._handle))
    print(f'PASS: game hash, {count} mapped assets, 14 three-stem loop sets, DLL load/export')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game-root', type=Path, required=True)
    ap.add_argument('--validate-only', action='store_true')
    ap.add_argument('--asset-root', type=Path, help='Alternate asset tree for validation only')
    args = ap.parse_args()
    game = args.game_root.resolve()
    validate(game, args.asset_root)
    if args.validate_only:
        return
    if args.asset_root: raise ValueError("--asset-root requires --validate-only")
    if process_names().intersection({'spice.exe', 'spice64.exe', 'bm2dx.exe'}):
        raise ValueError('close game before deployment')
    backup = PROJECT / 'backups' / ('deploy-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    for name in ('bgm_bridge.dll', 'bgm_bridge.ini'):
        old = game / 'modules' / name
        if old.exists():
            shutil.copy2(old, backup / name)
    folder = game / 'modules/bgm_profiles'
    folder.mkdir(exist_ok=True)
    for profile in (PROJECT / 'profiles').glob('*.ini'):
        old = folder / profile.name
        if old.exists():
            shutil.copy2(old, backup / profile.name)
        shutil.copy2(profile, old)
    config = game / 'modules/bgm_bridge.ini'
    ini = configparser.ConfigParser()
    ini.read(config, encoding='utf-8-sig')
    if not ini.has_section('bgm_bridge'):
        ini.add_section('bgm_bridge')
    if 'profile' not in ini['bgm_bridge']:
        ini['bgm_bridge']['profile'] = 'sinobuz24'
    if 'fallback_profile' not in ini['bgm_bridge']:
        ini['bgm_bridge']['fallback_profile'] = 'sinobuz24_compat'
    for key, value in [('enabled', '1'), ('weekday', 'auto'), ('smooth_transition', '1')]:
        if key not in ini['bgm_bridge']:
            ini['bgm_bridge'][key] = value
    ini.remove_option('bgm_bridge', 'transition_ms')
    with config.open('w', encoding='utf-8') as stream:
        ini.write(stream)
    shutil.copy2(PROJECT / 'build/bgm_bridge.dll', game / 'modules/bgm_bridge.dll')
    if not hashlib.sha256((game / 'modules/bgm_bridge.dll').read_bytes()).digest() == hashlib.sha256((PROJECT / 'build/bgm_bridge.dll').read_bytes()).digest():
        raise ValueError('Validation failed')
    print('Installed; previous DLL/config saved in', backup)
if __name__ == '__main__':
    main()
