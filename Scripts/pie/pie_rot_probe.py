"""快速探针：主机车 set_actor_rotation / 控制器 set_control_rotation 哪个能让镜头转向 +Y。"""

import unreal

out.clear()

world = None
for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
    pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
    if len(pcs) > 1:
        world = w
        break
if world is None:
    out.append("PIE 未运行")
else:
    pc = None
    host = None
    for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if c.is_local_player_controller() and c.get_controlled_pawn():
                pc = c
                host = c.get_controlled_pawn()
                break
        except Exception:  # noqa: BLE001
            pass
    out.append("before: pawn yaw=%.1f ctrl yaw=%.1f"
               % (host.get_actor_rotation().yaw, pc.get_control_rotation().yaw))
    host.set_actor_rotation(unreal.Rotator(0.0, 90.0, 0.0), False)
    out.append("after set_actor_rotation(90): pawn yaw=%.1f" % host.get_actor_rotation().yaw)
    pc.set_control_rotation(unreal.Rotator(0.0, 90.0, 0.0))
    out.append("after ctrl.set_control_rotation(90): pawn yaw=%.1f ctrl yaw=%.1f"
               % (host.get_actor_rotation().yaw, pc.get_control_rotation().yaw))
    host.set_actor_rotation(unreal.Rotator(0.0, 90.0, 0.0), False)
    out.append("set_actor_rotation again: pawn yaw=%.1f" % host.get_actor_rotation().yaw)
