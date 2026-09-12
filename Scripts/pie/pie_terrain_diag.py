"""地形跟随诊断：把主机本机车当前姿态下「C++ 那段贴地逻辑应该看到什么」完整打出来。

对照 TankPawn::UpdateGroundContact：
  A. C++ 探针复现：起点 = 车心 - (半高-10)，向下 10 + GroundSnapDownDistance(140)
     → 是否命中 / 命中 Z / 法线 Z / 是否满足 WalkableCos(cos45=0.707)
     = 「C++ 认为车踩在地面上了吗」
  B. 四个盒底角点（按当前旋转算出的世界坐标）正下方长探针：
     gap = 地面Z - 角点Z，正 = 悬空、负 = 穿模
  C. 由四角高度反推理想 pitch/roll 与「抬到零穿模所需上移量」

判据：A 未命中而 B 显示大面积悬空 → 「单点中心探测够不到坡面」；
      A 命中但 B 全部为负 → 「用未旋转半高贴地 ⇒ 整车下沉」。
"""

import math

import unreal

out.clear()

HALF_H = 59.0        # 整车 1/2 后的碰撞盒半高（读不到时的兜底）
SNAP_DOWN = 140.0    # TankPawn::GroundSnapDownDistance
MAX_SLOPE = 45.0     # TankPawn::MaxClimbSlopeDeg


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.is_in_play_in_editor():
        out.append("PIE 未运行")
        return

    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

    tank = None
    for pc in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                tank = pc.get_controlled_pawn()
                break
        except Exception:  # noqa: BLE001
            pass
    if tank is None:
        out.append("没找到主机本机车")
        return

    loc = tank.get_actor_location()
    rot = tank.get_actor_rotation()
    fwd = tank.get_actor_forward_vector()
    right = tank.get_actor_right_vector()
    up = tank.get_actor_up_vector()
    box = getattr(tank, "collision_box", None)

    half = HALF_H
    hx, hy = 190.0, 87.5
    if box is not None:
        try:
            se = box.get_scaled_box_extent()
            half, hx, hy = se.z, se.x, se.y
        except Exception:  # noqa: BLE001
            pass

    def offset(ax, ay, az):
        """按车体轴（前/右/上）取世界点。"""
        return unreal.Vector(loc.x + fwd.x * ax + right.x * ay + up.x * az,
                             loc.y + fwd.y * ax + right.y * ay + up.y * az,
                             loc.z + fwd.z * ax + right.z * ay + up.z * az)

    def drop(p, down_from=300.0, down_len=900.0):
        """从 p 上方 down_from 处向下打 down_len，返回 (世界Z, 法线Z, actor标签) 或 None。"""
        start = unreal.Vector(p.x, p.y, p.z + down_from)
        end = unreal.Vector(p.x, p.y, p.z - down_len)
        hit = unreal.SystemLibrary.line_trace_single(
            server, start, end, unreal.TraceTypeQuery.ECC_VISIBILITY,
            False, [tank], unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        z = nz = None
        actor = "?"
        try:
            z = hit.impact_point.z
            nz = hit.impact_normal.z
        except Exception:  # noqa: BLE001
            t = hit.to_tuple()
            for i, v in enumerate(t):
                if hasattr(v, "z"):
                    if z is None:
                        z = v.z
            if len(t) > 6 and hasattr(t[6], "z"):
                nz = t[6].z
        try:
            actor = hit.to_tuple()[11].get_actor_label()
        except Exception:  # noqa: BLE001
            pass
        return (z, nz, actor, hit)

    out.append("%s loc=(%.1f,%.1f,%.1f) 姿态(p/y/r)=(%.1f/%.1f/%.1f) 半高=%.1f 盒底Z=%.1f"
               % (tank.get_name(), loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll,
                  half, loc.z - half))

    raw = drop(offset(0, 0, 0), 100.0, 1400.0)
    if raw is not None:
        out.append("   车心正下方(长探针 1400)：地面 Z=%.1f 法线Z=%.2f <%s>" % (raw[0], raw[1], raw[2]))
        out.append("                            盒底 Z=%.1f → 间隙 %.1fcm"
                   % (loc.z - half, raw[0] - (loc.z - half)))

    walk_cos = math.cos(math.radians(MAX_SLOPE))

    # ---- A. 复现 C++ 探针 ----
    ps = unreal.Vector(loc.x, loc.y, loc.z - (half - 10.0))
    pe = unreal.Vector(loc.x, loc.y, ps.z - (10.0 + SNAP_DOWN))
    hit = unreal.SystemLibrary.line_trace_single(
        server, ps, pe, unreal.TraceTypeQuery.ECC_VISIBILITY,
        False, [tank], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        out.append("A. C++ 探针(%.1f→%.1f)：**未命中** → C++ 判定「悬空」→ 走自由落体分支"
                   % (ps.z, pe.z))
    else:
        z = nz = None
        try:
            z, nz = hit.impact_point.z, hit.impact_normal.z
        except Exception:  # noqa: BLE001
            t = hit.to_tuple()
            z = t[5].z if len(t) > 5 else None
            nz = t[6].z if len(t) > 6 else None
        out.append("A. C++ 探针(%.1f→%.1f)：命中 Z=%.1f 法线Z=%.2f 可行走=%s → C++ 会把盒底摆到 %.1f（Δ=%.1f）"
                   % (ps.z, pe.z, z, nz, nz >= walk_cos, z, z - (loc.z - half)))

    # ---- B. 四角 ----
    corners = [("左前", 1, -1), ("右前", 1, 1), ("左后", -1, -1), ("右后", -1, 1)]
    gaps = {}
    for name, fx, fy in corners:
        c = offset(fx * hx, fy * hy, -half)
        res = drop(c)
        if res is None:
            out.append("B. %s角 (%.0f,%.0f,%.0f)：900cm 内无地面 → 完全悬空" % (name, c.x, c.y, c.z))
            continue
        gz, gnz, actor = res[0], res[1], res[2]
        gap = gz - c.z
        gaps[name] = gap
        out.append("B. %s角 Z=%7.1f  地面 Z=%7.1f  %s %6.1fcm  法线Z=%5.2f  <%s>"
                   % (name, c.z, gz, "悬空" if gap > 0 else "穿模", abs(gap), gnz, actor))

    if gaps:
        vals = list(gaps.values())
        out.append("B. 汇总：最大悬空 %.1fcm / 最大穿模 %.1fcm" % (max(vals), -min(vals)))
        if min(vals) < 0:
            out.append("   → 零穿模需整车再抬 %.1fcm（当前整车正插在地面里）" % (-min(vals)))

    # ---- C. 反推理想姿态 ----
    if len(gaps) == 4:
        fl, fr, bl, br = gaps["左前"], gaps["右前"], gaps["左后"], gaps["右后"]
        # 用「角点高度 + gap」= 地面高度 反推理想 pitch/roll
        zfl, zfr = gaps["左前"] * 0 + (offset(hx, -hy, -half).z), offset(hx, hy, -half).z
        zbl, zbr = offset(-hx, -hy, -half).z, offset(-hx, hy, -half).z
        gfl, gfr, gbl, gbr = zfl + fl, zfr + fr, zbl + bl, zbr + br
        pitch_ideal = math.degrees(math.atan2(((gfl + gfr) * 0.5 - (gbl + gbr) * 0.5), 2.0 * hx))
        roll_ideal = math.degrees(math.atan2(((gfr + gbr) * 0.5 - (gfl + gbl) * 0.5), 2.0 * hy))
        out.append("C. 四角地面 Z：左前%.1f 右前%.1f 左后%.1f 右后%.1f" % (gfl, gfr, gbl, gbr))
        out.append("   反推理想姿态：pitch=%+.1f roll=%+.1f（当前 pitch=%+.1f roll=%+.1f）"
                   % (pitch_ideal, roll_ideal, rot.pitch, rot.roll))


main()
