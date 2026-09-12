"""M4c-2 取证截图 · 拍摄：读 _shot.txt 里的名字，调 tank_shot.shot(name)。"""

import os

import tank_shot
import unreal

out.clear()

SHOT = os.path.join(unreal.Paths.project_dir(), "Scripts", "_shot.txt")
with open(SHOT, "r", encoding="utf-8") as f:
    name = f.readline().strip()

out.append("tank_shot 已持有代理数: %d" % tank_shot.shot(name))
