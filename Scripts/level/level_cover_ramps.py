"""M4b：给全部 6 块中央/角落掩体各加坡道 —— 能开上去，也能开下来（无坠落伤害）。

掩体（实测，见 probe_layout_check）：
  Cover_Center_L @ (-1600,4600)  Cover_Center_R @ (1600,4600)
  Cover_NW @ (-1600,6200)  Cover_NE @ (1600,6200)
  Cover_SW @ (-1600,3000)  Cover_SE @ (1600,3000)      全部 1400×400×500，顶面 Z=460

⚠️ 两个历史教训（都踩过）：
1. **坡底不能按 Floor 顶面 (Z=-40) 算** —— 场上真正可行驶面是 road_hd / road_hd2（顶面 ≈ Z 2.1），
   按 -40 做坡会让下半截埋进路面、露出 ~42cm 的垂直小坎，坦克上不去。
   → 现在坡底位置先**向下探测实际地面**，再把坡底边下沉 SINK，两端都无坎。
2. **随机面会挡路** —— 三排掩体之间的走廊只有 1000cm。
   → 坡度提到 40°，水平投影压到 ~596，走廊/大道都还留得出通道（坦克缩放后宽仅 175）。
   每块掩体随机一个面（四面等概率），结果存进关卡后固定。

幂等：先删掉所有 Ramp_ 前缀的旧件再生成；末尾复核并保存关卡。
"""

import math
import random

import unreal

out.clear()

COVER_LABELS = ("Cover_Center_L", "Cover_Center_R", "Cover_NW", "Cover_NE", "Cover_SW", "Cover_SE")
SLOPE_DEG = 40.0            # 坡度（TankPawn.MaxClimbSlopeDeg = 45，留 5° 余量）
RAMP_WIDTH = 600.0          # 坡宽（cm）
RAMP_THICK = 60.0           # 坡体厚度（cm）
OVERLAP = 20.0              # 坡顶边伸进掩体的深度（cm）
SINK = 20.0                 # 坡底边下沉深度（cm），消除与地面的接缝小坎
RAMP_PREFIX = "Ramp_"
FACE_DIR = {"+X": (1, 0), "-X": (-1, 0), "+Y": (0, 1), "-Y": (0, -1)}

cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
mat = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
editor_world = None
for sub_name in ("LevelEditorSubsystem", "UnrealEditorSubsystem"):
    sub = unreal.get_editor_subsystem(getattr(unreal, sub_name))
    if sub and hasattr(sub, "get_editor_world"):
        w = sub.get_editor_world()
        if w is not None:
            editor_world = w
            break

out.append("Cube=%s Material=%s 编辑器世界=%s" % (cube is not None, mat is not None,
                                                 editor_world.get_name() if editor_world else None))
if cube is None:
    out.append("!! 加载不到 /Engine/BasicShapes/Cube，中止")
