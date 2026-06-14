import os
import sys
import shutil

os.environ.setdefault('HF_ENDPOINT', 'https://hf-mirror.com')

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
os.makedirs(pretrained_dir, exist_ok=True)

print(f"Target directory: {pretrained_dir}")
print(f"Using HF endpoint: {os.environ.get('HF_ENDPOINT', 'default')}")

REPO_ID = 'lj1995/GPT-SoVITS'

# Files we need, with their subfolder paths in the repo
NEEDED_FILES = [
    # BERT
    'chinese-roberta-wwm-ext-large/config.json',
    'chinese-roberta-wwm-ext-large/pytorch_model.bin',
    'chinese-roberta-wwm-ext-large/tokenizer_config.json',
    'chinese-roberta-wwm-ext-large/vocab.txt',
    'chinese-roberta-wwm-ext-large/special_tokens_map.json',
    'chinese-roberta-wwm-ext-large/added_tokens.json',
    # HuBERT
    'chinese-hubert-base/config.json',
    'chinese-hubert-base/pytorch_model.bin',
    'chinese-hubert-base/preprocessor_config.json',
    # GPT + VITS pretrained
    'gsv-v2final-pretrained/s1bert25hz-5kh-longer-epoch=12-step=369668.ckpt',
    'gsv-v2final-pretrained/s2G2333k.pth',
    'gsv-v2final-pretrained/s2D2333k.pth',
]

def download_file(repo_id, filename, local_dir):
    from huggingface_hub import hf_hub_download
    try:
        downloaded = hf_hub_download(
            repo_id=repo_id,
            filename=filename,
            local_dir=local_dir,
            local_dir_use_symlinks=False,
        )
        print(f"  OK: {filename}")
        return True
    except Exception as e:
        print(f"  FAIL: {filename} -> {e}")
        return False

all_ok = True
for rel_path in NEEDED_FILES:
    print(f"Downloading {rel_path} ...")
    if not download_file(REPO_ID, rel_path, pretrained_dir):
        all_ok = False

# Verify
required_check = [
    os.path.join(pretrained_dir, 'chinese-roberta-wwm-ext-large', 'config.json'),
    os.path.join(pretrained_dir, 'chinese-hubert-base', 'config.json'),
    os.path.join(pretrained_dir, 'gsv-v2final-pretrained', 's1bert25hz-5kh-longer-epoch=12-step=369668.ckpt'),
]

for f in required_check:
    if os.path.exists(f):
        size_mb = os.path.getsize(f) / (1024*1024)
        print(f"  VERIFIED: {os.path.basename(f)} ({size_mb:.1f} MB)")
    else:
        print(f"  MISSING: {f}")
        all_ok = False

if all_ok:
    print("\nAll pretrained models downloaded successfully!")
    sys.exit(0)
else:
    print("\nSome files failed to download.")
    sys.exit(1)
