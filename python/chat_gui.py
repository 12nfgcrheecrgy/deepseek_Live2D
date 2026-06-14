"""
Live2D Companion - Chat GUI (tkinter)
Provides chat interface, exit button, DeepSeek API integration, and TTS.
"""
import sys
import os
import json
import time
import threading
import traceback
import asyncio
import queue
import subprocess
import tempfile
import argparse

# ── Path setup ────────────────────────────────────────────────────────
_script_dir = os.path.dirname(os.path.abspath(__file__))
_project_root = os.path.dirname(_script_dir)
if os.path.basename(_project_root) == 'Release':
    _project_root = os.path.dirname(os.path.dirname(_project_root))

sys.path.insert(0, _script_dir)
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')

# ── tkinter ────────────────────────────────────────────────────────────
try:
    import tkinter as tk
    from tkinter import ttk, scrolledtext, messagebox, font
except ImportError:
    print("tkinter not available", file=sys.stderr)
    sys.exit(1)

# ── Optional: system tray icon ─────────────────────────────────────────
HAS_PYSTRAY = False
try:
    import pystray
    from PIL import Image, ImageDraw
    HAS_PYSTRAY = True
except ImportError:
    pass

# ── Config ─────────────────────────────────────────────────────────────
def load_config():
    # Priority: bin/Release/config.json (has real keys) > project root (placeholder)
    candidates = []
    exe_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    # 1. bin/Release/config.json (deployed config with real API key)
    candidates.append(os.path.join(_project_root, 'bin', 'Release', 'config.json'))
    # 2. Alongside the script: python/../config.json
    candidates.append(os.path.join(exe_dir, 'config.json'))
    # 3. Project root config.json
    candidates.append(os.path.join(_project_root, 'config.json'))

    for path in candidates:
        if os.path.exists(path):
            try:
                with open(path, 'r', encoding='utf-8') as f:
                    cfg = json.load(f)
                api_key = cfg.get('deepseek', {}).get('api_key', '')
                if api_key and not api_key.startswith('YOUR_'):
                    return cfg
            except Exception:
                continue

    # Fallback: any config (even with placeholder key)
    for path in candidates:
        if os.path.exists(path):
            try:
                with open(path, 'r', encoding='utf-8') as f:
                    return json.load(f)
            except Exception:
                continue
    return {}

# ── DeepSeek API ───────────────────────────────────────────────────────
def deepseek_chat(user_message, screen_content=""):
    """Call DeepSeek API and return reply text."""
    cfg = load_config()
    ds_cfg = cfg.get('deepseek', {})
    api_key = ds_cfg.get('api_key', '')
    api_base = ds_cfg.get('api_base', 'https://api.deepseek.com/v1')
    model = ds_cfg.get('model', 'deepseek-v4-flash')
    max_tokens = ds_cfg.get('max_tokens', 1024)
    temperature = ds_cfg.get('temperature', 0.8)
    system_prompt = ds_cfg.get('system_prompt', '')

    if not api_key or api_key.startswith('YOUR_'):
        return None, "请先配置 DeepSeek API Key（编辑 config.json）"

    try:
        from openai import OpenAI
        client = OpenAI(api_key=api_key, base_url=api_base)

        messages = []
        if system_prompt:
            messages.append({"role": "system", "content": system_prompt})

        content = user_message
        if screen_content:
            content = f"[当前屏幕内容]:\n{screen_content}\n\n[用户输入]:\n{user_message}"

        messages.append({"role": "user", "content": content})

        response = client.chat.completions.create(
            model=model,
            messages=messages,
            max_tokens=max_tokens,
            temperature=temperature,
            stream=False
        )
        return response.choices[0].message.content, None
    except Exception as e:
        return None, str(e)

