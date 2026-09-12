"""M4c 爬坡实测 · 采样器：Client 1 本机车的「C++ 贴地逻辑看到了什么」。

与 pie_terrain_diag.py 同构，但目标换成客户端世界里的本机车（投键驱动的那辆）。
A. C++ 探针复现（起点=车心-(半高-10)，向下 10+140）→ 命中/未命中 + C++ 会把盒底摆到哪
B. 四角盒底正下方长探针 → gap（正=悬空 / 负=穿模）
C. 四角地面高度反推理想 pitch/roll
另报 move_speed 与（服务端）代理位姿，便于对比复制落地。

用法：`deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_m4c_ramp_sample.py`
"""

import math
import time

import unreal

out.clear()

SNAP_DOWN = 140.0
MAX_SLOPE = 45.0


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.is_in_play_in_editor():
        out.append("PIE 未运行")
        return

    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    clients = [w for w in worlds if w is not server]

    tank = world = None
    for w in sorted(clients, key=lambda w: w.get_name()):
        for pc in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController):
            try:
                if pc.is_local_player_controller() and pc.get_controlled_pawn():
                    tank, world = pc.get_controlled_pawn(), w
                    break
            except Exception:  # noqa: BLE001
                pass
        if tank is not None:
            break
    if tank is None:
        out.append("没找到客户端本机车")
        return

    loc = tank.get_actor_location()
    rot = tank.get_actor_rotation()
    fwd = tank.get_actor_forward_vector()
    right = tank.get_actor_right_vector()
    up = tank.get_actor_up_vector()
    box = getattr(tank, "collision_box", None)
    half, hx, hy = 59.0, 190.0, 87.5
    if box is not None:
        try:
            se = box.get_scaled_box_extent()
            half, hx, hy = se.z, se.x, se.y
        except Exception:  # noqa: BLE001
            pass

    try:
        speed = tank.get_editor_property("move_speed")
    except Exception:  # noqa: BLE001
        speed = "?"

    def offset(ax, ay, az):
        return unreal.Vector(loc.x + fwd.x * ax + right.x * ay + up.x * az,
                             loc.y + fwd.y * ax + right.y * ay + up.y * az,
                             loc.z + fwd.z * ax + right.z * ay + up.z * az)

    def drop(p, down_from=300.0, down_len=900.0):
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(p.x, p.y, p.z + down_from), unreal.Vector(p.x, p.y, p.z - down_len),
            unreal.TraceTypeQuery.ECC_VISIBILITY, False, [tank], unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        try:
            return (hit.impact_point.z, hit.impact_normal.z)
        except Exception:  # noqa: BLE001
            t = hit.to_tuple()
            return (t[5].z, t[6].z)

    out.append("%s loc=(%.1f,%.1f,%.1f) 姿态(p/y/r)=(%.1f/%.1f/%.1f) 半高=%.1f 盒底Z=%.1f speed=%s"
               % (tank.get_name(), loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll,
                  half, loc.z - half, speed))

    raw = drop(unreal.Vector(loc.x, loc.y, loc.z), 100.0, 1400.0)
    if raw is not None:
        out.append("   车心正下方(长探针)：地面 Z=%.1f → 盒底间隙 %.1fcm" % (raw[0], raw[0] - (loc.z - half)))

    walk_cos = math.cos(math.radians(MAX_SLOPE))
    ps = unreal.Vector(loc.x, loc.y, loc.z - (half - 10.0))
    pe = unreal.Vector(loc.x, loc.y, ps.z - (10.0 + SNAP_DOWN))
    hit = unreal.SystemLibrary.line_trace_single(
        world, ps, pe, unreal.TraceTypeQuery.ECC_VISIBILITY,
        False, [tank], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        out.append("A. C++ 探针(%.1f→%.1f)：**未命中** → C++ 判定悬空、走自由落体分支" % (ps.z, pe.z))
    else:
        try:
            z, nz = hit.impact_point.z, hit.impact_normal.z
        except Exception:  # noqa: BLE001
            t = hit.to_tuple()
            z, nz = t[5].z, t[6].z
        out.append("A. C++ 探针(%.1f→%.1f)：命中 Z=%.1f 法线Z=%.2f 可行走=%s → 盒底将摆到 %.1f（Δ=%.1f）"
                   % (ps.z, pe.z, z, nz, nz >= walk_cos, z, z - (loc.z - half)))

    corners = [("左前", 1, -1), ("右前", 1, 1), ("左后", -1, -1), ("右后", -1, 1)]
    gaps = {}
    for name, fx, fy in corners:
        c = offset(fx * hx, fy * hy, -half)
        res = drop(c)
        if res is None:
            out.append("B. %s角 (%.0f,%.0f,%.0f)：900cm 内无地面 → 完全悬空" % (name, c.x, c.y, c.z))
            continue
        gz, gnz = res
        gap = gz - c.z
        gaps[name] = (gap, c.z)
        out.append("B. %s角 角点Z=%7.1f 地面Z=%7.1f %s%6.1fcm 法线Z=%5.2f"
                   % (name, c.z, gz, "悬空" if gap > 0 else "穿模", abs(gap), gnz))

    if gaps:
        vals = [v[0] for v in gaps.values()]
        out.append("B. 汇总：最大悬空 %.1fcm / 最大穿模 %.1fcm" % (max(vals), -min(vals)))

    if len(gaps) == 4:
        g = {k: (v[0] + v[1]) for k, v in gaps.items()}   # 角点地面世界Z
        pitch_ideal = math.degrees(math.atan2(
            ((g["左前"] + g["右前"]) * 0.5 - (g["左后"] + g["右后"]) * 0.5), 2.0 * hx))
        roll_ideal = math.degrees(math.atan2(
            ((g["右前"] + g["右后"]) * 0.5 - (g["左前"] + g["左后"]) * 0.5), 2.0 * hy))
        out.append("C. 反推理想姿态：pitch=%+.1f roll=%+.1f（当前 pitch=%+.1f roll=%+.1f）"
                   % (pitch_ideal, roll_ideal, rot.pitch, rot.roll))


main()
