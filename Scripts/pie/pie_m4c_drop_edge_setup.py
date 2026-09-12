"""M4c 跌落实测 · 准备：把 Client 1 本机车放到 SE 掩体顶面偏西处 (1350,3000)，朝 +X。

开到东缘 X=2000 后整车跌落到路面（顶面 460 → 路面 2.1，落差 ~458cm），
用于验证自由落体分支（重力 -980 / 落地着地）。顶面到东缘 650cm，400cm/s ≈ 1.6s，之后落体 ~1s。
"""

import unreal

out.clear()

SPOT = (1350.0, 3000.0)
FACE_YAW = 0.0
SLOW_SPEED = 400.0
TOP_Z = 460.0


def half_h_of(tank):
    box = getattr(tank, "collision_box", None)
    if box is not None:
        try:
            return box.get_scaled_box_extent().z
        except Exception:  # noqa: BLE001
            pass
    return 59.0


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
    t = None
    for w in sorted((w for w in worlds if w is not server), key=lambda w: w.get_name()):
        t = local_tank(w)
        if t is not None:
            break
    if t is None:
        out.append("没找到客户端本机车")
    else:
        target = unreal.Vector(SPOT[0], SPOT[1], TOP_Z + half_h_of(t) + 2.0)
        ok = t.set_actor_location_and_rotation(
            target, unreal.Rotator(pitch=0.0, yaw=FACE_YAW, roll=0.0), False, True)
        t.set_editor_property("move_speed", SLOW_SPEED)
        out.append("客户端车 %s → (%.0f,%.0f,%.1f) yaw=%.0f ok=%s speed=%.0f"
                   % (t.get_name(), target.x, target.y, target.z, FACE_YAW, ok, SLOW_SPEED))
        out.append("到东缘 X=2000 还有 %.0fcm" % (2000.0 - SPOT[0]))
