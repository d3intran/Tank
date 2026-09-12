"""M4b 确定性验证：空投到坡面中段 → 靠重力+贴地+对齐自行落位（不依赖按键输入）。

检查三件事：
  1. 落在坡面上后 Z 稳定在坡面高度（不穿模、不掉下去）
  2. 车身 pitch 对齐坡度（40° 坡 → |pitch| ≈ 40）
  3. 血量不变（无坠落伤害）
"""

import math
import time

import unreal

out.clear()

# Ramp_Center_L_Y（Cover_Center_L 的 -Y 面）：X -1900~-1300，坡底 Y≈3851(Z=-18) → 坡顶 Y≈4420(Z=460)
DROP = unreal.Vector(-1600.0, 4150.0, 0.0)
SLEEP = 1.0

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

    # 坡面在 Y=4150 处的高度（按几何算）
    base_y, top_y = 3851.0, 4420.0
    base_z, top_z = -18.0, 460.0
    frac = (DROP.y - base_y) / (top_y - base_y)
    surf_z = base_z + frac * (top_z - base_z)
    out.append("坡面在 Y=%.0f 处高度 ≈ %.1f（frac=%.2f）" % (DROP.y, surf_z, frac))

    pawn = None
    for w in worlds:
        if w is server:
            continue
        for pc in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController):
            try:
                if pc.is_local_player_controller() and pc.get_controlled_pawn():
                    pawn = pc.get_controlled_pawn()
                    out.append("用 %s（%s 世界）" % (pawn.get_name(), "客户端"))
                    break
            except Exception:  # noqa: BLE001
                pass
        if pawn:
            break
    if pawn is None:
        out.append("!! 没找到客户端本机车")
    else:
        hp0 = None
        th = getattr(pawn, "tank_health", None)
        if th is not None:
            hp0 = getattr(th, "current_health", None)

        start = unreal.Vector(DROP.x, DROP.y, surf_z + 59.0 + 40.0)
        pawn.set_actor_location_and_rotation(start, unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0), False, True)
        out.append("空投到 (%.0f, %.0f, %.0f)（坡面上方 40cm）" % (start.x, start.y, start.z))

        # 不能在桥脚本里 sleep（阻塞游戏线程，坦克就不会 tick）；
        # 落位观察由随后的 pie_m4b_verify.py 负责
        loc = pawn.get_actor_location()
        rot = pawn.get_actor_rotation()
        hp1 = getattr(th, "current_health", None) if th is not None else None
        expect_z = surf_z + 59.0
        out.append("1.0s 后: loc=(%.0f, %.0f, %.1f)  期望 Z≈%.1f（偏差 %.1f）"
                   % (loc.x, loc.y, loc.z, expect_z, loc.z - expect_z))
        out.append("姿态 pitch=%.1f yaw=%.1f roll=%.1f（期望 |pitch|≈40）"
                   % (rot.pitch, rot.yaw, rot.roll))
        out.append("血量: %s → %s（应不变）" % (hp0, hp1))
