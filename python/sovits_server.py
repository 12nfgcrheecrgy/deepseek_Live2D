import sys
import json
import os
import traceback
import time
import io
import re
import warnings
import tempfile

# Suppress jieba pkg_resources deprecation warnings early, before any import
warnings.filterwarnings('ignore', message='pkg_resources is deprecated', category=UserWarning)

# Add ffmpeg binary to PATH early, before any library that might need it
_script_dir = os.path.dirname(os.path.abspath(__file__))
_project_root_early = os.path.dirname(_script_dir)
if os.path.basename(_project_root_early) == 'Release':
    _project_root_early = os.path.dirname(os.path.dirname(_project_root_early))
_ffmpeg_bin = os.path.join(_project_root_early, 'ffmpeg-8.1.1-essentials_build', 'bin')
if os.path.isdir(_ffmpeg_bin) and _ffmpeg_bin not in os.environ.get('PATH', ''):
    os.environ['PATH'] = _ffmpeg_bin + os.pathsep + os.environ.get('PATH', '')
    # Also set FFMPEG_BINARY for libraries that use it directly
    _ffmpeg_exe = os.path.join(_ffmpeg_bin, 'ffmpeg.exe')
    if os.path.isfile(_ffmpeg_exe):
        os.environ['FFMPEG_BINARY'] = _ffmpeg_exe
        os.environ['FFPROBE_BINARY'] = os.path.join(_ffmpeg_bin, 'ffprobe.exe')

# Add GPT-SoVITS to path and set up environment
# This script is at <root>/python/sovits_server.py or <root>/bin/Release/python/sovits_server.py
_script_dir = os.path.dirname(os.path.abspath(__file__))
_gpt_sovits_base = os.path.dirname(_script_dir)  # One level up from python/
# If we're at bin/Release/python/, go up two more levels to project root
if os.path.basename(_gpt_sovits_base) == 'Release':
    _gpt_sovits_base = os.path.dirname(os.path.dirname(_gpt_sovits_base))
for _candidate in [
    os.path.join(_gpt_sovits_base, 'GPT-SoVITS-20250606v2pro'),
    os.path.join(_gpt_sovits_base, 'GPT-SoVITS'),
]:
    if os.path.isdir(_candidate):
        if _candidate not in sys.path:
            sys.path.insert(0, _candidate)
        # inference_webui.py uses absolute imports (from text.LangSegmenter),
        # so GPT_SoVITS/ subdir must be on sys.path for these to resolve
        _pkg = os.path.join(_candidate, 'GPT_SoVITS')
        if os.path.isdir(_pkg) and _pkg not in sys.path:
            sys.path.insert(0, _pkg)
        # inference_webui.py uses cwd-relative paths (./weight.json, GPT_SoVITS/pretrained_models/)
        # so we must change cwd to the repo root
        os.chdir(_candidate)
        # Set absolute paths for pretrained models so transformers doesn't treat them as HF repo IDs
        _pretrained = os.path.join(_candidate, 'GPT_SoVITS', 'pretrained_models')
        # Use forward slashes for transformers (it rejects Windows backslash paths as repo IDs)
        os.environ.setdefault('bert_path', os.path.join(_pretrained, 'chinese-roberta-wwm-ext-large').replace('\\', '/'))
        os.environ.setdefault('cnhubert_base_path', os.path.join(_pretrained, 'chinese-hubert-base').replace('\\', '/'))
        break

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stdin.reconfigure(encoding='utf-8', errors='replace')

# Redirect ALL stdout to stderr so library logs don't pollute
# the JSON communication channel on fd 1.
# 1) Save the original stdout fd for send_response() to write JSON back to the C++ host
_original_stdout_fd = os.dup(1)
# 2) Redirect fd 1 to stderr at the OS level
os.dup2(2, 1)
# 3) Redirect Python-level sys.stdout to stderr immediately (not just in main())
sys.stdout = sys.stderr
# 4) Also redirect C-level stdout (FILE*) and Windows API stdout handle.
#    This prevents C extensions that captured stdout at load time from writing to the pipe.
try:
    import ctypes
    # --- C runtime level ---
    _msvcrt = ctypes.CDLL('msvcrt.dll')
    _msvcrt.fflush(None)
    _msvcrt._dup2(2, 1)
    # --- Windows API level (some libs use GetStdHandle) ---
    _kernel32 = ctypes.windll.kernel32
    _STD_OUTPUT_HANDLE = -11
    _STD_ERROR_HANDLE = -12
    _hStderr = _kernel32.GetStdHandle(_STD_ERROR_HANDLE)
    if _hStderr and _hStderr != -1:
        _kernel32.SetStdHandle(_STD_OUTPUT_HANDLE, _hStderr)
