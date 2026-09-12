"""报告 PIE 是否在运行（用于快速轮询存活时长）。"""

import unreal

out.clear()
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
alive = les.is_in_play_in_editor()
out.append("in_pie=%s" % alive)
if alive:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = None
    for w in worlds:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            server = w
            break
    if server is not None:
        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)
        orphans = [t for t in tanks if t.get_controller() is None]
        out.append("server tanks=%d orphans=%d" % (len(tanks), len(orphans)))
        for t in tanks:
            c = t.get_controller()
            loc = t.get_actor_location()
            out.append("  %s (%.0f,%.0f) ctrl=%s" % (t.get_name(), loc.x, loc.y,
                                                     c.get_name() if c else "NONE"))
