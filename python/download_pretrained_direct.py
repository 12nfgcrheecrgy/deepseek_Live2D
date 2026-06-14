import os
import sys
import requests

BASE_URL = "https://hf-mirror.com/lj1995/GPT-SoVITS/resolve/main"
HEADERS = {"User-Agent": "Mozilla/5.0"}

_script_dir = os.path.dirname(os.path.abspath(__file__))
_gpt_sovits_base = os.path.dirname(_script_dir)
if os.path.basename(_gpt_sovits_base) == 'Release':
    _gpt_sovits_base = os.path.dirname(os.path.dirname(_gpt_sovits_base))

for _candidate in [
    os.path.join(_gpt_sovits_base, 'GPT-SoVITS-20250606v2'),
    os.path.join(_gpt_sovits_base, 'GPT-SoVITS-20250606v2pro'),
    os.path.join(_gpt_sovits_base, 'GPT-SoVITS'),
]:
    if os.path.isdir(_candidate):
        repo_root = _candidate
        break
else:
    print("GPT-SoVITS repo not found")
    sys.exit(1)

pretrained_dir = os.path.join(repo_root, 'GPT_SoVITS', 'pretrained_models')

# List all files in a repo subfolder via HuggingFace API
def list_files(subfolder):
    api_url = f"https://hf-mirror.com/api/models/lj1995/GPT-SoVITS/tree/main/{subfolder}"
    try:
        r = requests.get(api_url, headers=HEADERS, timeout=30)
        if r.status_code == 200:
            return r.json()
    except Exception as e:
        print(f"  Failed to list {subfolder}: {e}")
    return []

def download_file(rel_path, local_path):
    url = f"{BASE_URL}/{rel_path}"
    os.makedirs(os.path.dirname(local_path), exist_ok=True)
    if os.path.exists(local_path):
        print(f"  SKIP (exists): {rel_path}")
        return True
    try:
        print(f"  Downloading {rel_path} ...")
        r = requests.get(url, headers=HEADERS, stream=True, timeout=300)
        r.raise_for_status()
        total = int(r.headers.get('content-length', 0))
        downloaded = 0
        with open(local_path, 'wb') as f:
            for chunk in r.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
                    downloaded += len(chunk)
        size_mb = downloaded / (1024*1024)
        print(f"  OK: {rel_path} ({size_mb:.1f} MB)")
        return True
    except Exception as e:
        print(f"  FAIL: {rel_path} -> {e}")
        return False

all_ok = True

# Download BERT files
print("Listing chinese-roberta-wwm-ext-large ...")
files = list_files("chinese-roberta-wwm-ext-large")
for item in files:
    if item.get('type') == 'file':
        rel = item['path']
        local = os.path.join(pretrained_dir, rel.replace('/', os.sep))
        if not download_file(rel, local):
            all_ok = False

# Download HuBERT files
print("Listing chinese-hubert-base ...")
files = list_files("chinese-hubert-base")
for item in files:
    if item.get('type') == 'file':
        rel = item['path']
        local = os.path.join(pretrained_dir, rel.replace('/', os.sep))
        if not download_file(rel, local):
            all_ok = False

# Verify
required = [
    os.path.join(pretrained_dir, 'chinese-roberta-wwm-ext-large', 'config.json'),
    os.path.join(pretrained_dir, 'chinese-hubert-base', 'config.json'),
]
for f in required:
    if os.path.exists(f):
        print(f"  VERIFIED: {f}")
    else:
        print(f"  MISSING: {f}")
        all_ok = False

if all_ok:
    print("\nAll pretrained models downloaded successfully!")
    sys.exit(0)
else:
    print("\nSome files failed.")
    sys.exit(1)
