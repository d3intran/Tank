"""坡底疑点定位：为什么 Ramp_SW_Y 的坡底比别的坡高 64cm（坡底悬空 → 一道 44cm 的墙）。

对每道坡的**坡底边中点**做：
  1) 从上向下长探针（Z 800 → -200），打印命中 Z / 法线 / actor 标签（多种取法）
  2) 列出所有 AABB 覆盖该 (x,y) 且顶面 Z 在 [-120, 400] 的 actor（候选"挡路者"）
"""

import math

import unreal

out.clear()

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
acts = unreal.EditorLevelLibrary.get_all_level_actors()


def hit_label(hit):
    for path in ("actor", "hit_actor"):
        try:
            a = getattr(hit, path)
            if a is not None:
                return a.get_actor_label()
        except Exception:  # noqa: BLE001
            pass
    try:
        t = hit.to_tuple()
        for i in (11, 12, 9, 10):
            if i < len(t) and hasattr(t[i], "get_actor_label"):
                return t[i].get_actor_label()
    except Exception:  # noqa: BLE001
        pass
    return "?"


def trace(x, y):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 800.0), unreal.Vector(x, y, -200.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    try:
        return (hit.impact_point.z, hit.impact_normal.z, hit_label(hit))
    except Exception:  # noqa: BLE001
        t = hit.to_tuple()
        return (t[5].z, t[6].z, "?")


# 先看一次 FHitResult 的可用属性，避免再猜
sample = unreal.SystemLibrary.line_trace_single(
    world, unreal.Vector(-1600.0, 3874.0, 800.0), unreal.Vector(-1600.0, 3874.0, -200.0),
    unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
if sample is not None:
    out.append("FHitResult 属性: %s" % ", ".join(n for n in dir(sample) if not n.startswith("_"))[:600])
    try:
        t = sample.to_tuple()
        out.append("tuple len=%d 类型=%s" % (len(t), [type(v).__name__ for v in t]))
    except Exception as exc:  # noqa: BLE001
        out.append("to_tuple 失败: %s" % exc)
out.append("")

for r in sorted([a for a in acts if a.get_actor_label().startswith("Ramp_")],
                key=lambda a: a.get_actor_label()):
    lbl = r.get_actor_label()
    loc = r.get_actor_location()
    t = r.get_actor_forward_vector()
    up = r.get_actor_up_vector()
    slope_len = r.get_actor_scale3d().x * 100.0
    base = unreal.Vector(loc.x + up.x * 30.0 - t.x * slope_len * 0.5,
                         loc.y + up.y * 30.0 - t.y * slope_len * 0.5,
                         loc.z + up.z * 30.0 - t.z * slope_len * 0.5)
    res = trace(base.x, base.y)
    out.append("=== %s 坡底边中点 (%.0f, %.0f, 理论%.1f)" % (lbl, base.x, base.y, base.z))
    if res is None:
        out.append("    长探针：无命中")
    else:
        out.append("    长探针命中 Z=%.1f 法线Z=%.2f <%s>  → 坡底相对地面 %s %.1fcm"
                   % (res[0], res[1], res[2], "悬空" if base.z > res[0] else "埋入", abs(base.z - res[0])))
    for a in acts:
        if a.get_actor_label().startswith("PlayerStart") or a == r:
            continue
        try:
            o, e = a.get_actor_bounds(False)
        except Exception:  # noqa: BLE001
            continue
        if (o.x - e.x) <= base.x <= (o.x + e.x) and (o.y - e.y) <= base.y <= (o.y + e.y):
            top = o.z + e.z
            if -120.0 <= top <= 400.0:
                out.append("      覆盖点候选：%-22s 顶面 Z=%7.1f  底面 Z=%7.1f"
                           % (a.get_actor_label(), top, o.z - e.z))
    out.append("")
