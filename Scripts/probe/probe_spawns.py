import unreal, math
out.clear()
worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
server = max(worlds, key=lambda w: len(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
out.append("PlayerStart 旋转（设计意图：朝圆心 (0,4600)）")
for a in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerStart):
    l = a.get_actor_location()
    r = a.get_actor_rotation()
    want = math.degrees(math.atan2(4600.0 - l.y, 0.0 - l.x))
    out.append("  %-16s (%7.0f,%7.0f)  p=%7.1f y=%7.1f r=%5.1f   朝圆心应为 y=%6.1f  %s"
               % (a.get_actor_label(), l.x, l.y, r.pitch, r.yaw, r.roll, want,
                  "OK" if abs(((r.yaw - want + 180) % 360) - 180) < 2 else "!! 偏了"))
