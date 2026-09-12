"""侧视目验：把主机车（服务器世界本机车）吊到坡南侧上空 (850,2300,700) 朝 +Y，
镜头从侧面看 Ramp_SE_X 上 Client1 的 bug 姿态车。瞬移后立即截图（坠落 <2cm 可忽略）。"""

import unreal

import tank_shot

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
    host = None
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                host = pc.get_controlled_pawn()
                break
        except Exception:  # noqa: BLE001
            pass
    if host is None:
        out.append("服务器世界没找到本机车")
    else:
        host.set_actor_location(unreal.Vector(850.0, 2300.0, 700.0), False, False)
        host.set_actor_rotation(unreal.Rotator(0.0, 90.0, 0.0), False)
        loc = host.get_actor_location()
        out.append("主机机位 %s loc=(%.0f,%.0f,%.0f) yaw=%.0f"
                   % (host.get_name(), loc.x, loc.y, loc.z, host.get_actor_rotation().yaw))
        for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
            pawn = pc.get_controlled_pawn()
            if pawn and pawn != host:
                pl = pawn.get_actor_location()
                pr = pawn.get_actor_rotation()
                out.append("  他车 %s loc=(%.0f,%.0f,%.0f) 姿态(p/y/r)=(%.0f/%.0f/%.0f)"
                           % (pawn.get_name(), pl.x, pl.y, pl.z, pr.pitch, pr.yaw, pr.roll))

    n = tank_shot.shot("ramp_pitch_bug_side2.png")
    out.append("已请求截图 ramp_pitch_bug_side2.png（已持有代理 %d 个）" % n)
