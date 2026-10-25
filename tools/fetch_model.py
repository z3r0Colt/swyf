#!/usr/bin/env python3
"""Build-time only. Bundles the complete GGUF weight file INSIDE the APK, not a first-run download."""
import os, pathlib, sys, urllib.request
ROOT = pathlib.Path(__file__).resolve().parents[1]
DEST = ROOT / 'app/src/main/assets/model.gguf'
SOURCE = 'https://huggingface.co/bartowski/Qwen_Qwen3-1.7B-GGUF/resolve/main/Qwen3-1.7B-Q4_K_M.gguf?download=true'
MIN_SIZE = 1_000_000_000

def valid(path):
    if not path.is_file() or path.stat().st_size < MIN_SIZE: return False
    with path.open('rb') as f: return f.read(4) == b'GGUF'

if valid(DEST):
    print('Valid model already included:', DEST, DEST.stat().st_size, 'bytes')
    sys.exit(0)
if os.getenv('DIALTONE_MODEL_PATH'):
    source = pathlib.Path(os.environ['DIALTONE_MODEL_PATH'])
    if not valid(source): sys.exit('DIALTONE_MODEL_PATH is not a complete GGUF file')
    import shutil
    shutil.copyfile(source, DEST)
else:
    print('Downloading a 1.28 GB GGUF model for embedding at APK build time...')
    tmp = DEST.with_suffix('.gguf.part')
    try:
        req = urllib.request.Request(SOURCE, headers={'User-Agent':'DIALTONE-local-android-build/0.2'})
        with urllib.request.urlopen(req,timeout=120) as response, tmp.open('wb') as out:
            while True:
                chunk = response.read(4*1024*1024)
                if not chunk: break
                out.write(chunk)
        if not valid(tmp): raise RuntimeError('Incomplete or invalid GGUF download')
        tmp.replace(DEST)
    except Exception:
        if tmp.exists(): tmp.unlink()
        raise
assert valid(DEST)
print('Model included:',DEST,DEST.stat().st_size,'bytes')
