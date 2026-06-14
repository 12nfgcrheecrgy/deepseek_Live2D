import sys
import os
import json
import time
import argparse
import threading

try:
    import tkinter as tk
    from tkinter import ttk
except ImportError:
    print("tkinter not available", file=sys.stderr)
    sys.exit(1)


def read_progress(progress_file):
    """Read the latest progress from file."""
    try:
        if os.path.exists(progress_file):
            with open(progress_file, 'r', encoding='utf-8') as f:
                return json.load(f)
    except Exception:
        pass
    return None


def monitor_child_pid(pid, progress_file):
    """Monitor a child process PID and write its status to progress file."""
    try:
        import psutil
        proc = psutil.Process(pid)
        while proc.is_running():
            time.sleep(0.5)
    except Exception:
        pass
    # Child exited, write error if not done
    prog = read_progress(progress_file)
    if prog and not prog.get('done'):
        try:
            with open(progress_file, 'w', encoding='utf-8') as f:
                json.dump({"percent": 0, "message": "模型加载进程已退出", "error": True, "done": True}, f)
        except Exception:
            pass


def format_eta(seconds):
    """Format remaining seconds into human readable string."""
    if seconds < 0:
        return "即将完成..."
    if seconds < 60:
        return f"预计剩余 {int(seconds)} 秒"
    minutes = int(seconds / 60)
    secs = int(seconds % 60)
    return f"预计剩余 {minutes} 分 {secs} 秒"


