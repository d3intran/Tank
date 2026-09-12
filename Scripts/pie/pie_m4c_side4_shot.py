"""坡中 bug 侧视（更近机位）：主机站 (600,1900) 朝 +Y。"""

import unreal

import tank_shot

out.clear()

VANTAGE = unreal.Vector(600.0, 1900.0, 63.0)
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

n = tank_shot.shot("ramp_pitch_bug_side4.png")
out.append("已请求 ramp_pitch_bug_side4.png（已持有代理 %d 个）" % n)
