"""M4c-2 实测 · 准备：Client 1 本机车放到 Ramp_SE_X 的**东侧**起跑线（新坡朝 +X，从场边往掩体开）。

新坡几何：SE 掩体 (1600,3000) 的 +X 面在 X=2000，坡底边 X=2828，坡顶边 X=2000 / Z=460。
起跑点 (3150, 3000) 朝 -X（yaw=180）：到坡底 322cm，坡面投影 828cm，冲上掩体顶后
继续向西 800cm 会从掩体西缘掉下去（顺带验证自由落体与落地）。
主机车挪到西北角空地避免挡路。

用法：PIE 已运行时 `deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_m4c2_setup.py`
"""

import unreal

out.clear()

RUNWAY = (3150.0, 3000.0)
FACE_YAW = 180.0                # 朝 -X（往坡上开）
SLOW_SPEED = 400.0              # cm/s
HOST_PARK = (-2600.0, 7100.0)


def half_h_of(tank):
    box = getattr(tank, "collision_box", None)
    if box is not None:
        try:
            return box.get_scaled_box_extent().z
        except Exception:  # noqa: BLE001
            pass
    return 59.0


def ground_z(world, x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 300.0), unreal.Vector(x, y, -400.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, ignore, unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    return hit.to_tuple()[5].z


def local_tank(world):
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                return pc.get_controlled_pawn(), pc.get_name()
        except Exception:  # noqa: BLE001
            pass
    return None, None


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    clients = [w for w in worlds if w is not server]

    host, _ = local_tank(server)
    if host is not None:
        gz = ground_z(server, HOST_PARK[0], HOST_PARK[1], [host]) or 0.0
        host.set_actor_location_and_rotation(
            unreal.Vector(HOST_PARK[0], HOST_PARK[1], gz + half_h_of(host) + 5.0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), False, True)
        out.append("主机车 %s 挪到 (%0.f,%0.f) Z=%.1f" % (host.get_name(), HOST_PARK[0], HOST_PARK[1], gz))

    picked = None
    for w in sorted(clients, key=lambda w: w.get_name()):
        t, pcname = local_tank(w)
        if t is not None:
            picked = (w, t, pcname)
            break
        out.append("世界 %s 无本机车" % w.get_name())

    if picked is None:
        out.append("!! 没找到客户端本机车")
    else:
        w, t, pcname = picked
        gz = ground_z(w, RUNWAY[0], RUNWAY[1], [t]) or 0.0
        target = unreal.Vector(RUNWAY[0], RUNWAY[1], gz + half_h_of(t) + 2.0)
        ok = t.set_actor_location_and_rotation(
            target, unreal.Rotator(pitch=0.0, yaw=FACE_YAW, roll=0.0), False, True)
        out.append("客户端车 %s（%s，世界 %s）→ (%.0f,%.0f,%.1f) yaw=%.0f ok=%s"
                   % (t.get_name(), pcname, w.get_name(), target.x, target.y, target.z, FACE_YAW, ok))
        try:
            t.set_editor_property("move_speed", SLOW_SPEED)
            out.append("move_speed → %.0f（读回 %.0f）" % (SLOW_SPEED, t.get_editor_property("move_speed")))
        except Exception as exc:  # noqa: BLE001
            out.append("设 move_speed 失败: %s" % exc)
