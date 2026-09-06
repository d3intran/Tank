"""AI Agent 文件桥：轮询命令目录，在编辑器内执行 Python/控制台命令并回写结果。

由 UE Python 插件在编辑器启动时自动加载（Content/Python/init_unreal.py 约定）。
协议：
  Agent 写入  Bridge/queue/<uuid>.json  : {"command": "<python 代码>", "type": "py"}
  桥执行后写  Bridge/results/<uuid>.json : {"ok": true/false, "result": "...", "error": "..."}
  执行完删除队列文件。仅限本机开发使用（等价于把编辑器 Python 控制台暴露给本地 Agent）。
"""

import unreal
import os
import json
import time

BRIDGE_DIR = os.path.join(unreal.Paths.project_dir(), "Bridge")
QUEUE_DIR = os.path.join(BRIDGE_DIR, "queue")
RESULTS_DIR = os.path.join(BRIDGE_DIR, "results")
POLL_SECONDS = 0.5


def _ensure_dirs():
    for d in (QUEUE_DIR, RESULTS_DIR):
        os.makedirs(d, exist_ok=True)


def _execute(payload):
    command = payload.get("command", "")
    cmd_type = payload.get("type", "py")
    started = time.time()
    if cmd_type == "py":
        buffer = []
        try:
            exec(compile(command, "<bridge>", "exec"), {"unreal": unreal, "out": buffer})
            return {"ok": True, "result": "\n".join(buffer) or "ok", "error": ""}
        except Exception as exc:  # noqa: BLE001 - 错误必须回传给调用方
            return {"ok": False, "result": "", "error": f"{type(exc).__name__}: {exc}"}
    if cmd_type == "log":
        unreal.log(f"[Bridge] {command}")
        return {"ok": True, "result": "", "error": ""}
    return {"ok": False, "result": "", "error": f"unknown type: {cmd_type}"}


def _poll(_delta_time=0.0):
    _ensure_dirs()
    try:
        files = [f for f in os.listdir(QUEUE_DIR) if f.endswith(".json")]
    except OSError:
        return
    for name in files:
        queue_path = os.path.join(QUEUE_DIR, name)
        try:
            with open(queue_path, encoding="utf-8") as fh:
                payload = json.load(fh)
        except (OSError, ValueError) as exc:
            result = {"ok": False, "result": "", "error": f"bad payload: {exc}"}
        else:
            result = _execute(payload)
        result["elapsed_ms"] = int((time.time() - started) * 1000)
        result_path = os.path.join(RESULTS_DIR, name)
        with open(result_path, "w", encoding="utf-8") as fh:
            json.dump(result, fh, ensure_ascii=False)
        os.remove(queue_path)


def _bootstrap():
    _ensure_dirs()
    # 编辑器启动即通知：桥已就绪
    with open(os.path.join(BRIDGE_DIR, "ready"), "w", encoding="utf-8") as fh:
        fh.write(str(time.time()))
    unreal.register_slate_post_tick_callback(_poll)
    unreal.log("[Bridge] init_unreal.py: file bridge active (poll %.1fs)" % POLL_SECONDS)


_bootstrap()
