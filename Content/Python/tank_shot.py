"""截图辅助：把 latent action 的返回对象存住，避免被 Python GC 掉后回调指针悬空。

为什么需要这个模块（**重要，别再踩**）：
  `unreal.AutomationLibrary.take_high_res_screenshot(...)` 是 latent Blueprint action，
  返回一个代理对象，真正的截图在后续帧完成、通过该代理上的回调通知。
  文件桥的每次 exec 都是全新的 globals，调用完返回值就被丢弃 → 代理被 GC →
  回调指针悬空 → 后续帧/GC 时 **编辑器直接 EXCEPTION_ACCESS_VIOLATION 崩溃**
  （callstack 最内层是 python311.dll，故障地址形如 0x00007465736c6f18 = ASCII 文本当指针用）。

  实测：同一个桥调用里「击杀 + 截图」连做 3 次，约 50s 后必崩；
  把返回对象存进本模块的模块级列表（模块在 sys.modules 里长期存活）即可避开。

用法（在桥脚本里）：
    import tank_shot
    tank_shot.shot("hud_check.png")
"""

import unreal

# 模块级列表：模块对象在 sys.modules 中存活于整个解释器生命周期，
# 因此这里持有的代理不会被 GC
_KEEP = []


def shot(name, width=1600, height=900):
    """请求高清截图并持有其 latent 代理，返回已持有的代理数量。"""
    proxy = unreal.AutomationLibrary.take_high_res_screenshot(
        width, height, name, None, False, False
    )
    _KEEP.append(proxy)
    return len(_KEEP)


def held():
    return len(_KEEP)
