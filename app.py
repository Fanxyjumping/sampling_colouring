"""
app.py – Flask + Flask-SocketIO web GUI for sampling_colouring.

Startup:
    pip install flask flask-socketio eventlet pillow
    python app.py
    Open http://localhost:5000

Design notes
────────────
* async_mode='threading'  – simpler and reliable for subprocess I/O;
                            eventlet is installed but not monkey-patched
                            so standard threading works normally.
* stdout buffering fix    – main.cpp calls setvbuf(stdout, NULL, _IOLBF, 0)
                            so every printf() flushes immediately over the pipe.
* Auto stdin responses    – a state-machine thread feeds the C++ program's
                            stdin prompts automatically:
                              ① image path  ② K value
                              ③ '\n' for the Task-1 "press Enter" pause
                              ④ 'y\n' for the "save images?" prompt
* imshow auto-dismiss     – cv::waitKey(0) blocks the subprocess.
                            A background thread sends a Space keystroke via
                            osascript (macOS only) at the right moments to
                            dismiss each OpenCV window without user interaction.
"""

import os
import sys
import base64
import threading
import subprocess
import platform
import time

from flask import Flask, render_template, request
from flask_socketio import SocketIO

# ── Paths ─────────────────────────────────────────────────────────────
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
BINARY      = os.path.join(PROJECT_DIR, 'sampling_colouring')

# Result image file names produced by the C++ program
RESULT_IMAGES = [
    ('seeds',     'result_seeds.png'),
    ('watershed', 'result_watershed.png'),
    ('coloring',  'result_coloring.png'),
]

IS_MACOS = (platform.system() == 'Darwin')

# Per-session save-decision synchronisation
_save_events:    dict = {}   # sid -> threading.Event
_save_decisions: dict = {}   # sid -> 'y' | 'n'

# ── Flask / SocketIO setup ─────────────────────────────────────────────
app       = Flask(__name__)
socketio  = SocketIO(app, async_mode='threading', cors_allowed_origins='*')


# ──────────────────────────────────────────────────────────────────────
# Helper: dismiss the frontmost macOS window (OpenCV's waitKey blocker)
# ──────────────────────────────────────────────────────────────────────
def _dismiss_cv_window():
    """
    Send a Space keystroke to the frontmost application via osascript.
    This unblocks cv::waitKey(0) in the C++ subprocess so the program
    can continue without any manual interaction.
    No-op on non-macOS platforms.
    """
    if not IS_MACOS:
        return
    try:
        subprocess.run(
            ['osascript', '-e',
             'tell application "System Events" to key code 49'],
            capture_output=True,
            timeout=3,
        )
    except Exception:
        pass   # never let this crash the backend


