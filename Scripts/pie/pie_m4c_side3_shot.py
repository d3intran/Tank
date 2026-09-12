"""坡中 bug 侧视：主机车站到南侧路面 (300,2200) 朝 +Y 当机位，镜头越过坡侧看 Client 车的水平姿态。
瞬移用 set_actor_location_and_rotation（set_actor_rotation 单独调用在本 pawn 上无效）。"""

import unreal

import tank_shot

out.clear()

VANTAGE = unreal.Vector(300.0, 2200.0, 63.0)
VANTAGE_YAW = 90.0


def local_tank(world):
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                return pc.get_controlled_pawn()
        except Exception:  # noqa: BLE001
            pass
    return None


worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
server = max(worlds, key=lambda w: len(
    unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

for w in worlds:
    tank = local_tank(w)
    if tank is None:
        continue
    if w is server:
        tank.set_actor_location_and_rotation(
            VANTAGE, unreal.Rotator(0.0, VANTAGE_YAW, 0.0), False, True)
        rot = tank.get_actor_rotation()
        out.append("主机机位(%s) loc=(%.0f,%.0f,%.0f) yaw=%.0f"
                   % (tank.get_name(), VANTAGE.x, VANTAGE.y, VANTAGE.z, rot.yaw))
    else:
        loc = tank.get_actor_location()
        rot = tank.get_actor_rotation()
        out.append("目标车(%s) loc=(%.0f,%.0f,%.0f) 姿态(p/y/r)=(%.0f/%.0f/%.0f)"
                   % (tank.get_name(), loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll))

n = tank_shot.shot("ramp_pitch_bug_side3.png")
out.append("已请求截图 ramp_pitch_bug_side3.png（已持有代理 %d 个）" % n)
