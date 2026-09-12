"""修正中央掩体的通路宽度。

问题：初版中央矮墙 3 块（X=-1400/0/1400，各宽 1200）只留 200cm 缝，
而坦克宽 350cm —— 那两道"缝"根本过不去，视觉上像通路实际是堵死的，属误导性设计。

改为 2 块、留一条 1800cm 的中央大道 + 两侧各约 800cm 的迂回通道：
  Cover_Center_L @ (-1600, 4600) 1400 x 400 x 500  → 占 X -2300~-900
  Cover_Center_R @ ( 1600, 4600) 1400 x 400 x 500  → 占 X  900~2300
  中央通路 X -900~900（1800cm）；两翼 X 2300~3100 与 -3100~-2300（各 800cm）
"""

import unreal

out.clear()

actors = unreal.EditorLevelLibrary.get_all_level_actors()

# 1) 删掉中间那块
for a in actors:
    if a.get_actor_label() == "Cover_Center_M":
        a.destroy_actor()
        out.append("已删除 Cover_Center_M")

# 2) 重排左右两块
plan = {"Cover_Center_L": (-1600.0, 1400.0), "Cover_Center_R": (1600.0, 1400.0)}
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    lbl = a.get_actor_label()
    if lbl in plan:
        x, sx = plan[lbl]
        sc = a.get_actor_scale3d()
        a.set_actor_location(unreal.Vector(x, 4600.0, -40.0 + sc.z * 100.0 * 0.5), False, False)
        a.set_actor_scale3d(unreal.Vector(sx / 100.0, sc.y, sc.z))
        out.append("%s -> X=%.0f 宽=%.0f" % (lbl, x, sx))

# 3) 复核通路
covers = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
          if a.get_actor_label().startswith("Cover_")]
out.append("")
out.append("=== 当前掩体 ===")
for c in sorted(covers, key=lambda a: a.get_actor_label()):
    l = c.get_actor_location()
    sc = c.get_actor_scale3d()
    out.append("  %-18s (%7.0f,%7.0f) 尺寸 %4.0f x %4.0f"
               % (c.get_actor_label(), l.x, l.y, sc.x * 100, sc.y * 100))

# 中央那排（Y≈4600）按 X 排序，算相邻间隙
row = sorted([c for c in covers if abs(c.get_actor_location().y - 4600.0) < 1.0],
             key=lambda a: a.get_actor_location().x)
edges = []
for c in row:
    l = c.get_actor_location()
    hw = c.get_actor_scale3d().x * 100.0 * 0.5
    edges.append((l.x - hw, l.x + hw))
out.append("")
out.append("=== 中央矮墙的通行间隙（坦克宽 350，需 > 400）===")
prev_right = -3100.0  # 场地西边界
for i, (lo, hi) in enumerate(edges):
    gap = lo - prev_right
    out.append("  X %7.0f ~ %7.0f : 间隙 %6.0f cm  %s"
               % (prev_right, lo, gap, "可通行" if gap > 400 else "!! 过不去"))
    prev_right = hi
gap = 3100.0 - prev_right
out.append("  X %7.0f ~ %7.0f : 间隙 %6.0f cm  %s"
           % (prev_right, 3100.0, gap, "可通行" if gap > 400 else "!! 过不去"))

out.append("")
try:
    out.append("保存关卡: %s" % unreal.EditorLevelLibrary.save_current_level())
except Exception as exc:  # noqa: BLE001
    out.append("保存失败: %s" % exc)
