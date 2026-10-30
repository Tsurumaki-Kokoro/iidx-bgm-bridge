"""Shared build registry for local import/deploy tools."""
import json
from pathlib import Path
import sys
import re
import xml.etree.ElementTree as ET

ROOT = Path(sys.executable).resolve().parent.parent if getattr(sys,'frozen',False) else Path(__file__).resolve().parents[1]

def builds():
    return json.loads((ROOT/'tools/host_builds.json').read_text(encoding='utf-8'))

def metadata_tag(path,name):
    if not path.is_file():return None
    if path.stat().st_size>2*1024*1024:raise ValueError('Host metadata is too large')
    try:root=ET.fromstring(path.read_bytes().decode('utf-8',errors='replace'))
    except ET.ParseError as error:raise ValueError('Invalid host metadata: '+str(path)) from error
    values=root.findall('.//'+name)
    if len(values)>1:raise ValueError('Duplicate host metadata tag: '+name)
    return (values[0].text or '').strip() if values else None

def version(game):
    model=metadata_tag(game/'prop/ea3-config.xml','model')
    if model is not None and model!='LDJ':raise ValueError('Host model is not LDJ')
    release=metadata_tag(game/'prop/bootstrap.xml','release_code')
    if release is not None:result,source=release,'prop/bootstrap.xml release_code'
    else:result,source=metadata_tag(game/'prop/ea3-config.xml','ext'),'prop/ea3-config.xml ext'
    if not result or not re.fullmatch(r'20[0-9]{8}',result):raise ValueError('Missing or invalid host version')
    return result,source

def detect(game):
    if not (game/'modules/bm2dx.dll').is_file():raise ValueError('Missing modules/bm2dx.dll')
    release,source=version(game)
    for family,info in builds().items():
        if release in (info['version'],*info.get('aliases',[])):return family,dict(info,version=release,version_source=source)
    raise ValueError('Unsupported game version: '+release)