# ── TTS (Edge-TTS) ────────────────────────────────────────────────────
def speak_text(text):
    """Use edge-tts to speak text and return path to audio file."""
    cfg = load_config()
    tts_cfg = cfg.get('tts', {})
    voice = tts_cfg.get('voice', 'zh-CN-XiaoxiaoNeural')
    rate = tts_cfg.get('rate', '+0%')
    pitch = tts_cfg.get('pitch', '+0Hz')

    # Detect emotion
    lowered = text.lower()
    if any(kw in lowered for kw in ['!', '~', 'wow', '哈哈', '开心', '棒']):
        voice = tts_cfg.get('voice_happy', voice)
        rate = tts_cfg.get('rate_happy', rate)
        pitch = tts_cfg.get('pitch_happy', pitch)
    elif any(kw in lowered for kw in ['?', 'what', '咦', '啊', '什么']):
        voice = tts_cfg.get('voice_surprised', voice)
        rate = tts_cfg.get('rate_surprised', rate)
        pitch = tts_cfg.get('pitch_surprised', pitch)

    # Generate output path
    tmp_dir = tempfile.gettempdir()
    ts = time.strftime('%Y%m%d_%H%M%S')
    output_path = os.path.join(tmp_dir, f'furina_tts_{ts}.mp3')

    try:
        import edge_tts

        async def _gen():
            communicate = edge_tts.Communicate(
                text=text, voice=voice, rate=rate, pitch=pitch
            )
            await communicate.save(output_path)

        asyncio.run(_gen())

        if os.path.exists(output_path) and os.path.getsize(output_path) > 0:
            return output_path
        return None
    except ImportError:
        return None
    except Exception as e:
        traceback.print_exc()
        return None


