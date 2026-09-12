"""采样 + 截图（坡中侧视目验）。前置：已用 win_key 按住 E 让 Client1 镜头侧摆。"""

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
    cam_yaw = getattr(tank, "camera_relative_yaw", None)
    out.append("[%s] %s loc=(%.1f,%.1f,%.1f) 姿态(p/y/r)=(%.1f/%.1f/%.1f) 盒底Z=%.1f 镜头相对偏航=%.1f"
               % (w.get_name(), tank.get_name(), loc.x, loc.y, loc.z,
                  rot.pitch, rot.yaw, rot.roll, loc.z - half,
                  cam_yaw if cam_yaw is not None else -999.0))

n = tank_shot.shot("ramp_pitch_bug_side.png")
out.append("已请求截图 ramp_pitch_bug_side.png（已持有代理 %d 个）" % n)
