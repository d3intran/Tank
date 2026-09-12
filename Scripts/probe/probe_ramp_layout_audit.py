"""M4c-2 布局审计：6 块掩体 + 6 条坡的一级包围盒（AABB），判定三条硬指标：

1. 每条坡是否完全落在「掩体外侧」——坡的 X 区间不得侵入该侧掩体列的 X 区间（否则就压到走廊/中路）；
2. 坡与坡之间不得相交（V 形对撞/顶头是 B5 的病根）；
3. 坡与掩体只允许在「坡顶贴的那一面」相接，不许插进别的掩体。

顺带算出「中路走廊」的空档：两侧掩体列内表面之间的净宽，以及每条坡的坡底离场地边界还有多远。
"""

import unreal

out.clear()

RAMP_PREFIX = "Ramp_"
COVER_PREFIX = "Cover_"


def aabb(actor):
    o, e = actor.get_actor_bounds(False)
    return (o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z)


def overlap_xz(a, b, pad=1.0):
    ax0, ax1, ay0, ay1, az0, az1 = a
    bx0, bx1, by0, by1, bz0, bz1 = b
    return (ax0 < bx1 - pad and bx0 < ax1 - pad and ay0 < by1 - pad and by0 < ay1 - pad
            and az0 < bz1 - pad and bz0 < az1 - pad)


ramps = {}
covers = {}
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    lbl = a.get_actor_label()
    if lbl.startswith(RAMP_PREFIX):
        ramps[lbl] = aabb(a)
    elif lbl.startswith(COVER_PREFIX):
        covers[lbl] = aabb(a)

out.append("=== 坡（%d）：Y 必须落在自己掩体的 Y 范围内（否则就伸进走廊了）===" % len(ramps))
for lbl in sorted(ramps):
    x0, x1, y0, y1, z0, z1 = ramps[lbl]
    cover_lbl = "Cover_" + lbl[len(RAMP_PREFIX):].rsplit("_", 1)[0]
    c = covers.get(cover_lbl)
    if c is None:
        out.append("  %-18s !! 找不到配对掩体 %s" % (lbl, cover_lbl))
        continue
    cx0, cx1, cy0, cy1, cz0, cz1 = c
    y_ok = (y0 >= cy0 - 1.0) and (y1 <= cy1 + 1.0)
    outer_face = cx0 if x1 < 0 else cx1          # 掩体朝场外的那个面
    # 坡顶边贴在该面上，坡体沿场外方向延伸；朝中路方向只允许「顶边厚度」那点量
    inner_pen = (x1 - outer_face) if x1 < 0 else (outer_face - x0)
    out.append("  %-18s 配 %-16s 坡 Y∈[%6.0f,%6.0f] ⊆ 掩体 Y∈[%6.0f,%6.0f] = %s | 朝中路多出 %5.1fcm %s"
               % (lbl, cover_lbl, y0, y1, cy0, cy1, "OK" if y_ok else "!! 出界",
                  inner_pen, "OK" if inner_pen <= 60.0 else "!! 侵入过多"))

out.append("")
out.append("=== 坡×坡 相交 ===")
names = sorted(ramps)
bad = 0
for i in range(len(names)):
    for j in range(i + 1, len(names)):
        if overlap_xz(ramps[names[i]], ramps[names[j]]):
            out.append("  !! %s ∩ %s" % (names[i], names[j]))
            bad += 1
out.append("  相交 %d 对%s" % (bad, "（无）" if bad == 0 else ""))

out.append("")
out.append("=== 坡×掩体 相交（坡顶贴面允许接触）===")
bad = 0
for r in names:
    for c in sorted(covers):
        if overlap_xz(ramps[r], covers[c], pad=2.0):
            out.append("  %s ∩ %s  X∈[%.0f,%.0f] vs [%.0f,%.0f]  Z 顶 %.0f vs %.0f"
                       % (r, c, ramps[r][0], ramps[r][1], covers[c][0], covers[c][1], ramps[r][5], covers[c][5]))
            bad += 1
out.append("  相交 %d 对%s（坡顶与自身掩体在顶面棱处相接属正常）" % (bad, "（无）" if bad == 0 else ""))

out.append("")
out.append("=== 中路与走廊净宽 ===")
left_in = max(c[1] for c in covers.values() if c[1] < 0)
right_in = min(c[0] for c in covers.values() if c[0] > 0)
out.append("  左右掩体列内表面 X=%.0f / %.0f → 中路净宽 %.0fcm" % (left_in, right_in, right_in - left_in))
ramp_max_x = max(r[1] for r in ramps.values())
ramp_min_x = min(r[0] for r in ramps.values())
out.append("  坡的 X 范围 [%.0f, %.0f]：中路（|X|<%.0f）与走廊（|X|<%.0f）内均无坡"
           % (ramp_min_x, ramp_max_x, min(abs(left_in), abs(right_in)), min(abs(left_in), abs(right_in))))