else:
    all_actors = unreal.EditorLevelLibrary.get_all_level_actors()

    old = [a for a in all_actors if a.get_actor_label().startswith(RAMP_PREFIX)]
    for a in old:
        a.destroy_actor()
    out.append("清理旧坡道 %d 个" % len(old))

    covers = {a.get_actor_label(): a for a in all_actors if a.get_actor_label() in COVER_LABELS}
    out.append("找到掩体 %d/%d: %s" % (len(covers), len(COVER_LABELS), sorted(covers)))

    def ground_z_at(x, y):
        """该点向下探测实际可行驶面（road_hd/road_hd2 顶面 ≈2.1，而不是 Floor 的 -40）。"""
        start = unreal.Vector(x, y, 300.0)
        end = unreal.Vector(x, y, -400.0)
        ctx = editor_world if editor_world else unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        hit = unreal.SystemLibrary.line_trace_single(
            ctx, start, end, unreal.TraceTypeQuery.ECC_VISIBILITY,
            False, [], unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return COVER_Z_BASE
        h = hit.to_tuple()
        return h[4].z

    for name in COVER_LABELS:
        cover = covers.get(name)
        if cover is None:
            out.append("!! 找不到 %s，跳过" % name)
            continue

        cl = cover.get_actor_location()
        cs = cover.get_actor_scale3d()
        ext = unreal.Vector(cs.x * 50.0, cs.y * 50.0, cs.z * 50.0)
        cover_top_z = cl.z + ext.z

        face_key = random.choice(tuple(FACE_DIR))
        nx, ny = FACE_DIR[face_key]
        normal = unreal.Vector(nx, ny, 0.0)
        half_ext = ext.x if face_key.endswith("X") else ext.y

        # 坡底位置的地面高度（沿法线向外 run 处）
        run = rise = None
        # 先按「顶面 - 地面」的粗估算 run，再实测地面高度精算一次
        ground0 = ground_z_at(cl.x + normal.x * (half_ext + 300.0), cl.y + normal.y * (half_ext + 300.0))
        rise = cover_top_z - (ground0 - SINK)
        run = rise / math.tan(math.radians(SLOPE_DEG))
        # 用真正的 run 再测一次坡底地面（坡底比粗估更远）
        ground = ground_z_at(cl.x + normal.x * (half_ext - OVERLAP + run),
                             cl.y + normal.y * (half_ext - OVERLAP + run))
        base_z = ground - SINK
        rise = cover_top_z - base_z
        run = rise / math.tan(math.radians(SLOPE_DEG))
        slope_len = rise / math.sin(math.radians(SLOPE_DEG))

        # 顶边贴掩体面并压入 OVERLAP；底边按地面实测高度下沉 SINK
        top_edge = unreal.Vector(cl.x + normal.x * (half_ext - OVERLAP),
                                 cl.y + normal.y * (half_ext - OVERLAP),
                                 cover_top_z)
        base_edge = unreal.Vector(top_edge.x + normal.x * run,
                                  top_edge.y + normal.y * run,
                                  base_z)
        mid = unreal.Vector((top_edge.x + base_edge.x) * 0.5,
                            (top_edge.y + base_edge.y) * 0.5,
                            (top_edge.z + base_edge.z) * 0.5)
        ascent = unreal.Vector(-normal.x * run, -normal.y * run, rise)
        t_len = math.sqrt(ascent.x ** 2 + ascent.y ** 2 + ascent.z ** 2)
        t = unreal.Vector(ascent.x / t_len, ascent.y / t_len, ascent.z / t_len)
        n = unreal.Vector(normal.x * rise / t_len, normal.y * rise / t_len, run / t_len)
        center = unreal.Vector(mid.x - n.x * RAMP_THICK * 0.5,
                               mid.y - n.y * RAMP_THICK * 0.5,
                               mid.z - n.z * RAMP_THICK * 0.5)
        yaw = math.degrees(math.atan2(t.y, t.x))
        pitch = math.degrees(math.asin(max(-1.0, min(1.0, t.z))))
        rot = unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)   # 必须关键字传参（顺序是 roll,pitch,yaw）

        a = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, center, rot)
        if a is None:
            out.append("!! %s 生成失败" % name)
            continue
        a.set_actor_label("%s%s_%s" % (RAMP_PREFIX, name.replace("Cover_", ""), face_key.lstrip("+-")))
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        if smc:
            smc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            smc.set_editor_property("static_mesh", cube)
            if mat:
                smc.set_material(0, mat)
            a.set_actor_scale3d(unreal.Vector(slope_len / 100.0, RAMP_WIDTH / 100.0, RAMP_THICK / 100.0))
            smc.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

        out.append("%-16s 面 %s  坡高 %.0f（地面 %.1f）  投影 %.0f  斜面 %.0f  顶边 Z=%.0f"
                   % (a.get_actor_label(), face_key, rise, ground, run, slope_len, top_edge.z))

    ramps = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
             if a.get_actor_label().startswith(RAMP_PREFIX)]
    out.append("")
    out.append("=== 坡道 %d/%d ===" % (len(ramps), len(COVER_LABELS)))
    for r in sorted(ramps, key=lambda x: x.get_actor_label()):
        l = r.get_actor_location()
        s = r.get_actor_scale3d()
        rot = r.get_actor_rotation()
        deg = math.degrees(math.asin(max(-1.0, min(1.0, math.sin(math.radians(rot.pitch))))))
        out.append("  %-18s @ (%7.0f,%7.0f,%6.0f)  斜面 %5.0f x %4.0f  坡度 %4.1f°"
                   % (r.get_actor_label(), l.x, l.y, l.z, s.x * 100, s.y * 100, deg))
    out.append("")
    try:
        out.append("保存关卡: %s" % unreal.EditorLevelLibrary.save_current_level())
    except Exception as exc:  # noqa: BLE001
        out.append("保存失败: %s" % exc)
