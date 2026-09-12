import unreal
out.clear()
worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
server = max(worlds, key=lambda w: len(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

for a in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.Actor):
    lbl = a.get_actor_label()
    if lbl.startswith("road"):
        o, e = a.get_actor_bounds(False)
        out.append("%-10s X %7.0f~%7.0f  Y %7.0f~%7.0f  Z %6.1f~%6.1f"
                   % (lbl, o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z))

tank = None
for pc in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController):
    try:
        if pc.is_local_player_controller() and pc.get_controlled_pawn():
            tank = pc.get_controlled_pawn()
            break
    except Exception:
        pass
loc = tank.get_actor_location()
out.append("")
out.append("主机本机车 %s loc=(%.1f,%.1f,%.1f)  盒底 Z=%.1f" % (tank.get_name(), loc.x, loc.y, loc.z, loc.z - 59.0))

start = unreal.Vector(loc.x, loc.y, loc.z - 49.0)
end = unreal.Vector(loc.x, loc.y, loc.z - 119.0)
hit = unreal.SystemLibrary.line_trace_single(server, start, end,
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [tank], unreal.DrawDebugTrace.NONE, True)
if hit is None:
    out.append("Visibility 探测：无命中")
else:
    hl = hit.to_tuple()
    h_loc, h_norm = hl[4], hl[5]
    actor = hl[11] if len(hl) > 11 else None
    out.append("Visibility 探测：命中 Z=%.1f 法线=(%.2f,%.2f,%.2f) Actor=%s"
               % (h_loc.z, h_norm.x, h_norm.y, h_norm.z, actor.get_name() if actor else "None"))
