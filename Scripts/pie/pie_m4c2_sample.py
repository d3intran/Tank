"""M4c-2 实测 · 采样器：一次调用打一行（给 bash 循环用）。

对 Client 1 本机车做两块测量：
  A. **真实角 gap 体检**：车体四角（±半长, ±半宽, -半高，按当前姿态旋转后的真实角点）
     竖直探针，gap = 地面Z - 角点Z → gap<0 = 悬空 |gap|；gap>0 = 角点插进地面（穿模）
  B. **C++ 复算**：与 C++ 完全同参数（yaw 水平偏移 ±(半长-10)/±(半宽-10)、同窗口、同过滤），
     atan2 反推理想 pitch/roll，TargetActorZ = 地面高 + 半高/cos(倾角)，Δz = 目标 - 当前
  C. 实际姿态 / b_grounded；服务端那份**代理**（按位置就近配对）看 pitch 复制

用法（循环里重复调用，每次一行）：
  for i in $(seq 24); do deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_m4c2_sample.py; sleep 0.05; done
"""

import math

import unreal

out.clear()

MAX_SLOPE_DEG = 45.0
MAX_STEP_UP = 15.0
SNAP_DOWN = 140.0


def get_grounded(tank):
    for name in ("b_grounded", "grounded", "bGrounded"):
        try:
            return getattr(tank, name)
        except Exception:  # noqa: BLE001
            continue
    return "?"


def box_extent(tank):
    box = getattr(tank, "collision_box", None)
    if box is not None:
        try:
            se = box.get_scaled_box_extent()
            return se.x, se.y, se.z
        except Exception:  # noqa: BLE001
            pass
    return 190.0, 87.5, 59.0


def trace(world, x, y, top, bottom, ignore):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, top), unreal.Vector(x, y, bottom),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, ignore, unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    return (t[5].z, t[6].z)


def report(world, tank, tag, detail=False):
    loc = tank.get_actor_location()
    rot = tank.get_actor_rotation()
    fwd = tank.get_actor_forward_vector()
    right = tank.get_actor_right_vector()
    up = tank.get_actor_up_vector()
    hx, hy, half = box_extent(tank)
    probe_long = max(10.0, hx - 10.0)
    probe_lat = max(10.0, hy - 10.0)
    probe_up = hx + half + 20.0
    probe_down = 3.0 * hx + half + 20.0
    walk_cos = math.cos(math.radians(MAX_SLOPE_DEG))

    # ---- A. 真实角点 gap（车体几何上的四个底角）----
    gap_lines = []
    gaps = []
    for ax, ay, nm in ((+hx, -hy, "左前"), (+hx, +hy, "右前"), (-hx, -hy, "左后"), (-hx, +hy, "右后")):
        cx = loc.x + fwd.x * ax + right.x * ay + up.x * (-half)
        cy = loc.y + fwd.y * ax + right.y * ay + up.y * (-half)
        cz = loc.z + fwd.z * ax + right.z * ay + up.z * (-half)
        res = trace(world, cx, cy, cz + probe_up, cz - probe_down, [tank])
        if res is None:
            gap_lines.append("  %s 未命中" % nm)
            continue
        gz = res[0]
        gap = gz - cz
        gaps.append(gap)
        gap_lines.append("  %s角 角点Z=%7.1f 地面Z=%7.1f %s %5.1f" % (
            nm, cz, gz, "穿模" if gap > 0 else "悬空", abs(gap)))

    # ---- B. C++ 复算（yaw 水平偏移，与 C++ 同参数）----
    yaw_rad = math.radians(rot.yaw)
    fwd_h = (math.cos(yaw_rad), math.sin(yaw_rad))
    right_h = (-math.sin(yaw_rad), math.cos(yaw_rad))

    def probe(ax, ay):
        px = loc.x + fwd_h[0] * ax + right_h[0] * ay
        py = loc.y + fwd_h[1] * ax + right_h[1] * ay
        cz = loc.z + fwd.z * ax + right.z * ay + up.z * (-half)
        res = trace(world, px, py, cz + probe_up, cz - probe_down, [tank])
        if res is None:
            return None
        gz, nz = res
        if nz < walk_cos or gz > cz + MAX_STEP_UP:
            return None
        return gz

    zFL, zFR = probe(+probe_long, -probe_lat), probe(+probe_long, +probe_lat)
    zRL, zRR = probe(-probe_long, -probe_lat), probe(-probe_long, +probe_lat)

    def grp(a, b):
        vals = [v for v in (a, b) if v is not None]
        return (sum(vals) / len(vals)) if vals else None

    zf, zr = grp(zFL, zFR), grp(zRL, zRR)
    zl, zrt = grp(zFL, zRL), grp(zFR, zRR)
    pitch_ideal = roll_ideal = delta = None
    in_band = "-"
    if zf is not None and zr is not None:
        pitch_ideal = math.degrees(math.atan2(zf - zr, 2.0 * probe_long))
        if zl is not None and zrt is not None:
            roll_ideal = math.degrees(math.atan2(zrt - zl, 2.0 * probe_lat))
        tilt_cos = max(0.05, math.cos(math.radians(pitch_ideal)) * math.cos(math.radians(roll_ideal or 0.0)))
        target_z = (zf + zr) * 0.5 + half / tilt_cos
        delta = target_z - loc.z
        in_band = "在带内" if (-SNAP_DOWN <= delta <= MAX_STEP_UP) else "出带"

    gap_txt = ""
    if gaps:
        gap_txt = " 角gap[%s] max悬空%.1f max穿模%.1f" % (
            " ".join("%+.1f" % g for g in gaps),
            max(0.0, -min(gaps)), max(0.0, max(gaps)))
    out.append("t=%6.2f [%s %s] loc=(%7.1f,%7.1f,%7.1f) p/y/r=(%+6.1f,%6.1f,%+5.1f) G=%s |理想p/r=(%s,%s) Δz=%s(%s)|%s"
               % (unreal.GameplayStatics.get_time_seconds(world), tag, tank.get_name(), loc.x, loc.y, loc.z,
                  rot.pitch, rot.yaw, rot.roll, get_grounded(tank),
                  "  ?  " if pitch_ideal is None else "%+5.1f" % pitch_ideal,
                  "  ?  " if roll_ideal is None else "%+5.1f" % roll_ideal,
                  "    ?" if delta is None else "%+7.1f" % delta, in_band, gap_txt))
    if detail:
        out.extend(gap_lines)


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    clients = [w for w in worlds if w is not server]

    client_world = client_tank = None
    for w in sorted(clients, key=lambda w: w.get_name()):
        for pc in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController):
            try:
                if pc.is_local_player_controller() and pc.get_controlled_pawn():
                    client_world, client_tank = w, pc.get_controlled_pawn()
                    break
            except Exception:  # noqa: BLE001
                pass
        if client_tank is not None:
            break

    if client_tank is None:
        out.append("没找到客户端本机车")
    else:
        report(client_world, client_tank, "client", detail=True)
        cl = client_tank.get_actor_location()
        best = best_d = None
        for a in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn):
            d = (a.get_actor_location() - cl).length()
            if best_d is None or d < best_d:
                best, best_d = a, d
        if best is not None:
            report(server, best, "server@%.0fcm" % best_d)
