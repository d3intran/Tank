"""切换 PIE 联机模式（写 Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini 里
[/Script/UnrealEd.LevelEditorPlaySettings] 的三个键）。**必须在编辑器关闭时跑**，
否则编辑器退出时会用内存里的值覆盖。

为什么需要它：载具升级阶段 1 只做单人驾驶（联机同步是阶段 3），
而项目默认 PIE 是「ListenServer + 3 客户端」。3 客户端下：
  - 联机链路未接，客户端那几台车只在本地各算各的；
  - 主机玩家在 PIE 里经常拿不到 Pawn（本项目的占有巡检虽会补，但测试单人驾驶时徒增噪声）。

用法：
    uv run --no-project python Scripts/tools/pie_mode.py standalone   # 单人
    uv run --no-project python Scripts/tools/pie_mode.py listen3      # 恢复三开
"""

import pathlib
import sys

INI = pathlib.Path(r"E:\UE\Tank\Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini")

MODES = {
    "standalone": {"PlayNetMode": "PIE_Standalone", "RunUnderOneProcess": "True", "PlayNumberOfClients": "1"},
    "listen3": {"PlayNetMode": "PIE_ListenServer", "RunUnderOneProcess": "True", "PlayNumberOfClients": "3"},
}

KEYS = ("PlayNetMode", "RunUnderOneProcess", "PlayNumberOfClients")


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in MODES:
        print("用法: pie_mode.py <%s>" % "|".join(MODES))
        return 1

    target = MODES[sys.argv[1]]
    lines = INI.read_text(encoding="utf-8").split("\n")
    in_section = False
    seen = set()
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("["):
            in_section = stripped == "[/Script/UnrealEd.LevelEditorPlaySettings]"
            continue
        if not in_section:
            continue
        key = stripped.split("=", 1)[0]
        if key in KEYS:
            lines[i] = "%s=%s" % (key, target[key])
            seen.add(key)

    missing = [k for k in KEYS if k not in seen]
    if missing:
        # 段落缺键时补在段首（新装的编辑器可能整段都还没写）
        idx = next(i for i, l in enumerate(lines) if l.strip() == "[/Script/UnrealEd.LevelEditorPlaySettings]")
        for offset, key in enumerate(missing, start=1):
            lines.insert(idx + offset, "%s=%s" % (key, target[key]))

    INI.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print("已切到 %s：%s（缺键补齐: %s）" % (sys.argv[1], target, missing or "无"))
    return 0


sys.exit(main())
