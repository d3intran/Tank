"""M4c-3：把 6 块掩体的高度缩到原来的 2/3（总高 500 → 333.3，顶面 460 → 293.3），
底面仍压在地板 Z=-40 上。

为什么缩：掩体顶 460 太高 —— 从顶上打地面坦克是死角（俯角 -5° 只能打到 34m 外），
落差大也让"上顶 → 下车"体验很差。缩到 2/3 后顶面 293，配合拓宽到 -18° 的俯角好用得多。

⚠️ 立方体是**中心锚点**：只改 Z 缩放而不改中心 Z，底面会离开地板浮起来。
   顶 = 中心 + 半高、底 = 中心 − 半高，所以缩完必须重算中心：中心 = 地板顶面 + 新半高。

幂等：目标值是**绝对值**（不是"再缩 2/3"），重复执行不会继续变矮。
缩完必须重跑 level_cover_ramps.py 重生成坡道（坡高/坡长自动跟着变）。

用法：编辑器关闭 PIE 后
  `deno run -A Scripts/editor.deno.ts execfile Scripts/level/level_cover_shorter.py`
"""

import unreal

out.clear()

FRACTION = 2.0 / 3.0          # 目标高度比例
ORIG_TOTAL = 500.0            # 掩体原始总高（scale z=5 × 100）
FLOOR_TOP_Z = -40.0           # 地板顶面：掩体底面压在这里
COVER_PREFIX = "Cover_"

target_total = ORIG_TOTAL * FRACTION              # 333.33
target_half = target_total * 0.5                  # 166.67
target_scale_z = target_total / 100.0             # 立方体 100 单位 → 3.3333
target_center_z = FLOOR_TOP_Z + target_half       # 126.67

covers = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
          if a.get_actor_label().startswith(COVER_PREFIX)]
out.append("找到掩体 %d 块；目标：总高 %.1f，缩放 Z=%.4f，中心 Z=%.2f" % (
    len(covers), target_total, target_scale_z, target_center_z))

for a in sorted(covers, key=lambda x: x.get_actor_label()):
    lbl = a.get_actor_label()
    loc = a.get_actor_location()
    sc = a.get_actor_scale3d()
    o, e = a.get_actor_bounds(False)
    old_top, old_bottom = o.z + e.z, o.z - e.z
    a.set_actor_scale3d(unreal.Vector(sc.x, sc.y, target_scale_z))
    a.set_actor_location(unreal.Vector(loc.x, loc.y, target_center_z), False, False)
    o2, e2 = a.get_actor_bounds(False)
    out.append("  %-16s scale Z %.3f→%.3f  loc Z %.1f→%.1f  bounds Z [%.1f,%.1f]→[%.1f,%.1f]"
               % (lbl, sc.z, target_scale_z, loc.z, target_center_z,
                  old_bottom, old_top, o2.z - e2.z, o2.z + e2.z))

out.append("")
try:
    out.append("保存关卡: %s" % unreal.EditorLevelLibrary.save_current_level())
except Exception as exc:  # noqa: BLE001
    out.append("保存失败: %s" % exc)
out.append("→ 接着必须重跑 Scripts/level/level_cover_ramps.py 重生成 6 条坡道（坡顶要重新对上 293.3）")
