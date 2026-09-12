"""关卡侦察：出生点分布 + 主要静态几何 + 场地范围，为 M4「地图 FFA 化」提供依据。"""

import unreal

out.clear()

ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.EditorLevelLibrary.get_all_level_actors()
out.append("编辑器世界: %s，Actor 总数 = %d" % (ws.get_name(), len(actors)))

# ---- PlayerStart 分布 ----
starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
out.append("")
out.append("=== PlayerStart 共 %d 个 ===" % len(starts))
pts = []
for s in starts:
    loc = s.get_actor_location()
    rot = s.get_actor_rotation()
    pts.append((loc, rot.yaw))
    out.append("  %-16s loc=(%8.0f,%8.0f,%8.0f) yaw=%6.1f"
               % (s.get_name(), loc.x, loc.y, loc.z, rot.yaw))

if pts:
    xs = [p[0].x for p in pts]
    ys = [p[0].y for p in pts]
    zs = [p[0].z for p in pts]
    out.append("  X 范围 %.0f ~ %.0f    Y 范围 %.0f ~ %.0f    Z 范围 %.0f ~ %.0f"
               % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
    cx, cy = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
    out.append("  几何中心 ≈ (%.0f, %.0f)" % (cx, cy))
    out.append("")
    out.append("  相对中心的极坐标（看是否环形均布）:")
    import math
    for s, (loc, yaw) in zip(starts, pts):
        dx, dy = loc.x - cx, loc.y - cy
        r = math.hypot(dx, dy)
        a = math.degrees(math.atan2(dy, dx))
        out.append("    %-16s r=%7.0f  angle=%7.1f" % (s.get_name(), r, a))

# ---- 主要几何统计 ----
out.append("")
out.append("=== 静态网格 Actor（按 mesh 名归类，取前 15）===")
counts = {}
for a in actors:
    if isinstance(a, unreal.StaticMeshActor):
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        name = "<none>"
        if smc:
            m = smc.get_editor_property("static_mesh")
            if m:
                name = m.get_name()
        counts[name] = counts.get(name, 0) + 1
for k, v in sorted(counts.items(), key=lambda kv: -kv[1])[:15]:
    out.append("  %-46s x%d" % (k, v))

out.append("")
out.append("=== 其他类型 Actor 统计 ===")
kinds = {}
for a in actors:
    n = a.get_class().get_name()
    kinds[n] = kinds.get(n, 0) + 1
for k, v in sorted(kinds.items(), key=lambda kv: -kv[1])[:15]:
    out.append("  %-40s x%d" % (k, v))