def play_audio(filepath):
    """Play audio file asynchronously using Windows Media Player."""
    try:
        # Use PowerShell to play audio asynchronously
        ps_cmd = f'''
        Add-Type -AssemblyName presentationCore
        $player = New-Object System.Windows.Media.MediaPlayer
        $player.Open('{filepath}')
        $player.Play()
        Start-Sleep -Milliseconds 200
        while ($player.Position.Ticks -lt $player.NaturalDuration.TimeSpan.Ticks) {{
            Start-Sleep -Milliseconds 200
        }}
        $player.Close()
        '''
        subprocess.Popen(
            ['powershell', '-WindowStyle', 'Hidden', '-Command', ps_cmd],
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        return True
    except Exception:
        pass

    # Fallback: try winsound for WAV files
    if filepath.lower().endswith('.wav'):
        try:
            import winsound
            threading.Thread(target=lambda: winsound.PlaySound(
                filepath, winsound.SND_FILENAME | winsound.SND_ASYNC
            ), daemon=True).start()
            return True
        except Exception:
            pass

    return False


# ── Chat Application ───────────────────────────────────────────────────
class ChatApp:
    def __init__(self, root, standalone=True):
        self.root = root
        self.standalone = standalone
        self.chat_history = []
        self.is_processing = False
        self.task_queue = queue.Queue()
        self.tray_icon = None
        self.minimized_to_tray = False

        self.cfg = load_config()
        self._history_path = self._get_history_path()
        self._loaded_history = False

        self._setup_ui()
        self._setup_bindings()
        self._setup_tray()

        restored = self._load_history()
        if not restored:
            model_name = self.cfg.get('model', {}).get('name', 'Furina')
            self.add_message("Furina", f"主人好~ 我是{model_name}，来陪我聊天吧！✨")
        self._loaded_history = True

        # Focus input
        self.input_field.focus_set()

        # Start queue processor
        self.root.after(100, self._process_queue)

    # ── History persistence ─────────────────────────────────────
    def _get_history_path(self):
        appdata = os.environ.get('APPDATA', os.path.join(os.path.expanduser('~'), '.live2d'))
        hist_dir = os.path.join(appdata, 'Live2DCompanion')
        os.makedirs(hist_dir, exist_ok=True)
        return os.path.join(hist_dir, 'chat_history.json')

    def _load_history(self):
        if not os.path.exists(self._history_path):
            return False
        try:
            with open(self._history_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            messages = data.get('messages', [])
            if not messages:
                return False
            for msg in messages:
                sender = msg.get('sender', '')
                text = msg.get('text', '')
                is_user = msg.get('is_user', False)
                if sender and text:
                    self._add_message_impl(sender, text, is_user)
            return len(messages) > 0
        except Exception:
            return False

    def _save_message(self, sender, text, is_user):
        if not self._loaded_history:
            return
        messages = []
        if os.path.exists(self._history_path):
            try:
                with open(self._history_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    messages = data.get('messages', [])
            except Exception:
                messages = []
        messages.append({
            'sender': sender,
            'text': text,
            'is_user': is_user,
            'time': time.strftime('%Y-%m-%d %H:%M:%S')
        })
        if len(messages) > 500:
            messages = messages[-500:]
        try:
            with open(self._history_path, 'w', encoding='utf-8') as f:
                json.dump({'messages': messages}, f, ensure_ascii=False, indent=2)
        except Exception:
            pass

    def _setup_ui(self):
        self.root.title("芙宁娜 - Chat")
        self.root.geometry("420x550")
        self.root.minsize(360, 400)

        # Color scheme (Furina-themed: ocean blue + white + gold)
        self.COLORS = {
            'bg': '#f0f4f8',
            'chat_bg': '#ffffff',
            'user_bubble': '#d4eafc',
            'ai_bubble': '#fef9e7',
            'accent': '#3b82c4',
            'accent_dark': '#1d4ed8',
            'text': '#1e293b',
            'text_light': '#64748b',
            'send_btn': '#3b82c4',
            'send_btn_hover': '#2563eb',
            'exit_btn': '#ef4444',
            'exit_btn_hover': '#dc2626',
            'input_border': '#cbd5e1',
            'thinking_color': '#f59e0b',
        }

        self.root.configure(bg=self.COLORS['bg'])

        # ── Font ──────────────────────────────────────────────────
        default_font = ('Microsoft YaHei UI', 10)
        bold_font = ('Microsoft YaHei UI', 10, 'bold')
        title_font = ('Microsoft YaHei UI', 12, 'bold')
        small_font = ('Microsoft YaHei UI', 8)

        # ── Title bar ─────────────────────────────────────────────
        title_frame = tk.Frame(self.root, bg=self.COLORS['accent'], height=44)
        title_frame.pack(fill=tk.X, side=tk.TOP)
        title_frame.pack_propagate(False)

        title_label = tk.Label(
            title_frame,
            text="💧 芙宁娜 · Chat",
            font=title_font,
            fg='white',
            bg=self.COLORS['accent'],
            anchor='w',
            padx=12
        )
        title_label.pack(side=tk.LEFT, fill=tk.Y)

        # Exit button in title bar
        exit_btn = tk.Button(
            title_frame,
            text="✕ 退出",
            font=small_font,
            fg='white',
            bg=self.COLORS['exit_btn'],
            activebackground=self.COLORS['exit_btn_hover'],
            activeforeground='white',
            relief=tk.FLAT,
            padx=10,
            pady=2,
            cursor='hand2',
            command=self._on_exit
        )
        exit_btn.pack(side=tk.RIGHT, padx=8, pady=8)

        # ── Chat history ──────────────────────────────────────────
        chat_frame = tk.Frame(self.root, bg=self.COLORS['chat_bg'])
        chat_frame.pack(fill=tk.BOTH, expand=True, padx=8, pady=(8, 0))

        self.chat_display = tk.Text(
            chat_frame,
            wrap=tk.WORD,
            font=default_font,
            bg=self.COLORS['chat_bg'],
            fg=self.COLORS['text'],
            relief=tk.FLAT,
            padx=12,
            pady=10,
            state=tk.DISABLED,
            cursor='arrow',
            spacing1=2,
            spacing2=4,
            spacing3=2,
        )
        self.chat_display.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        scrollbar = ttk.Scrollbar(chat_frame, command=self.chat_display.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.chat_display.configure(yscrollcommand=scrollbar.set)

        # Configure text tags for styling
        self.chat_display.tag_configure('sender', font=bold_font, foreground=self.COLORS['accent_dark'])
        self.chat_display.tag_configure('user_sender', font=bold_font, foreground='#0284c7')
        self.chat_display.tag_configure('message', font=default_font, foreground=self.COLORS['text'])
        self.chat_display.tag_configure('thinking', font=small_font, foreground=self.COLORS['thinking_color'])
        self.chat_display.tag_configure('error', font=small_font, foreground='#ef4444')
        self.chat_display.tag_configure('system', font=small_font, foreground=self.COLORS['text_light'])

        # ── Status bar (progress bar + thinking text) ────────────
        status_frame = tk.Frame(self.root, bg=self.COLORS['bg'], height=32)
        status_frame.pack(fill=tk.X, side=tk.BOTTOM, padx=8, pady=(0, 2))
        status_frame.pack_propagate(False)

        self.progress_bar = ttk.Progressbar(
            status_frame, mode='indeterminate', length=100
        )
        self.progress_bar.pack(side=tk.LEFT, padx=(4, 8), pady=4)

        self.status_label = tk.Label(
            status_frame,
            text='',
            font=small_font,
            fg=self.COLORS['thinking_color'],
            bg=self.COLORS['bg'],
            anchor='w'
        )
        self.status_label.pack(side=tk.LEFT, fill=tk.X, expand=True)

        # ── Input area ────────────────────────────────────────────
        input_frame = tk.Frame(self.root, bg=self.COLORS['bg'])
        input_frame.pack(fill=tk.X, side=tk.BOTTOM, padx=8, pady=(0, 6))

        # Input text field
        self.input_field = tk.Text(
            input_frame,
            wrap=tk.WORD,
            font=default_font,
            height=3,
            relief=tk.SOLID,
            borderwidth=1,
            padx=10,
            pady=8,
            fg=self.COLORS['text'],
            bg='white',
            insertbackground=self.COLORS['accent'],
        )
        self.input_field.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.input_field.configure(
            highlightthickness=2,
            highlightcolor=self.COLORS['accent'],
            highlightbackground=self.COLORS['input_border'],
        )
        # Placeholder
        self._input_placeholder = '在这里输入消息... Enter 发送'
        self.input_field.insert('1.0', self._input_placeholder)
        self.input_field['fg'] = self.COLORS['text_light']
        self.input_field.bind('<FocusIn>', self._on_input_focus_in)
        self.input_field.bind('<FocusOut>', self._on_input_focus_out)

        # Send button
        self.send_btn = tk.Button(
            input_frame,
            text='发送 ↑',
            font=bold_font,
            fg='white',
            bg=self.COLORS['send_btn'],
            activebackground=self.COLORS['send_btn_hover'],
            activeforeground='white',
            relief=tk.FLAT,
            padx=20,
            pady=8,
            cursor='hand2',
            command=self._on_send
        )
        self.send_btn.pack(side=tk.RIGHT, padx=(8, 0))

    def _setup_bindings(self):
        # Enter to send (Shift+Enter for newline)
        self.input_field.bind('<Return>', self._on_enter_key)
        self.input_field.bind('<Shift-Return>', self._on_shift_enter)

        # Ctrl+Enter to send
        self.root.bind('<Control-Return>', lambda e: self._on_send())

        # Close window → minimize to tray or exit
        self.root.protocol('WM_DELETE_WINDOW', self._on_close)

        # Ctrl+C in chat display → copy
        self.chat_display.bind('<Control-c>', lambda e: self.root.clipboard_append(
            self.chat_display.selection_get()
        ))

    def _setup_tray(self):
        """Set up system tray icon (requires pystray + Pillow)."""
        if not HAS_PYSTRAY:
            return

        try:
            # Create a simple 64x64 icon
            img = Image.new('RGBA', (64, 64), color=(0, 0, 0, 0))
            draw = ImageDraw.Draw(img)
            # Blue circle
            draw.ellipse([8, 8, 56, 56], fill='#3b82c4', outline='#1d4ed8', width=2)
            # Water drop symbol
            draw.ellipse([24, 16, 40, 48], fill='white')

            menu = pystray.Menu(
                pystray.MenuItem('显示窗口', self._show_from_tray, default=True),
                pystray.MenuItem('─' * 8, lambda: None, enabled=False),
                pystray.MenuItem('退出', self._on_exit),
            )

            self.tray_icon = pystray.Icon(
                'furina_chat',
                img,
                '芙宁娜 Chat',
                menu
            )

            # Start tray in background thread
            threading.Thread(target=self.tray_icon.run, daemon=True).start()
        except Exception:
            pass

    def _on_close(self):
        """Handle window close - minimize to tray if available."""
        if self.tray_icon and not self.minimized_to_tray:
            self.root.withdraw()
            self.minimized_to_tray = True
        else:
            self._on_exit()

    def _show_from_tray(self):
        """Restore window from tray."""
        self.root.deiconify()
        self.root.lift()
        self.root.focus_force()
        self.minimized_to_tray = False

    def _on_exit(self):
        """Exit application."""
        if self.is_processing:
            if not messagebox.askyesno("确认退出", "正在处理中，确定要退出吗？"):
                return

        # Stop tray icon
        if self.tray_icon:
            try:
                self.tray_icon.stop()
            except Exception:
                pass

        self.root.quit()
        self.root.destroy()
        if self.standalone:
            sys.exit(0)

    def _on_enter_key(self, event):
        """Enter key pressed in input field."""
        # Check if Shift is held
        if event.state & 0x0001:  # Shift
            return None  # Let default handler insert newline
        self._on_send()
        return 'break'  # Prevent default newline

    def _on_shift_enter(self, event):
        """Shift+Enter - insert newline."""
        self.input_field.insert(tk.INSERT, '\n')
        return 'break'

    def _on_send(self):
        """Send button clicked or Enter pressed."""
        if self.is_processing:
            return

        text = self.input_field.get('1.0', 'end-1c').strip()
        if not text:
            return

        # Clear input
        self.input_field.delete('1.0', tk.END)

        # Display user message
        self.add_message("You", text, is_user=True)

        # Process in background
        self.is_processing = True
        self.set_thinking(True)
        self.send_btn.configure(state=tk.DISABLED, text='...')

        # Put task in queue
        self.task_queue.put(('chat', text))

    def _process_queue(self):
        """Process tasks from the queue in the main thread."""
        try:
            while True:
                task_type, data = self.task_queue.get_nowait()
                if task_type == 'chat':
                    threading.Thread(target=self._do_chat, args=(data,), daemon=True).start()
                elif task_type == 'add_message':
                    sender, text, is_user = data
                    self._add_message_impl(sender, text, is_user)
                elif task_type == 'set_thinking':
                    self._set_thinking_impl(data)
                elif task_type == 'done_processing':
                    self._done_processing_impl()
        except queue.Empty:
            pass

        self.root.after(100, self._process_queue)

    def _do_chat(self, user_message):
        """Perform chat request in background thread."""
        try:
            reply, error = deepseek_chat(user_message)

            if error:
                self.task_queue.put(('add_message', (
                    'Furina',
                    f"唔... 我的脑子有点乱：{error}",
                    False
                )))
            elif reply:
                self.task_queue.put(('add_message', ('Furina', reply, False)))
                # TTS in background
                threading.Thread(target=self._do_tts, args=(reply,), daemon=True).start()
            else:
                self.task_queue.put(('add_message', (
                    'Furina',
                    "主人~ 我好像断线了... 再试一次吧！",
                    False
                )))
        except Exception as e:
            self.task_queue.put(('add_message', (
                'Furina',
                f"出了点小问题：{e}",
                False
            )))

        self.task_queue.put(('done_processing', None))

    def _do_tts(self, text):
        """Generate and play TTS audio in background thread."""
        audio_path = speak_text(text)
        if audio_path:
            play_audio(audio_path)

    def _add_message_impl(self, sender, text, is_user=False):
        """Actually add message to chat display (called from main thread)."""
        self.chat_display.configure(state=tk.NORMAL)

        # Sender label
        tag = 'user_sender' if is_user else 'sender'
        self.chat_display.insert(tk.END, f'\n{sender}: ', tag)
        self.chat_display.insert(tk.END, f'{text}\n', 'message')

        self.chat_display.configure(state=tk.DISABLED)
        self.chat_display.see(tk.END)

        # Persist to history
        self._save_message(sender, text, is_user)

    def _set_thinking_impl(self, thinking):
        """Update thinking indicator with progress bar."""
        if thinking:
            self.status_label.configure(text='💭 芙宁娜正在思考...')
            self.progress_bar.start(15)  # animate at 15ms interval
        else:
            self.status_label.configure(text='')
            self.progress_bar.stop()

    def _done_processing_impl(self):
        """Reset UI after processing complete."""
        self.is_processing = False
        self.set_thinking(False)
        self.send_btn.configure(state=tk.NORMAL, text='发送 ↑')
        self.input_field.focus_set()

    def _on_input_focus_in(self, event):
        """Clear placeholder text on focus."""
        current = self.input_field.get('1.0', 'end-1c')
        if current == self._input_placeholder:
            self.input_field.delete('1.0', tk.END)
            self.input_field['fg'] = self.COLORS['text']

    def _on_input_focus_out(self, event):
        """Restore placeholder if empty."""
        current = self.input_field.get('1.0', 'end-1c').strip()
        if not current:
            self.input_field.delete('1.0', tk.END)
            self.input_field.insert('1.0', self._input_placeholder)
            self.input_field['fg'] = self.COLORS['text_light']

    # ── Public API (thread-safe) ──────────────────────────────────
    def add_message(self, sender, text, is_user=False):
        """Add message to chat (thread-safe)."""
        self.task_queue.put(('add_message', (sender, text, is_user)))

    def set_thinking(self, thinking):
        """Set thinking indicator (thread-safe)."""
        self.task_queue.put(('set_thinking', thinking))


# ── Main ────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description='Live2D Companion Chat GUI')
    parser.add_argument('--standalone', action='store_true', default=True,
                        help='Run as standalone app')
    args = parser.parse_args()

    root = tk.Tk()

    # Set DPI awareness on Windows
    try:
        import ctypes
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except Exception:
        pass

    app = ChatApp(root, standalone=args.standalone)

    # Center window on screen
    root.update_idletasks()
    w = root.winfo_width()
    h = root.winfo_height()
    sw = root.winfo_screenwidth()
    sh = root.winfo_screenheight()
    x = (sw - w) // 2
    y = (sh - h) // 2
    root.geometry(f'+{x}+{y}')

    root.mainloop()


if __name__ == '__main__':
    main()
