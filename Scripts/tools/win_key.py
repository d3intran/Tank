"""向指定窗口投递一次「按住 t 秒再松开」的按键（默认不抢焦点）。

为什么需要它：验证「客户端坦克能不能移动」必须让客户端本地真的产生输入，而 Python 侧
拿不到 EnhancedInputLocalPlayerSubsystem（ULocalPlayer::GetSubsystem 是模板、未反射；
SubsystemBlueprintLibrary 在本机 Python 绑定里不存在 —— 三条探针交叉确认过），
所以只能从窗口这一层投键。PIE 三开是同进程多窗口，可按标题精确定位到具体客户端。

为什么需要 --activate：UE 的键事件经 Slate 路由到「当前活动窗口」的焦点控件。
只 PostMessage WM_KEYDOWN 到非活动窗口时，键可能被投给编辑器（实测同一窗口时灵时不灵，
差别就在它当时是不是活动窗口）。`--activate` 先发一条 WM_ACTIVATE(WA_ACTIVE)，
让 Slate 的窗口簿记切过去，**不改变操作系统前台的窗口**；
`--foreground` 才是真的 SetForegroundWindow（会抢你的前台焦点，用后自动还回去）。

用法：python Scripts/tools/win_key.py "<窗口标题子串>" <按键字符> [按住秒数] [--activate|--foreground]
      python Scripts/tools/win_key.py "Client 1" W 1.5 --activate
"""

import ctypes
import sys
import time
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)

EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows.argtypes = [EnumWindowsProc, wintypes.LPARAM]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user32.MapVirtualKeyW.argtypes = [wintypes.UINT, wintypes.UINT]
user32.GetForegroundWindow.restype = wintypes.HWND
user32.SetForegroundWindow.argtypes = [wintypes.HWND]
user32.BringWindowToTop.argtypes = [wintypes.HWND]

WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_ACTIVATE = 0x0006
WA_ACTIVE = 1


def find_window(substr):
    found = []

    def _cb(hwnd, _lparam):
        if not user32.IsWindowVisible(hwnd):
            return True
        buf = ctypes.create_unicode_buffer(512)
        user32.GetWindowTextW(hwnd, buf, 512)
        if substr.lower() in buf.value.lower():
            found.append((hwnd, buf.value))
        return True

    user32.EnumWindows(EnumWindowsProc(_cb), 0)
    return found


def main():
    argv = sys.argv[1:]
    mode = "none"
    for flag in ("--activate", "--foreground"):
        if flag in argv:
            mode = flag.lstrip("-")
            argv.remove(flag)
    if len(argv) < 2:
        print(__doc__)
        return 2
    target, key = argv[0], argv[1].upper()
    hold = float(argv[2]) if len(argv) > 2 else 1.0

    matches = find_window(target)
    if not matches:
        print("没找到标题含 %r 的可见窗口" % target)
        return 1
    for hwnd, title in matches:
        print("目标窗口 hwnd=0x%08X  %s" % (hwnd, title))

    vk = ord(key)
    scan = user32.MapVirtualKeyW(vk, 0)
    down_lp = 1 | (scan << 16)
    up_lp = 1 | (scan << 16) | (1 << 30) | (1 << 31)

    prev_fg = user32.GetForegroundWindow()
    if mode == "activate":
        # 只改 Slate 的窗口簿记，不动操作系统前台
        for hwnd, _t in matches:
            print("  WM_ACTIVATE(WA_ACTIVE) -> %s" % bool(user32.PostMessageW(hwnd, WM_ACTIVATE, WA_ACTIVE, 0)))
        time.sleep(0.3)
    elif mode == "foreground":
        for hwnd, _t in matches:
            user32.BringWindowToTop(hwnd)
            print("  SetForegroundWindow -> %s" % bool(user32.SetForegroundWindow(hwnd)))
        time.sleep(0.5)

    for hwnd, _title in matches:
        ok_d = user32.PostMessageW(hwnd, WM_KEYDOWN, vk, down_lp)
        print("  WM_KEYDOWN('%s' vk=0x%02X) -> %s" % (key, vk, bool(ok_d)))
    time.sleep(hold)
    for hwnd, _title in matches:
        ok_u = user32.PostMessageW(hwnd, WM_KEYUP, vk, up_lp)
        print("  WM_KEYUP   -> %s（按住 %.2fs）" % (bool(ok_u), hold))

    if mode == "foreground" and prev_fg:
        user32.SetForegroundWindow(prev_fg)
        print("  已把前台焦点还给原窗口")
    return 0


if __name__ == "__main__":
    sys.exit(main())
