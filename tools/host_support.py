"""Shared build registry for local import/deploy tools."""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(sys.executable).resolve().parent.parent if getattr(sys,'frozen',False) else Path(__file__).resolve().parents[1]

def builds():
    return json.loads((ROOT/'tools/host_builds.json').read_text(encoding='utf-8'))

def detect(game):
    digest=hashlib.sha256((game/'modules/bm2dx.dll').read_bytes()).hexdigest()
    for family,info in builds().items():
        if info['sha256']==digest:return family,info
    raise ValueError('Unsupported game executable SHA-256')
