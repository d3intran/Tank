"""地形基线：dump 关卡里所有与「可行驶面 / 障碍」相关的 actor 的 bounds。

不需要 PIE（读编辑器世界）。输出的 Z 值作为之后判定「坦克该在哪」的判据：
  Floor 顶面 = 地面高度；road_hd 顶面 = 真正可行驶面；Ramp_* = 坡道；Cover_* = 掩体；Bound_* = 边界台阶
"""

import unreal

out.clear()

GROUPS = ("Floor", "road", "Ramp_", "Cover_", "Bound_", "Wall", "PlayerStart", "Battle")


def group_of(label):
    for g in GROUPS:
        if label.startswith(g):
            return g
    return None


rows = {}
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    lbl = a.get_actor_label()
    g = group_of(lbl)
    if g is None:
        continue
    try:
        o, e = a.get_actor_bounds(False)
    except Exception:  # noqa: BLE001
        continue
    rows.setdefault(g, []).append((lbl, o, e, a))

for g in GROUPS:
    items = rows.get(g, [])
    out.append("=== %s (%d) ===" % (g, len(items)))
    for lbl, o, e, a in sorted(items):
        try:
            rot = a.get_actor_rotation()
            r = "pitch=%6.1f yaw=%6.1f" % (rot.pitch, rot.yaw)
        except Exception:  # noqa: BLE001
            r = "?"
        out.append("  %-24s X %7.1f~%7.1f  Y %7.1f~%7.1f  Z %7.1f~%7.1f  %s"
                   % (lbl, o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z, r))
    out.append("")

# 坡道参数复核：由 actor 变换反推上坡方向与坡长
out.append("=== 坡道几何反推 ===")
for lbl, o, e, a in sorted(rows.get("Ramp_", [])):
    try:
        rot = a.get_actor_rotation()
        sc = a.get_actor_scale3d()
        loc = a.get_actor_location()
    except Exception:  # noqa: BLE001
        continue
    import math
    pitch = math.radians(rot.pitch)
    yaw = math.radians(rot.yaw)
    slope_len = sc.x * 100.0
    width = sc.y * 100.0
    thick = sc.z * 100.0
    rise = slope_len * math.sin(pitch)
    run = slope_len * math.cos(pitch)
    out.append("  %-24s @ (%7.0f,%7.0f,%6.0f) 斜面 %5.0f x %4.0f 厚 %3.0f 坡度 %4.1f° 升 %5.0f 投影 %5.0f 坡宽 %4.0f"
               % (lbl, loc.x, loc.y, loc.z, slope_len, width, thick,
                  math.degrees(pitch), rise, run, width))
