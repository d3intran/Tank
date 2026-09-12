"""一次性检查：坡道与道路/掩体的平面重叠（验证「随机坡面挡路」的具体情况）。"""

import unreal

out.clear()
acts = unreal.EditorLevelLibrary.get_all_level_actors()
groups = {"Road": [], "Ramp": [], "Cover": [], "Wall": [], "Floor": []}
for a in acts:
    lbl = a.get_actor_label()
    for k in groups:
        if lbl.startswith(k) or k in lbl:
            o, e = a.get_actor_bounds(False)
            groups[k].append((lbl, o, e))
            break

for k, items in groups.items():
    out.append("=== %s (%d) ===" % (k, len(items)))
    for lbl, o, e in sorted(items):
        out.append("  %-20s X %7.0f~%7.0f  Y %7.0f~%7.0f  Z %6.0f~%6.0f"
                   % (lbl, o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z))
    out.append("")

out.append("=== 坡道与道路/掩体的平面重叠 ===")
for rl, ro, re in groups["Ramp"]:
    for other, oo, oe in groups["Road"] + groups["Cover"]:
        if other == rl:
            continue
        x_ov = min(ro.x + re.x, oo.x + oe.x) - max(ro.x - re.x, oo.x - oe.x)
        y_ov = min(ro.y + re.y, oo.y + oe.y) - max(ro.y - re.y, oo.y - oe.y)
        if x_ov > 0 and y_ov > 0:
            out.append("  %s × %s : 重叠 X %.0f × Y %.0f" % (rl, other, x_ov, y_ov))
