import unreal
out.clear()
out.append("SystemLibrary trace 方法: %s" % ", ".join(n for n in dir(unreal.SystemLibrary) if "trace" in n.lower()))
worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
out.append("PIE 世界数 = %d" % len(worlds))
if worlds:
    w = worlds[0]
    acts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
    out.append("PIE 世界 actor 数 = %d" % len(acts))
    labels = sorted(a.get_actor_label() for a in acts)
    out.append("标签: %s" % ", ".join(labels))
