"""采样当前姿态 + 截图（坡中 bug 目验）。截图异步落盘，稍后再去 Saved/Screenshots 找。"""

import unreal

import tank_shot  # 持有式截图，见 Content/Python/tank_shot.py

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
    out.append("[%s] %s loc=(%.1f,%.1f,%.1f) 姿态(p/y/r)=(%.1f/%.1f/%.1f) 盒底Z=%.1f"
               % (w.get_name(), tank.get_name(), loc.x, loc.y, loc.z,
                  rot.pitch, rot.yaw, rot.roll, loc.z - half))

n = tank_shot.shot("ramp_pitch_bug.png")
out.append("已请求截图 ramp_pitch_bug.png（已持有代理 %d 个）" % n)
