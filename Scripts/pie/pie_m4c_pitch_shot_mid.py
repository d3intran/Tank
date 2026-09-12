"""目验截图（坡中）：把主机车与本机客户端车都摆到「水平姿态骑在 40° 坡上」的 bug 姿态。

bug 姿态的构造依据（见本会话采样）：
  盒底 = 坡面在**车头前缘**处的高度（水平盒子的前缘刚好触坡点）
  → X=850 时坡面在 X=1040 处 = 309.0，故 loc.z = 309.0 + 59 = 368。
  随后 C++ 的中心探针够不到脚下坡面（差 159cm > 140 窗口）→ 不修正姿态，
  自由落体 sweep 又被坡面前缘接触挡住 → 车就保持「水平横躺」悬在坡上：
  前缘贴坡、车尾悬空（最高 320cm）。

主机车直接瞬移即可（服务器本机权威）；客户端车在各自 PIE 世界里也瞬移一份，
这样无论截图命中哪个视口都能拍到。
"""

import unreal

out.clear()

MID = unreal.Vector(850.0, 3000.0, 368.0)
YAW = 0.0


def local_tank(world):
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                return pc.get_controlled_pawn()
        except Exception:  # noqa: BLE001
            pass
    return None


def pose(tank):
    loc = tank.get_actor_location()
    rot = tank.get_actor_rotation()
    box = getattr(tank, "collision_box", None)
    half = box.get_scaled_box_extent().z if box else 59.0
    return "%s loc=(%.1f,%.1f,%.1f) 姿态(p/y/r)=(%.1f/%.1f/%.1f) 盒底Z=%.1f" % (
        tank.get_name(), loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll, loc.z - half)


worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
for w in worlds:
    tank = local_tank(w)
    if tank is None:
        continue
    tank.set_actor_location(MID, False, False)
    tank.set_actor_rotation(unreal.Rotator(0.0, YAW, 0.0), False)
    out.append("[%s] %s" % (w.get_name(), pose(tank)))

out.append("已把各本机车摆到坡中 bug 姿态，下一步截图")
