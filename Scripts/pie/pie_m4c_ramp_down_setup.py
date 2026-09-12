"""M4c 下坡实测 · 准备：把 Client 1 本机车放到 SE 掩体顶面 (1500,3000)，朝 -X（下坡方向）。

SE 掩体顶面 X∈[1200,2000]，坡在 X∈[651,1220] 以 40° 降到路面。
车从顶面开下去 → 进入坡面（40° 下坡）→ 到坡底。用来验证：
  - M4b 把 GroundSnapDownDistance 60→140 之后，下坡还像不像自由落体
  - 下坡姿态（pitch 是否跟坡面）
"""

import unreal

out.clear()

SPOT = (1500.0, 3000.0)
FACE_YAW = 180.0     # 朝 -X
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
        out.append("客户端车 %s → (%.0f,%.0f,%.1f) yaw=%.0f ok=%s speed=%.0f（到坡顶边 X=1220 还有 %.0fcm）"
                   % (t.get_name(), target.x, target.y, target.z, FACE_YAW, ok, SLOW_SPEED,
                      SPOT[0] - 1220.0))
