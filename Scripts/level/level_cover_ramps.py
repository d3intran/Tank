"""M4c：6 块掩体各配一条**固定朝外**的坡道 —— 左列朝西 (-X)、右列朝东 (+X)。

掩体（实测）：Cover_Center_L @ (-1600,4600)、Cover_Center_R @ (1600,4600)、
Cover_NW @ (-1600,6200)、Cover_NE @ (1600,6200)、Cover_SW @ (-1600,3000)、Cover_SE @ (1600,3000)；
800(X)×400(Y)×500(Z) 实心方块，顶面 Z=460（脚本按各 actor 的实际 scale 算，不写死）。

⚠️ 三条历史教训（都踩过）：
1. **方向不能随机**：块与块之间的走廊只有 1200cm，两块坡头对头就把走廊堵死
   （B5：SW 坡与 Center_L 坡在 Y≈3874 顶头，坡底还悬空 43cm）。
   → 现在方向写死朝外：坡底落在场边空地（掩体面外还有 1400+cm），走廊与中路一概不碰。
2. **坡顶不能 OVERLAP**：旧版让坡面伸进掩体 20cm 消缝，坡面在掩体表面处因此比掩体顶低
   20·tan(坡角) ≈ 17cm，变成一道竖坎 —— 车头撞上去卡死在坡顶（G2）。
   → 现在 OVERLAP=0：坡面正好交在「掩体表面 × 顶面」的棱上，接缝齐平（顶边 Z = 掩体顶面 Z）。
3. **坡底不能按 Floor 顶面 (Z=-40) 算**：场上真正可行驶面是 road_hd / road_hd2（顶面 ≈ Z 2.1），
   按 -40 做坡会让下半截埋进路面、露出 ~42cm 的垂直小坎，坦克上不去。
   → 现在沿坡底边**三点向下探真实地面**（**探针 ignore 掉坡与掩体**，否则会量到邻居坡面上，
   见 B5），取**最低**者再下沉 SINK —— 坡底边整条埋进地面，任何一段都不会悬空。

幂等：先删掉所有 Ramp_ 前缀的旧件再生成；末尾复核并保存关卡。
"""

import math

import unreal

out.clear()