except Exception:
    pass

# 5) Redirect stderr (fd 2) to a log file to prevent pipe buffer from blocking.
#    Model loading produces massive log output. The C++ host's stderr pipe buffer
#    is only 4KB, and if it fills up the Python process blocks indefinitely.
#    We keep fd 1 already redirected to fd 2 (now log file), and send_response()
#    uses the saved _original_stdout_fd for JSON communication with the C++ host.
_log_dir = os.path.join(os.environ.get('TEMP', tempfile.gettempdir()), 'live2d_logs')
os.makedirs(_log_dir, exist_ok=True)
_log_path = os.path.join(_log_dir, 'sovits_server.log')
_log_fd = os.open(_log_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
os.dup2(_log_fd, 2)  # Redirect fd 2 (stderr) to log file
os.close(_log_fd)
sys.stderr = open(_log_path, 'a', encoding='utf-8', buffering=1)

# Compute the actual project root (not bin/Release/) for model file resolution.
# Mirrors the _gpt_sovits_base logic above.
_project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if os.path.basename(_project_root) == 'Release':
    _project_root = os.path.dirname(os.path.dirname(_project_root))

def resolve_model_path(rel_path):
    """Resolve a model file path relative to project root, with exe-dir fallback."""
    if not rel_path or os.path.isabs(rel_path):
        return rel_path
    # Try project root first
    path = os.path.join(_project_root, rel_path)
    if os.path.exists(path):
        return path
    # Fallback: try alongside the exe (bin/Release/ etc.)
    exe_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(exe_dir, rel_path)


def log(msg):
    print(f"[sovits_server] {msg}", file=sys.stderr, flush=True)

# Progress reporting for GUI loader
_progress_file = os.environ.get('TTS_PROGRESS_FILE', '')

def write_progress(percent, message, detail="", done=False, error=False, elapsed_sec=None):
    if not _progress_file:
        return
    try:
        data = {
            "percent": percent,
            "message": message,
            "detail": detail,
            "done": done,
            "error": error
        }
        if elapsed_sec is not None:
            data["elapsed_sec"] = round(elapsed_sec, 1)
        with open(_progress_file, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False)
    except Exception:
        pass

def receive_request():
    line = sys.stdin.readline().strip()
    if not line:
        raise EOFError("Client closed connection")
    return json.loads(line)

def send_response(data):
    line = json.dumps(data, ensure_ascii=False) + '\n'
    # Temporarily restore original stdout fd to send clean JSON
    os.dup2(_original_stdout_fd, 1)
    os.write(1, line.encode('utf-8'))
    os.dup2(2, 1)


class SoVITSEngine:
    def __init__(self):
        self.tts_pipeline = None
        self.tts_pipeline_name = None
        self.gpt_weights_path = None
        self.sovits_weights_path = None
        self.diffusion_weights_path = None
        self.reference_audio = None
        self.reference_audio_dir = None
        self._emotion_audio_map = {}
        self.prompt_text_cache = None
        self._prompt_audio_cache = None
        self.device = "cuda"
        self.is_half = True
        self.sample_rate = 44100

    def _scan_reference_audios(self, ref_dir):
        """Scan reference audio directory and build emotion->path mapping."""
        if not ref_dir or not os.path.isdir(ref_dir):
            return {}
        mapping = {}
        for fname in os.listdir(ref_dir):
            if not fname.lower().endswith(('.wav', '.mp3', '.flac', '.ogg', '.m4a')):
                continue
            fpath = os.path.join(ref_dir, fname)
            # Parse emotion tag like 【正常】【悲伤】from filename
            m = re.search(r'【(.+?)】', fname)
            if m:
                tag = m.group(1).strip()
            else:
                tag = 'neutral'
            # Map Chinese tags to English emotions
            emotion = self._map_emotion_tag(tag)
            if emotion not in mapping:
                mapping[emotion] = fpath
                log(f"  Ref audio mapping: {emotion} -> {fname}")
        return mapping

    @staticmethod
    def _map_emotion_tag(tag):
        tag = tag.lower()
        if tag in ('正常', '普通', '标准', 'neutral', '慵懒', '无聊', '假装不知道'):
            return 'neutral'
        if tag in ('开心', '高兴', '快乐', '骄傲', '激动', 'happy', 'excited'):
            return 'happy'
        if tag in ('悲伤', '伤心', '难过', '低落', 'sad', 'depressed'):
            return 'sad'
        if tag in ('生气', '愤怒', 'angry', '怒了', '急了'):
            return 'angry'
        if tag in ('惊讶', '震惊', '担心', 'surprised', 'shocked', 'worried'):
            return 'surprised'
        return 'neutral'

    def _get_ref_audio_for_emotion(self, emotion):
        """Get reference audio path for given emotion."""
        if emotion in self._emotion_audio_map:
            return self._emotion_audio_map[emotion]
        if 'neutral' in self._emotion_audio_map:
            return self._emotion_audio_map['neutral']
        if self.reference_audio and os.path.exists(self.reference_audio):
            return self.reference_audio
        return None

    def load_model(self, model_path, config_path, diffusion_path, diffusion_config,
                   gpt_weights=None, ref_audio=None, ref_audio_dir=None, use_fp16=True,
                   load_start=None):
        log(f"Loading SoVITS model...")
        log(f"  Model: {model_path}")
        log(f"  Config: {config_path}")
        log(f"  Diffusion: {diffusion_path}")
        log(f"  FP16: {use_fp16}")

        if load_start is None:
            load_start = time.time()

        def _elapsed():
            return time.time() - load_start

        self.is_half = use_fp16

        if not os.path.exists(model_path):
            raise FileNotFoundError(f"Model not found: {model_path}")

        # Monkey-patch transformers/huggingface_hub up front so both TTS_infer_pack
        # and inference_webui can load local Windows paths without hitting the network.
        write_progress(26, "正在准备环境...", detail="配置本地模型路径映射", elapsed_sec=_elapsed())
        try:
            from transformers import AutoTokenizer, AutoModelForMaskedLM
            _orig_tokenizer_from_pretrained = AutoTokenizer.from_pretrained
            _orig_model_from_pretrained = AutoModelForMaskedLM.from_pretrained
            def _local_tokenizer_from_pretrained(pretrained_model_name_or_path, *args, **kwargs):
                if os.path.isabs(pretrained_model_name_or_path) or os.path.isdir(pretrained_model_name_or_path):
                    kwargs.setdefault('local_files_only', True)
                return _orig_tokenizer_from_pretrained(pretrained_model_name_or_path, *args, **kwargs)
            def _local_model_from_pretrained(pretrained_model_name_or_path, *args, **kwargs):
                if os.path.isabs(pretrained_model_name_or_path) or os.path.isdir(pretrained_model_name_or_path):
                    kwargs.setdefault('local_files_only', True)
                return _orig_model_from_pretrained(pretrained_model_name_or_path, *args, **kwargs)
            AutoTokenizer.from_pretrained = staticmethod(_local_tokenizer_from_pretrained)
            AutoModelForMaskedLM.from_pretrained = staticmethod(_local_model_from_pretrained)
        except Exception:
            pass

        try:
            import huggingface_hub.utils._validators as _hf_validators
            _orig_validate_repo_id = _hf_validators.validate_repo_id
            def _patched_validate_repo_id(repo_id):
                if os.path.isabs(repo_id) or os.path.isdir(repo_id):
                    return
                return _orig_validate_repo_id(repo_id)
            _hf_validators.validate_repo_id = _patched_validate_repo_id
        except Exception:
            pass

        # Resolve absolute paths for pretrained models so transformers treats them as local dirs
        _pretrained = os.path.join(os.getcwd(), 'GPT_SoVITS', 'pretrained_models')
        _bert_path = os.path.join(_pretrained, 'chinese-roberta-wwm-ext-large').replace('\\', '/')
        _hubert_path = os.path.join(_pretrained, 'chinese-hubert-base').replace('\\', '/')

        # Helper to keep GUI alive during long blocking calls
        def _pulse_progress(percent, message, detail, interval=3.0):
            """Update progress file periodically until stop flag is set."""
            import threading
            stop = threading.Event()
            def _loop():
                while not stop.is_set():
                    write_progress(percent, message, detail=detail, elapsed_sec=_elapsed())
                    stop.wait(interval)
            t = threading.Thread(target=_loop, daemon=True)
            t.start()
            return stop

        # Try TTS_infer_pack first
        e1 = None
        try:
            from GPT_SoVITS.TTS_infer_pack.TTS import TTS, TTS_Config

            write_progress(28, "正在加载BERT编码器...", detail="chinese-roberta-wwm-ext-large", elapsed_sec=_elapsed())

            if config_path and os.path.exists(config_path):
                tts_config = TTS_Config(config_path)
            else:
                # Wrap in "custom" key so TTS_Config reads parameters correctly.
                # TTS_Config.__init__ reads from configs["custom"] when a dict is passed;
                # top-level keys outside "custom" are ignored.
                tts_config = TTS_Config({
                    "custom": {
                        "version": "v2",
                        "t2s_weights_path": gpt_weights if gpt_weights else "",
                        "vits_weights_path": model_path,
                        "bert_base_path": _bert_path,
                        "cnhuhbert_base_path": _hubert_path,
                        "device": self.device,
                        "is_half": self.is_half,
                    }
                })
            tts_config.device = self.device
            tts_config.is_half = self.is_half
            if gpt_weights and os.path.exists(gpt_weights):
                tts_config.t2s_weights_path = gpt_weights
            tts_config.vits_weights_path = model_path
            tts_config.version = "v2"

            write_progress(38, "正在加载HuBERT特征提取器...", detail="chinese-hubert-base", elapsed_sec=_elapsed())
            write_progress(45, "正在加载GPT语义模型权重...", detail=f"{'(默认)' if not gpt_weights else os.path.basename(gpt_weights)} — 模型加载进显存中，预计需要1-3分钟", elapsed_sec=_elapsed())
            log(f"[load_model] Starting TTS() construction — this may take 1-3 minutes...")
            pulse = _pulse_progress(45, "正在加载GPT语义模型权重...", detail=f"{os.path.basename(gpt_weights) if gpt_weights else '默认权重'} — 模型加载进显存中，请耐心等待")
            self.tts_pipeline = TTS(tts_config)
            pulse.set()
            log(f"[load_model] TTS() construction finished")
            self.tts_pipeline_name = "TTS_infer_pack"
            self.diffusion_weights_path = diffusion_path if os.path.exists(diffusion_path) else None
            self.reference_audio = ref_audio
            self.reference_audio_dir = ref_audio_dir
            write_progress(62, "正在加载SoVITS声学模型权重...", detail=f"{os.path.basename(model_path)}", elapsed_sec=_elapsed())
            self._emotion_audio_map = self._scan_reference_audios(ref_audio_dir)
            if ref_audio and os.path.exists(ref_audio):
                log(f"  Reference audio: {ref_audio}")
                write_progress(68, "正在缓存参考音频特征...", detail=f"{os.path.basename(ref_audio)}", elapsed_sec=_elapsed())
                self._cache_prompt_features(ref_audio)
            if diffusion_path and os.path.exists(diffusion_path):
                write_progress(70, "正在加载扩散模型...", detail=f"{os.path.basename(diffusion_path)}", elapsed_sec=_elapsed())
            log("SoVITS model loaded via TTS_infer_pack")
            return

        except Exception as _e1:
            e1 = _e1
            log(f"TTS_infer_pack failed: {e1}, trying inference_webui...")

        # Fallback: inference_webui

        try:
            import GPT_SoVITS.inference_webui as webui

            write_progress(30, "TTS_infer_pack加载失败，回退到inference_webui...", detail="尝试备用加载方式", elapsed_sec=_elapsed())

            self.sovits_weights_path = model_path
            if gpt_weights and os.path.exists(gpt_weights):
                self.gpt_weights_path = gpt_weights
            else:
                self.gpt_weights_path = None

            if self.gpt_weights_path:
                log(f"  GPT weights: {self.gpt_weights_path}")
                write_progress(45, "正在加载GPT权重...", detail=f"{os.path.basename(self.gpt_weights_path)} — 模型加载进显存中，请耐心等待", elapsed_sec=_elapsed())
                pulse2 = _pulse_progress(45, "正在加载GPT权重...", detail=f"{os.path.basename(self.gpt_weights_path)} — 模型加载进显存中，请耐心等待")
                webui.change_gpt_weights(self.gpt_weights_path)
                pulse2.set()

            log(f"  SoVITS weights: {self.sovits_weights_path}")
            write_progress(55, "正在加载SoVITS主模型权重...", detail=f"{os.path.basename(self.sovits_weights_path)}", elapsed_sec=_elapsed())
            webui.change_sovits_weights(self.sovits_weights_path)

            if diffusion_path and os.path.exists(diffusion_path):
                self.diffusion_weights_path = diffusion_path
                write_progress(65, "正在加载扩散模型...", detail=f"{os.path.basename(diffusion_path)}", elapsed_sec=_elapsed())
                webui.change_diffusion_weights(self.diffusion_weights_path)

            self.reference_audio = ref_audio
            self.reference_audio_dir = ref_audio_dir
            self._emotion_audio_map = self._scan_reference_audios(ref_audio_dir)
            if ref_audio and os.path.exists(ref_audio):
                write_progress(68, "正在缓存参考音频特征...", detail=f"{os.path.basename(ref_audio)}", elapsed_sec=_elapsed())
                self._cache_prompt_features(ref_audio)

            self.tts_pipeline = "webui"
            self.tts_pipeline_name = "inference_webui"
            log("SoVITS model loaded via GPT_SoVITS.inference_webui")
            return

        except Exception as e2:
            raise RuntimeError(
                f"Failed to load SoVITS model via any method. "
                f"TTS_infer_pack error: {e1}. inference_webui error: {e2}"
            )

    def _cache_prompt_features(self, ref_audio_path):
        try:
            import numpy as np
            import soundfile as sf
            audio_data, sr = sf.read(ref_audio_path)
            self._prompt_audio_cache = audio_data
            log(f"Cached prompt audio: {len(audio_data)} samples at {sr}Hz")
        except Exception as e:
            log(f"Prompt feature caching skipped: {e}")
            self._prompt_audio_cache = None

    def stream_synthesize(self, text, output_dir, emotion="neutral"):
        sentences = self._split_sentences(text)
        if not sentences:
            sentences = [text]

        results = []
        for i, sentence in enumerate(sentences):
            if not sentence.strip():
                continue
            output_path = os.path.join(output_dir, f"chunk_{i:04d}.wav")
            try:
                self.synthesize(sentence, output_path, emotion)
                results.append(output_path)
            except Exception as e:
                log(f"Chunk {i} synthesis failed: {e}")
                continue

        if len(results) == 1:
            return {"first": results[0], "rest": []}
        elif len(results) > 1:
            return {"first": results[0], "rest": results[1:]}
        else:
            return {"first": None, "rest": []}

    def _split_sentences(self, text):
        text = re.sub(r'\s+', '', text)
        sentences = re.split(r'(?<=[。！？.!?\n])', text)
        sentences = [s.strip() for s in sentences if s.strip()]
        if len(sentences) <= 1:
            sentences = re.split(r'(?<=[，,、；;：:])', text)
            sentences = [s.strip() for s in sentences if s.strip()]
        return sentences

    def synthesize(self, text, output_path, emotion="neutral"):
        if self.tts_pipeline is None:
            raise RuntimeError("Model not loaded")

        output_dir = os.path.dirname(output_path)
        if output_dir:
            os.makedirs(output_dir, exist_ok=True)

        # Always use the default reference audio (neutral) — it's the only one
        # guaranteed to work. Emotion is controlled via top_k/temperature params.
        # Previously switching ref audio by emotion caused silent output for some files.
        if not self.reference_audio or not os.path.exists(self.reference_audio):
            raise RuntimeError("No valid reference audio available")

        log(f"[synthesize] emotion={emotion}, ref_audio={os.path.basename(self.reference_audio)}, prompt_text={self.prompt_text_cache[:40] if self.prompt_text_cache else 'NOT SET'}")

        if self.tts_pipeline_name == "inference_webui":
            return self._synthesize_webui(text, output_path, emotion)
        else:
            return self._synthesize_pack(text, output_path, emotion)

    def _synthesize_webui(self, text, output_path, emotion):
        import GPT_SoVITS.inference_webui as webui
        import numpy as np

        kwargs = {
            'text': text,
            'text_language': 'zh',
            'how_to_cut': '凑四句一切',
        }

        if self.reference_audio and os.path.exists(self.reference_audio):
            kwargs['ref_wav_path'] = self.reference_audio
            if self.prompt_text_cache is None:
                self._detect_prompt_text()
            kwargs['prompt_text'] = self.prompt_text_cache or ''
            kwargs['prompt_language'] = 'zh'

        if self.diffusion_weights_path:
            kwargs['use_diffusion'] = True

        emotion_params = self._get_emotion_params(emotion)
        kwargs.update(emotion_params)

        log(f"[synthesize_webui] text={text[:80]}..., len={len(text)}")
        log(f"[synthesize_webui] ref_audio={kwargs.get('ref_wav_path','NONE')}, prompt_text={kwargs.get('prompt_text','NONE')[:60]}")

        try:
            result = webui.get_tts_wav(**kwargs)
            if isinstance(result, tuple):
                sampling_rate, audio_data = result
            else:
                audio_data = result
                sampling_rate = self.sample_rate

            # Detect silent output — prompt_text mismatch is a common cause
            nonzero = np.count_nonzero(audio_data)
            log(f"[synthesize_webui] sr={sampling_rate}, shape={audio_data.shape}, "
                f"dtype={audio_data.dtype}, min={audio_data.min():.4f}, max={audio_data.max():.4f}, "
                f"nonzero={nonzero}")
            if nonzero == 0:
                raise RuntimeError(
                    "Model produced silent audio — "
                    "prompt_text likely does not match the reference audio. "
                    "Create a matching .txt sidecar file next to the reference audio."
                )

            import soundfile as sf
            sf.write(output_path, audio_data, sampling_rate)
            return True
        except Exception as e:
            log(f"WebUI synthesis error: {e}")
            raise

    def _synthesize_pack(self, text, output_path, emotion):
        kwargs = {
            'text': text,
            'text_lang': 'zh',
        }
        if self.reference_audio and os.path.exists(self.reference_audio):
            kwargs['ref_audio_path'] = self.reference_audio
            if self.prompt_text_cache is None:
                self._detect_prompt_text()
            kwargs['prompt_text'] = self.prompt_text_cache or ''
            kwargs['prompt_lang'] = 'zh'

        emotion_params = self._get_emotion_params(emotion)
        kwargs.update(emotion_params)

        log(f"[synthesize_pack] text={text[:80]}..., len={len(text)}")
        log(f"[synthesize_pack] ref_audio={kwargs.get('ref_audio_path','NONE')}, prompt_text={kwargs.get('prompt_text','NONE')[:60]}")

        try:
            result = self.tts_pipeline.run(kwargs)
            import numpy as np
            import soundfile as sf
            # TTS.run is a generator, consume all yields
            result_items = list(result)
            if not result_items:
                raise RuntimeError("TTS returned empty audio")
            sampling_rate, audio_data = result_items[0]
            nonzero = np.count_nonzero(audio_data)
            log(f"[synthesize_pack] sr={sampling_rate}, shape={audio_data.shape}, "
                f"dtype={audio_data.dtype}, min={audio_data.min():.4f}, max={audio_data.max():.4f}, "
                f"nonzero={nonzero}")
            # Reject silent output — prompt_text mismatch is a common cause
            if nonzero == 0:
                raise RuntimeError(
                    "Model produced silent audio — "
                    "prompt_text likely does not match the reference audio. "
                    "Create a matching .txt sidecar file next to each reference audio."
                )
            sf.write(output_path, audio_data, sampling_rate)
            return True
        except Exception as e:
            log(f"TTS_infer_pack synthesis error: {e}")
            raise

    def _detect_prompt_text(self):
        if not self.reference_audio or not os.path.exists(self.reference_audio):
            return
        # Try reading a sidecar .txt file first
        txt_path = os.path.splitext(self.reference_audio)[0] + '.txt'
        if os.path.exists(txt_path):
            try:
                with open(txt_path, 'r', encoding='utf-8') as f:
                    self.prompt_text_cache = f.read().strip()
                if self.prompt_text_cache:
                    log(f"Loaded prompt text from {txt_path}")
                    return
            except Exception as e:
                log(f"Failed to read prompt text file: {e}")
        # Fallback: infer from filename, strip emotion tags like 【激动】
        name_no_ext = os.path.splitext(os.path.basename(self.reference_audio))[0]
        # Remove 【...】 emotion tags from the text
        name_no_ext = re.sub(r'【[^】]+】', '', name_no_ext).strip()
        self.prompt_text_cache = name_no_ext.replace('_', ' ').replace('-', ' ')
        log(f"Auto-detected prompt text: {self.prompt_text_cache}")

    def _get_emotion_params(self, emotion):
        params = {
            'top_k': 15,
            'top_p': 0.6,
            'temperature': 0.6,
        }

        if emotion == 'happy':
            params['top_k'] = 20
            params['top_p'] = 0.7
            params['temperature'] = 0.8
        elif emotion == 'surprised':
            params['top_k'] = 25
            params['top_p'] = 0.8
            params['temperature'] = 0.9
        elif emotion == 'sad':
            params['top_k'] = 10
            params['top_p'] = 0.5
            params['temperature'] = 0.5
        elif emotion == 'angry':
            params['top_k'] = 20
            params['top_p'] = 0.7
            params['temperature'] = 0.85

        return params


def main():
    log("Starting SoVITS TTS server...")
    write_progress(5, "正在启动SoVITS语音模型...", detail="检查GPU环境")

    try:
        import torch

        if not torch.cuda.is_available():
            write_progress(0, "CUDA不可用，SoVITS需要NVIDIA GPU", error=True)
            send_response({
                'status': 'error',
                'error': 'CUDA not available. GPU required for SoVITS.'
            })
            return

        gpu_name = torch.cuda.get_device_name(0)
        total_vram = torch.cuda.get_device_properties(0).total_memory / (1024 ** 3)
        free_vram, _ = torch.cuda.mem_get_info()
        free_vram_gb = free_vram / (1024 ** 3)

        detail = f"GPU: {gpu_name} | 显存: {total_vram:.1f}GB (可用 {free_vram_gb:.1f}GB)"
        log(f"GPU: {gpu_name}, Total VRAM: {total_vram:.1f}GB, Free VRAM: {free_vram_gb:.1f}GB")
        write_progress(10, "检测到GPU，正在初始化...", detail=detail)

        if free_vram_gb < 3.0:
            write_progress(0, f"显存不足: 仅{free_vram_gb:.1f}GB可用，需要至少3GB", error=True)
            send_response({
                'status': 'error',
                'error': f'Insufficient VRAM: {free_vram_gb:.1f}GB free, need at least 3GB'
            })
            return

        log("Initializing GPU optimizations...")
        write_progress(15, "正在优化GPU设置...", detail=detail)
        torch.backends.cudnn.benchmark = True
        if hasattr(torch.backends.cuda.matmul, 'allow_tf32'):
            torch.backends.cuda.matmul.allow_tf32 = True

        config_file = os.path.join(_project_root, 'config.json')
        cfg = {}
        if os.path.exists(config_file):
            with open(config_file, 'r', encoding='utf-8') as f:
                cfg = json.load(f)

        tts_cfg = cfg.get('tts', {})

        model_path = resolve_model_path(tts_cfg.get('sovits_model_path', ''))
        config_path = resolve_model_path(tts_cfg.get('sovits_config_path', ''))
        diffusion_path = resolve_model_path(tts_cfg.get('sovits_diffusion_path', ''))
        diffusion_config = resolve_model_path(tts_cfg.get('sovits_diffusion_config', ''))
        gpt_weights = resolve_model_path(tts_cfg.get('sovits_gpt_weights', ''))
        ref_audio = resolve_model_path(tts_cfg.get('sovits_reference_audio', ''))
        ref_audio_dir = resolve_model_path(tts_cfg.get('sovits_reference_dir', ''))

        use_fp16 = tts_cfg.get('sovits_fp16', True)

        if not model_path:
            model_path = resolve_model_path('芙宁娜/芙宁娜主模型 v7.0.pth')
        if not config_path:
            config_path = resolve_model_path('芙宁娜/芙宁娜主模型配置文件config.json')
        if not diffusion_path:
            diffusion_path = resolve_model_path('芙宁娜/芙宁娜扩散模型 v1.0.pt')
        if not diffusion_config:
            diffusion_config = resolve_model_path('芙宁娜/芙宁娜扩散模型配置文件diffusion.yaml')
        if not ref_audio:
            fallback = resolve_model_path('music/5l51ir4quuzh6t4c7t906rhnnh6vida.mp3')
            if os.path.exists(fallback):
                ref_audio = fallback

        log(f"Model: {model_path}")
        log(f"Config: {config_path}")
        log(f"Diffusion: {diffusion_path}")
        log(f"Reference audio: {ref_audio}")
        log(f"Reference dir: {ref_audio_dir}")

        write_progress(20, f"正在加载模型文件...", detail=f"主模型: {os.path.basename(model_path)}")

        load_start = time.time()

        engine = SoVITSEngine()
        write_progress(25, "正在初始化TTS推理引擎...", detail="加载BERT和HuBERT编码器", elapsed_sec=0)
        engine.load_model(
            model_path=model_path,
            config_path=config_path,
            diffusion_path=diffusion_path,
            diffusion_config=diffusion_config,
            gpt_weights=gpt_weights if gpt_weights else None,
            ref_audio=ref_audio if os.path.exists(ref_audio) else None,
            ref_audio_dir=ref_audio_dir if os.path.exists(ref_audio_dir) else None,
            use_fp16=use_fp16,
            load_start=load_start,
        )

        load_time = time.time() - load_start
        log(f"Models loaded in {load_time:.1f}s")
        write_progress(75, "模型加载完成，正在预热...", detail=f"耗时 {load_time:.1f}秒", elapsed_sec=load_time)

        log("Running warmup inference...")
        try:
            warmup_output = os.path.join(os.environ.get('TEMP', '.'), '_sovits_warmup.wav')
            engine.synthesize('你好', warmup_output, 'neutral')
            if os.path.exists(warmup_output):
                os.remove(warmup_output)
            log("Warmup completed")
        except Exception as e:
            log(f"Warmup failed (non-fatal): {e}")

        torch.cuda.empty_cache()
        _, after_vram = torch.cuda.mem_get_info()
        vram_used = (free_vram - after_vram) / (1024 ** 3)
        log(f"VRAM used after model load: {vram_used:.2f}GB")
        write_progress(95, "正在清理显存...", detail=f"显存占用: {vram_used:.2f}GB", elapsed_sec=load_time)

        send_response({
            'status': 'ready',
            'load_time_ms': round(load_time * 1000),
            'vram_used_gb': round(vram_used, 2),
            'gpu_name': gpu_name
        })
        write_progress(100, "SoVITS模型已就绪！", detail=f"{gpu_name} | 显存占用 {vram_used:.2f}GB", done=True)

        request_count = 0

        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue

            try:
                request = json.loads(line)
            except json.JSONDecodeError as e:
                log(f"Invalid JSON: {e}")
                send_response({'success': False, 'error': f'Invalid JSON: {e}'})
                continue

            command = request.get('command', '')
            if command == 'shutdown':
                log("Shutdown requested")
                send_response({'status': 'shutdown'})
                break

            if command == 'ping':
                send_response({'status': 'pong', 'request_count': request_count})
                continue

            if command == 'stream':
                text = request.get('text', '')
                output_dir = request.get('output_dir', '')
                emotion = request.get('emotion', 'neutral')

                if not text or not output_dir:
                    send_response({'success': False, 'error': 'Missing text or output_dir'})
                    continue

                try:
                    os.makedirs(output_dir, exist_ok=True)
                    synth_start = time.time()
                    result = engine.stream_synthesize(text, output_dir, emotion)
                    synth_time = time.time() - synth_start

                    torch.cuda.empty_cache()
                    request_count += 1
                    send_response({
                        'success': True,
                        'first': result.get('first'),
                        'rest': result.get('rest', []),
                        'synthesis_ms': round(synth_time * 1000)
                    })
                    log(f"Request #{request_count}: stream synthesized {len(result.get('rest', [])) + 1} chunks in {synth_time:.2f}s")
                except Exception as e:
                    torch.cuda.empty_cache()
                    send_response({
                        'success': False,
                        'error': f'{type(e).__name__}: {str(e)}'
                    })
                    traceback.print_exc(file=sys.stderr)
                continue

            text = request.get('text', '')
            output_path = request.get('output', '')
            emotion = request.get('emotion', 'neutral')

            if not text or not output_path:
                send_response({'success': False, 'error': 'Missing text or output'})
                continue

            try:
                output_dir = os.path.dirname(output_path)
                if output_dir:
                    os.makedirs(output_dir, exist_ok=True)

                synth_start = time.time()

                engine.synthesize(text, output_path, emotion)

                synth_time = time.time() - synth_start

                if os.path.exists(output_path):
                    file_size = os.path.getsize(output_path)
                    torch.cuda.empty_cache()
                    request_count += 1
                    send_response({
                        'success': True,
                        'output_file': output_path,
                        'file_size': file_size,
                        'synthesis_ms': round(synth_time * 1000)
                    })
                    log(f"Request #{request_count}: synthesized {file_size} bytes in {synth_time:.2f}s")
                else:
                    send_response({
                        'success': False,
                        'error': 'Output file was not created'
                    })

            except Exception as e:
                torch.cuda.empty_cache()
                send_response({
                    'success': False,
                    'error': f'{type(e).__name__}: {str(e)}'
                })
                traceback.print_exc(file=sys.stderr)

        log("SoVITS server shutting down")
        del engine
        torch.cuda.empty_cache()

    except ImportError as e:
        write_progress(0, f"GPT-SoVITS未安装: {e}", error=True)
        send_response({
            'status': 'error',
            'error': f'GPT-SoVITS not installed: {e}'
        })
    except Exception as e:
        err_msg = f"{type(e).__name__}: {str(e)}"
        write_progress(0, f"模型加载失败: {err_msg}", error=True)
        send_response({
            'status': 'error',
            'error': err_msg
        })
        traceback.print_exc(file=sys.stderr)


if __name__ == '__main__':
    main()