def main():
    parser = argparse.ArgumentParser(description="AI Voice Model Loader GUI")
    parser.add_argument("--progress-file", required=True, help="Path to progress JSON file")
    parser.add_argument("--title", default="AI语音模型加载中", help="Window title")
    parser.add_argument("--monitor-pid", type=int, default=0, help="PID to monitor (auto-close when it exits)")
    parser.add_argument("--engine", default="SoVITS", help="Engine name to display")
    args = parser.parse_args()

    progress_file = args.progress_file
    engine_name = args.engine
    gui_start_time = time.time()

    # Ensure progress file exists with initial state
    try:
        with open(progress_file, 'w', encoding='utf-8') as f:
            json.dump({"percent": 5, "message": "正在启动模型加载程序...", "done": False}, f)
    except Exception:
        pass

    # Start PID monitor thread if needed
    if args.monitor_pid > 0:
        monitor_thread = threading.Thread(
            target=monitor_child_pid,
            args=(args.monitor_pid, progress_file),
            daemon=True
        )
        monitor_thread.start()

    root = tk.Tk()
    root.title(args.title)
    root.geometry("500x280")
    root.resizable(False, False)
    root.configure(bg="#2b2b2b")

    try:
        root.iconbitmap("")  # No icon for now
    except Exception:
        pass

    # Center window on screen
    root.update_idletasks()
    screen_width = root.winfo_screenwidth()
    screen_height = root.winfo_screenheight()
    x = (screen_width // 2) - (500 // 2)
    y = (screen_height // 2) - (280 // 2)
    root.geometry(f"+{x}+{y}")

    # Main frame
    main_frame = tk.Frame(root, bg="#2b2b2b", padx=20, pady=20)
    main_frame.pack(fill=tk.BOTH, expand=True)

    # Title label
    title_label = tk.Label(
        main_frame,
        text=f"正在加载 {engine_name} 语音模型",
        font=("Microsoft YaHei UI", 14, "bold"),
        fg="#ffffff",
        bg="#2b2b2b"
    )
    title_label.pack(pady=(0, 8))

    # Status label (main step description)
    status_text = tk.StringVar(value="初始化中...")
    status_label = tk.Label(
        main_frame,
        textvariable=status_text,
        font=("Microsoft YaHei UI", 10),
        fg="#cccccc",
        bg="#2b2b2b",
        wraplength=460
    )
    status_label.pack(pady=(0, 8))

    # Sub-status label (detail / sub-step)
    substatus_text = tk.StringVar(value="")
    substatus_label = tk.Label(
        main_frame,
        textvariable=substatus_text,
        font=("Microsoft YaHei UI", 9),
        fg="#aaaaaa",
        bg="#2b2b2b",
        wraplength=460
    )
    substatus_label.pack(pady=(0, 8))

    # Progress bar style
    style = ttk.Style()
    style.theme_use('clam')
    style.configure(
        "Custom.Horizontal.TProgressbar",
        troughcolor="#404040",
        background="#00bcd4",
        thickness=20
    )

    # Progress bar
    progress_var = tk.DoubleVar(value=0)
    progress_bar = ttk.Progressbar(
        main_frame,
        variable=progress_var,
        maximum=100,
        length=460,
        mode="determinate",
        style="Custom.Horizontal.TProgressbar"
    )
    progress_bar.pack(pady=(0, 6))

    # Percent + ETA row
    info_frame = tk.Frame(main_frame, bg="#2b2b2b")
    info_frame.pack(fill=tk.X, pady=(0, 6))

    percent_text = tk.StringVar(value="0%")
    percent_label = tk.Label(
        info_frame,
        textvariable=percent_text,
        font=("Microsoft YaHei UI", 9),
        fg="#888888",
        bg="#2b2b2b"
    )
    percent_label.pack(side=tk.LEFT)

    eta_text = tk.StringVar(value="")
    eta_label = tk.Label(
        info_frame,
        textvariable=eta_text,
        font=("Microsoft YaHei UI", 9),
        fg="#00bcd4",
        bg="#2b2b2b"
    )
    eta_label.pack(side=tk.RIGHT)

    # Detail label (VRAM/GPU info)
    detail_text = tk.StringVar(value="")
    detail_label = tk.Label(
        main_frame,
        textvariable=detail_text,
        font=("Microsoft YaHei UI", 8),
        fg="#666666",
        bg="#2b2b2b"
    )
    detail_label.pack(pady=(4, 0))

    last_progress = {"percent": 0, "message": "等待中...", "done": False}
    closing = False

    def update_progress():
        nonlocal last_progress, closing
        if closing:
            return

        prog = read_progress(progress_file)
        if prog is None:
            prog = last_progress
        else:
            last_progress = prog

        pct = float(prog.get("percent", 0))
        msg = prog.get("message", "加载中...")
        done = prog.get("done", False)
        error = prog.get("error", False)
        detail = prog.get("detail", "")
        elapsed_sec = prog.get("elapsed_sec", None)

        progress_var.set(min(pct, 100))
        status_text.set(msg)
        percent_text.set(f"{min(pct, 100):.0f}%")

        # Build substatus from detail or default empty
        if detail:
            detail_text.set(detail)

        # ETA calculation
        if done:
            eta_text.set("")
        elif error:
            eta_text.set("")
        elif pct > 3 and elapsed_sec is not None and elapsed_sec > 0:
            # Linear ETA estimate: total = elapsed / pct * 100
            total_est = elapsed_sec / pct * 100.0
            remaining = total_est - elapsed_sec
            eta_text.set(format_eta(remaining))
        elif pct > 0:
            # Fallback: use GUI local time if server hasn't reported elapsed yet
            local_elapsed = time.time() - gui_start_time
            total_est = local_elapsed / pct * 100.0
            remaining = total_est - local_elapsed
            eta_text.set(format_eta(remaining))
        else:
            eta_text.set("正在估算时间...")

        if error:
            status_label.config(fg="#ff6b6b")
            substatus_label.config(fg="#ff6b6b")
            progress_bar.config(style="red.Horizontal.TProgressbar")
            style.configure("red.Horizontal.TProgressbar", troughcolor="#404040", background="#ff6b6b", thickness=20)
            root.after(3000, lambda: root.destroy())
            return

        if done:
            progress_var.set(100)
            percent_text.set("100%")
            status_text.set("加载完成！")
            eta_text.set("")
            root.after(800, lambda: root.destroy())
            return

        root.after(200, update_progress)

    def on_close():
        nonlocal closing
        closing = True
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.after(200, update_progress)
    root.mainloop()


if __name__ == "__main__":
    main()