# ──────────────────────────────────────────────────────────────────────
# Worker – launched in a daemon thread for each "Run" request
# ──────────────────────────────────────────────────────────────────────
def _run_worker(sid: str, image_path: str, k_value: int) -> None:
    """
    Orchestrate the C++ subprocess:
      1. Launch ./sampling_colouring
      2. Auto-feed stdin (path, K, newlines)
      3. Stream stdout line-by-line to the frontend via SocketIO
      4. Auto-dismiss OpenCV imshow windows via osascript
      5. After exit, load result images and push as base64
    """

    def log(text: str) -> None:
        socketio.emit('log', {'text': text}, to=sid)

    # ── Banner ────────────────────────────────────────────────────────
    log('═' * 58)
    log(f'  图片路径 : {image_path}')
    log(f'  K 值     : {k_value}')
    log('═' * 58)

    # ── Sanity check: binary must exist ──────────────────────────────
    if not os.path.isfile(BINARY):
        log(f'[错误] 找不到可执行文件：{BINARY}')
        log('请先在项目目录执行 make')
        socketio.emit('done', {'success': False}, to=sid)
        return

    # ── Launch subprocess ─────────────────────────────────────────────
    try:
        proc = subprocess.Popen(
            [BINARY, '--headless'],
            cwd=PROJECT_DIR,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,   # merge stderr → stdout
            text=True,
            bufsize=1,                  # line-buffered on the Python side
        )
    except OSError as exc:
        log(f'[错误] 启动进程失败：{exc}')
        socketio.emit('done', {'success': False}, to=sid)
        return

    # ── stdin writer helper ───────────────────────────────────────────
    def send(text: str) -> None:
        """Write text to the subprocess's stdin (ignore broken pipe)."""
        try:
            proc.stdin.write(text)
            proc.stdin.flush()
        except (BrokenPipeError, OSError):
            pass

    # ── Feed initial inputs after a short startup delay ──────────────
    def _feed_initial() -> None:
        time.sleep(0.35)
        send(image_path + '\n')
        time.sleep(0.25)
        send(str(k_value) + '\n')

    threading.Thread(target=_feed_initial, daemon=True).start()

    # ── Stdout reader ─────────────────────────────────────────────────
    # In --headless mode the C++ program never opens any OpenCV windows
    # or interactive prompts, so we just stream every line straight to
    # the browser with no orchestration needed.
    for raw in proc.stdout:
        line = raw.rstrip('\n')
        log(line)

    # ── Wait for process to exit ──────────────────────────────────────
    proc.wait()
    log('─' * 58)
    log(f'  程序退出，返回码：{proc.returncode}')
    log('─' * 58)

    # ── Load and push result images as base64 ─────────────────────────
    any_sent = False
    for img_id, img_file in RESULT_IMAGES:
        full_path = os.path.join(PROJECT_DIR, img_file)
        if os.path.isfile(full_path):
            with open(full_path, 'rb') as fh:
                b64 = base64.b64encode(fh.read()).decode('ascii')
            socketio.emit('image', {'id': img_id, 'data': b64}, to=sid)
            log(f'  ✓ 图片已推送：{img_file}')
            any_sent = True
        else:
            log(f'  ⚠ 未找到图片：{img_file}')

    # ── Ask user whether to keep files on disk ────────────────────────
    if any_sent:
        evt = threading.Event()
        _save_events[sid]    = evt
        _save_decisions[sid] = 'n'          # safe default
        socketio.emit('save_prompt', {}, to=sid)

        evt.wait(timeout=120)               # wait up to 2 min for click

        decision = _save_decisions.pop(sid, 'n')
        _save_events.pop(sid, None)

        if decision == 'y':
            log('  ✓ 图片文件已保留在磁盘。')
        else:
            deleted = []
            for _, img_file in RESULT_IMAGES:
                fp = os.path.join(PROJECT_DIR, img_file)
                try:
                    if os.path.isfile(fp):
                        os.remove(fp)
                        deleted.append(img_file)
                except OSError:
                    pass
            log(f'  🗑  已删除：{", ".join(deleted) if deleted else "（无文件）"}')

    socketio.emit('done', {'success': any_sent}, to=sid)


# ──────────────────────────────────────────────────────────────────────
# Flask routes
# ──────────────────────────────────────────────────────────────────────
@app.route('/')
def index():
    return render_template('index.html')


# ──────────────────────────────────────────────────────────────────────
# SocketIO events
# ──────────────────────────────────────────────────────────────────────
@socketio.on('run')
def handle_run(data):
    """Validate inputs and start the worker thread."""
    sid        = request.sid
    image_path = (data.get('image_path') or '').strip()
    k_raw      = data.get('k_value', '')

    # ── Frontend validation (defence in depth) ────────────────────────
    if not image_path:
        socketio.emit('log',  {'text': '[错误] 图片路径不能为空'}, to=sid)
        socketio.emit('done', {'success': False},                  to=sid)
        return

    try:
        k_value = int(k_raw)
        if k_value <= 0:
            raise ValueError
    except (ValueError, TypeError):
        socketio.emit('log',  {'text': '[错误] K 必须为正整数'}, to=sid)
        socketio.emit('done', {'success': False},                to=sid)
        return

    threading.Thread(
        target=_run_worker,
        args=(sid, image_path, k_value),
        daemon=True,
    ).start()


@socketio.on('save_decision')
def handle_save_decision(data):
    """Receive the user's save (y/n) choice from the frontend."""
    sid    = request.sid
    choice = (data.get('choice') or 'n').strip().lower()
    _save_decisions[sid] = choice
    evt = _save_events.get(sid)
    if evt:
        evt.set()


# ──────────────────────────────────────────────────────────────────────
if __name__ == '__main__':
    print(f'\n  Project : {PROJECT_DIR}')
    print(f'  Binary  : {BINARY}')
    print('\n  Open http://localhost:5001 in your browser.\n')
    socketio.run(app, host='0.0.0.0', port=5001, debug=False)