# 每块掩体固定一个方向（朝场外）。写死而不是随机：随机面踩过「挡路 + 坡底悬空」两个坑
FACE_BY_COVER = {
    "Cover_Center_L": "-X",
    "Cover_NW": "-X",
    "Cover_SW": "-X",
    "Cover_Center_R": "+X",
    "Cover_NE": "+X",
    "Cover_SE": "+X",
}
SLOPE_DEG = 30.0            # 坡度（TankPawn.MaxClimbSlopeDeg = 45，留 15° 余量）
RAMP_THICK = 60.0           # 坡体厚度（cm，沿坡面法线）
SINK = 20.0                 # 坡底边下沉深度（cm）：埋进地面，与路面接缝无坎
EDGE_INSET = 10.0           # 坡宽每侧比掩体面收进这么多（cm）：坡不宽出掩体的脸
RAMP_PREFIX = "Ramp_"
FACE_DIR = {"+X": (1.0, 0.0), "-X": (-1.0, 0.0), "+Y": (0.0, 1.0), "-Y": (0.0, -1.0)}
FALLBACK_GROUND_Z = -40.0   # 探不到地面时的兜底（Floor 顶面）：宁可埋深也不悬空

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

    covers = {a.get_actor_label(): a for a in all_actors if a.get_actor_label() in FACE_BY_COVER}
    out.append("找到掩体 %d/%d: %s" % (len(covers), len(FACE_BY_COVER), sorted(covers)))

    # 探地面时要 ignore 的：所有坡 + 所有掩体（B5 的根因就是没 ignore，量到了邻居坡面）
    ignore = [a for a in all_actors
              if a.get_actor_label().startswith(RAMP_PREFIX) or a.get_actor_label() in FACE_BY_COVER]

    def ground_z_at(x, y, tag):
        """该点向下探测实际可行驶面（road_hd/road_hd2 顶面 ≈2.1，而不是 Floor 的 -40）。"""
        start = unreal.Vector(x, y, 300.0)
        end = unreal.Vector(x, y, -400.0)
        ctx = editor_world if editor_world else unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        hit = unreal.SystemLibrary.line_trace_single(
            ctx, start, end, unreal.TraceTypeQuery.ECC_VISIBILITY,
            False, ignore, unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            out.append("  !! %s 探不到地面 (%.0f,%.0f)，兜底 %.1f" % (tag, x, y, FALLBACK_GROUND_Z))
            return FALLBACK_GROUND_Z
        return hit.to_tuple()[5].z   # [5]=ImpactPoint（与 probe_ramp_bases_all.py 一致）

    for name, face_key in FACE_BY_COVER.items():
        cover = covers.get(name)
        if cover is None:
            out.append("!! 找不到 %s，跳过" % name)
            continue

        cl = cover.get_actor_location()
        cs = cover.get_actor_scale3d()
        ext = unreal.Vector(cs.x * 50.0, cs.y * 50.0, cs.z * 50.0)
        cover_top_z = cl.z + ext.z

        nx, ny = FACE_DIR[face_key]
        # 沿法线的面深（半）与面宽（半）：X 面用 ext.y 当宽度，Y 面用 ext.x
        face_half = ext.x if face_key.endswith("X") else ext.y
        lateral_half = ext.y if face_key.endswith("X") else ext.x
        ramp_width = max(60.0, 2.0 * lateral_half - 2.0 * EDGE_INSET)
        lat_x, lat_y = -ny, nx

        def base_ground(run_est):
            """坡底边（三点：两端 + 中点）下方的地面，取最低者 —— 保证整条底边都不悬空。"""
            bx = cl.x + nx * (face_half + run_est)
            by = cl.y + ny * (face_half + run_est)
            half = ramp_width * 0.5
            pts = ((bx + lat_x * half, by + lat_y * half),
                   (bx - lat_x * half, by - lat_y * half),
                   (bx, by))
            return min(ground_z_at(px, py, "%s 坡底" % name) for px, py in pts)

        # 迭代求坡底位置：地面高度取决于坡底在哪，坡底位置又取决于地面高度（2~3 轮即收敛）
        run = (cover_top_z - (base_ground(300.0) - SINK)) / math.tan(math.radians(SLOPE_DEG))
        for _ in range(3):
            run = (cover_top_z - (base_ground(run) - SINK)) / math.tan(math.radians(SLOPE_DEG))

        ground = base_ground(run)
        base_z = ground - SINK
        rise = cover_top_z - base_z
        run = rise / math.tan(math.radians(SLOPE_DEG))
        slope_len = rise / math.sin(math.radians(SLOPE_DEG))

        # 顶边正好压在「掩体表面 × 顶面」的棱上（OVERLAP=0）→ 接缝齐平、无台阶
        top_edge = unreal.Vector(cl.x + nx * face_half, cl.y + ny * face_half, cover_top_z)
        base_edge = unreal.Vector(top_edge.x + nx * run, top_edge.y + ny * run, base_z)
        mid = unreal.Vector((top_edge.x + base_edge.x) * 0.5,
                            (top_edge.y + base_edge.y) * 0.5,
                            (top_edge.z + base_edge.z) * 0.5)

        # 坡面朝上的法线：沿法线反推 + 抬升（厚度方向）
        ascent = unreal.Vector(-nx * run, -ny * run, rise)
        t_len = math.sqrt(ascent.x ** 2 + ascent.y ** 2 + ascent.z ** 2)
        t = unreal.Vector(ascent.x / t_len, ascent.y / t_len, ascent.z / t_len)
        n = unreal.Vector(nx * rise / t_len, ny * rise / t_len, run / t_len)
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
            a.set_actor_scale3d(unreal.Vector(slope_len / 100.0, ramp_width / 100.0, RAMP_THICK / 100.0))
            smc.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

        out.append("%-16s 朝 %s  坡高 %.0f（地面 %.1f）  投影 %.0f  斜面 %.0f  宽 %.0f  顶边 Z=%.0f"
                   % (a.get_actor_label(), face_key, rise, ground, run, slope_len, ramp_width, top_edge.z))

    ramps = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
             if a.get_actor_label().startswith(RAMP_PREFIX)]
    out.append("")
    out.append("=== 坡道 %d/%d ===" % (len(ramps), len(FACE_BY_COVER)))
    for r in sorted(ramps, key=lambda x: x.get_actor_label()):
        l = r.get_actor_location()
        s = r.get_actor_scale3d()
        rot = r.get_actor_rotation()
        fwd = r.get_actor_forward_vector()
        up = r.get_actor_up_vector()
        # 坡底端「坡面中心」= 坡心 - 上坡方向*半长 + 坡面法线*半厚（应 ≈ 地面 - SINK）
        base_surface_z = l.z - fwd.z * (s.x * 50.0) + up.z * (s.z * 50.0)
        out.append("  %-18s @ (%7.0f,%7.0f,%6.0f)  斜面 %5.0f x %4.0f  坡度 %4.1f°  坡底坡面 Z≈%.1f"
                   % (r.get_actor_label(), l.x, l.y, l.z, s.x * 100, s.y * 100, rot.pitch, base_surface_z))
    out.append("")
    try:
        out.append("保存关卡: %s" % unreal.EditorLevelLibrary.save_current_level())
    except Exception as exc:  # noqa: BLE001
        out.append("保存失败: %s" % exc)
