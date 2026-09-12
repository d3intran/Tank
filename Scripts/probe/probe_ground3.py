import unreal
out.clear()
# 主机出生点 (2125, 5480)，车底实测 Z≈2.2 —— 查这里脚下是什么
px, py, pz = 2125.0, 5480.0, 2.2
out.append("查找在 (%.0f,%.0f) 附近、Z 区间覆盖 %.1f 的静态体：" % (px, py, pz))
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    try:
        o, e = a.get_actor_bounds(False)
    except Exception:
        continue
    if abs(o.x - e.x) > 9000 and abs(o.y - e.y) > 9000:
        pass  # 大地面也列出来
    if (o.x - e.x) <= px <= (o.x + e.x) and (o.y - e.y) <= py <= (o.y + e.y) and (o.z - e.z) <= pz <= (o.z + e.z):
        out.append("  %-26s X %7.0f~%7.0f Y %7.0f~%7.0f Z %6.1f~%6.1f"
                   % (a.get_actor_label(), o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z))
out.append("")
out.append("=== 全部 Z 顶面在 -100~200 之间的 actor（地表面候选）===")
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    try:
        o, e = a.get_actor_bounds(False)
    except Exception:
        continue
    if -100.0 <= (o.z + e.z) <= 200.0:
        out.append("  %-26s 顶面 Z=%6.1f  X %7.0f~%7.0f Y %7.0f~%7.0f"
                   % (a.get_actor_label(), o.z + e.z, o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y))
