"""边界台阶交叉复核 v2：PIE 期间 `get_editor_world()` 返回 None（上一条探针全"无命中"是假的），
改以 server PIE world 为关卡几何来源，客户端 world 为对照。

查：台阶在不在（actor 枚举 + AABB）、有没有碰撞（两通道竖直/水平射线）、车为什么停在 3257.9。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
in_pie = les.is_in_play_in_editor()

if in_pie:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    geom_world, geo_tag = server, "server PIE world"
    client = None
    for w in sorted((w for w in worlds if w is not server), key=lambda w: w.get_name()):
        for pc in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController):
            try:
                if pc.is_local_player_controller():
                    client = w
                    break
            except Exception:  # noqa: BLE001
                pass
        if client is not None:
            break
else:
    geom_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    geo_tag = "editor world"
    client = None

out.append("几何来源=%s (%s)  in_pie=%s" % (geom_world.get_name() if geom_world else None, geo_tag, in_pie))
out.append("")


def trace(world, x, y, ch, horiz_to=None):
    if horiz_to is None:
        a, b = unreal.Vector(x, y, 400.0), unreal.Vector(x, y, -400.0)
    else:
        a, b = unreal.Vector(x, y, 60.0), unreal.Vector(horiz_to, y, 60.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, a, b, ch, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return "无命中"
    t = hit.to_tuple()
    lbl = "?"
    for i in (9, 11, 10, 12):
        if i < len(t) and hasattr(t[i], "get_actor_label"):
            lbl = t[i].get_actor_label()
            break
    return "Z=%.1f <%s>" % (t[5].z, lbl)


if geom_world is not None:
    out.append("— 竖直射线（VISIBILITY）")
    for x in (-600.0, 3069.0, 3257.0, 3448.0):
        out.append("  @(%.0f,3000): %s"
                   % (x, trace(geom_world, x, 3000.0, unreal.TraceTypeQuery.ECC_VISIBILITY)))
    out.append("— 水平射线 2400→3600 @Y3000,Z60（穿台阶）")
    out.append("  VISIBILITY: %s" % trace(geom_world, 2400.0, 3000.0,
                                          unreal.TraceTypeQuery.ECC_VISIBILITY, horiz_to=3600.0))

    acts = unreal.GameplayStatics.get_all_actors_of_class(geom_world, unreal.Actor)
    pref = {}
    for a in acts:
        lb = a.get_actor_label()
        pref[lb.split("_")[0]] = pref.get(lb.split("_")[0], 0) + 1
    out.append("— actor 总数 %d；前缀统计：%s" % (len(acts), pref))
    out.append("  含 Bound/Step/Edge：%s"
               % [a.get_actor_label() for a in acts
                  if any(k in a.get_actor_label() for k in ("Bound", "Step", "Edge"))])

    out.append("— (3257,3000) 邻域 AABB（顶面 Z∈[-120,500]）")
    n = 0
    for a in acts:
        try:
            o, e = a.get_actor_bounds(False)
        except Exception:  # noqa: BLE001
            continue
        if (o.x - e.x) <= 3257.0 <= (o.x + e.x) and (o.y - e.y) <= 3000.0 <= (o.y + e.y):
            top = o.z + e.z
            if -120.0 <= top <= 500.0:
                n += 1
                out.append("      %-24s 顶面%7.1f 底面%7.1f %s"
                           % (a.get_actor_label(), top, o.z - e.z, a.get_class().get_name()))
    out.append("      共 %d 个" % n)
