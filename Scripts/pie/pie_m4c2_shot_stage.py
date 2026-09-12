"""M4c-2 取证截图 · 摆位：读 Scripts/_shot.txt 把三辆车一次摆好。

_shot.txt 格式（4 行）：
  行1: 截图名（不含 .png）
  行2: 主机车 x,y,z,yaw         ← 主机视口就是被截图的那个视口
  行3: Client1 车 x,y,z,pitch,yaw
  行4: Client2 车 x,y,z,pitch,yaw

搭配：pie_m4c2_shot_take.py（等 1~2s 让客户端车自行贴坡对齐后再截图）。
"""

import os

import unreal

out.clear()

SHOT = os.path.join(unreal.Paths.project_dir(), "Scripts", "_shot.txt")

with open(SHOT, "r", encoding="utf-8") as f:
    lines = [ln.strip() for ln in f if ln.strip()]
name = lines[0]


def nums(line):
    return [float(v) for v in line.split(",")]


def place(tank, vals, world):
    x, y, z = vals[0], vals[1], vals[2]
    pitch = vals[3] if len(vals) >= 5 else 0.0
    yaw = vals[4] if len(vals) >= 5 else vals[3]
    tank.set_actor_location_and_rotation(
        unreal.Vector(x, y, z), unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0), False, True)
    out.append("  %s → (%.0f,%.0f,%.0f) p=%.0f y=%.0f" % (tank.get_name(), x, y, z, pitch, yaw))


def local_tank(world):
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                return pc.get_controlled_pawn()
        except Exception:  # noqa: BLE001
            pass
    return None


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    clients = sorted([w for w in worlds if w is not server], key=lambda w: w.get_name())
    out.append("截图名 %s" % name)

    host = local_tank(server)
    if host is not None:
        place(host, nums(lines[1]), server)
    else:
        out.append("  !! 没找到主机车")

    for i, w in enumerate(clients):
        t = local_tank(w)
        if t is None:
            continue
        idx = i + 2
        if idx < len(lines):
            place(t, nums(lines[idx]), w)
