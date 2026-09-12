"""把各本机车的镜头摆到车身正侧面（camera_relative_yaw=90），再截一张侧视图。

正后方视角看不出「水平 vs 40° 坡」的差别；侧视时坡面斜线是强参照，
水平车身的 bug 一眼可辨（正确姿态应整车与坡面平行）。
"""

import unreal

import tank_shot

out.clear()


def local_tank(world):
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                return pc.get_controlled_pawn()
        except Exception:  # noqa: BLE001
            pass
    return None


for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
    tank = local_tank(w)
    if tank is None:
        continue
    loc = tank.get_actor_location()
    rot = tank.get_actor_rotation()
    box = getattr(tank, "collision_box", None)
    half = box.get_scaled_box_extent().z if box else 59.0
    try:
        tank.set_editor_property("camera_relative_yaw", 90.0)
        cam = "镜头侧摆=90 成功"
    except Exception as exc:  # noqa: BLE001
        cam = "镜头侧摆失败: %s" % exc
    out.append("[%s] %s loc=(%.1f,%.1f,%.1f) 姿态(p/y/r)=(%.1f/%.1f/%.1f) 盒底Z=%.1f | %s"
               % (w.get_name(), tank.get_name(), loc.x, loc.y, loc.z,
                  rot.pitch, rot.yaw, rot.roll, loc.z - half, cam))

n = tank_shot.shot("ramp_pitch_bug_side.png")
out.append("已请求截图 ramp_pitch_bug_side.png（已持有代理 %d 个）" % n)
