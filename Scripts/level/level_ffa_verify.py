"""复核 FFA 化结果：出生环均布性 + 掩体清单，并把编辑器视口摆到俯视图后截图。"""

import math
import unreal
import tank_shot  # 持有式截图

out.clear()

ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.EditorLevelLibrary.get_all_level_actors()

starts = sorted([a for a in actors if isinstance(a, unreal.PlayerStart)], key=lambda a: a.get_name())
out.append("=== PlayerStart %d 个 ===" % len(starts))

pts = []
for s in starts:
    l = s.get_actor_location()
    pts.append((s.get_name(), l))

# 均布性检查：以 8 点质心为圆心，比较各点半径与相邻夹角
if len(pts) >= 2:
    cx = sum(p[1].x for p in pts) / len(pts)
    cy = sum(p[1].y for p in pts) / len(pts)
    out.append("  质心 ≈ (%.0f, %.0f)" % (cx, cy))
    info = []
    for name, l in pts:
        dx, dy = l.x - cx, l.y - cy
        r = math.hypot(dx, dy)
        ang = (math.degrees(math.atan2(dy, dx)) + 360.0) % 360.0
        info.append((name, l, r, ang))
        out.append("  %-16s (%7.0f,%7.0f)  r=%7.1f  ang=%6.1f" % (name, l.x, l.y, r, ang))

    rs = [i[2] for i in info]
    out.append("  半径 min=%.1f max=%.1f（差 %.1f，越小越均匀）" % (min(rs), max(rs), max(rs) - min(rs)))

    angs = sorted(i[3] for i in info)
    gaps = [angs[(k + 1) % len(angs)] - angs[k] for k in range(len(angs) - 1)]
    gaps.append(360.0 - angs[-1] + angs[0])
    out.append("  相邻夹角: %s" % ", ".join("%.1f" % g for g in gaps))
    out.append("  夹角 min=%.1f max=%.1f（8 点均布应为 45）" % (min(gaps), max(gaps)))

# 掩体
covers = [a for a in actors if a.get_actor_label().startswith("Cover_")]
out.append("")
out.append("=== 掩体 %d 块 ===" % len(covers))
for c in sorted(covers, key=lambda a: a.get_actor_label()):
    l = c.get_actor_location()
    sc = c.get_actor_scale3d()
    out.append("  %-18s (%7.0f,%7.0f,%7.0f)  尺寸 %4.0f x %4.0f x %4.0f"
               % (c.get_actor_label(), l.x, l.y, l.z, sc.x * 100, sc.y * 100, sc.z * 100))

out.append("")
out.append("=== 出生点与掩体的最近距离（应 > 车长 760）===")
for name, l in pts:
    d = min(math.hypot(l.x - c.get_actor_location().x, l.y - c.get_actor_location().y) for c in covers) if covers else -1
    out.append("  %-16s 最近掩体 %.0f cm" % (name, d))

# ---- 俯视图截图 ----
out.append("")
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    key = les.get_active_viewport_config_key()
    out.append("活动视口 key = %s" % key)
    cam_loc = unreal.Vector(0.0, 4600.0, 11000.0)
    cam_rot = unreal.Rotator(-89.0, -90.0, 0.0)  # 垂直向下
    les.set_level_viewport_camera_info(cam_loc, cam_rot, key)
    n = tank_shot.shot("level_topdown.png")
    out.append("编辑器视口已摆到俯视图并请求截图（持有代理数=%d）" % n)
except Exception as exc:  # noqa: BLE001
    out.append("俯视图截图失败: %s" % exc)
