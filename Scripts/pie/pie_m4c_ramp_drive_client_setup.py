"""M4c 爬坡实测 · 准备（客户端版）：把 Client 1 的**本机**车放到 Ramp_SE_X 西侧起跑线，
主机车挪到远处避免挡路/推挤。

为什么用客户端车而不是主机车：主机视口在编辑器主窗口里，PostMessage 投键到 "Tank - Unreal Editor"
不产生 PIE 输入（实测主机车不动）；Client 1 有独立窗口且此前验证过投键有效。

用法：PIE 已运行时 `deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_m4c_ramp_drive_client_setup.py`
"""

import unreal

out.clear()

RUNWAY = (-600.0, 3000.0)
FACE_YAW = 0.0
SLOW_SPEED = 400.0
HOST_PARK = (-2600.0, 7100.0)   # 主机车挪走的位置（西北角空地）


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
    try:
        return hit.impact_point.z
    except Exception:  # noqa: BLE001
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

    # 1) 主机车挪到西北角
    host, host_pc = local_tank(server)
    if host is not None:
        gz = ground_z(server, HOST_PARK[0], HOST_PARK[1], [host]) or 0.0
        host.set_actor_location_and_rotation(
            unreal.Vector(HOST_PARK[0], HOST_PARK[1], gz + half_h_of(host) + 5.0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), False, True)
        out.append("主机车 %s 挪到 %s Z=%.1f" % (host.get_name(), HOST_PARK, gz))
    else:
        out.append("!! 没找到主机车")

    # 2) 选客户端 1（按 PC 名后缀序号最小者）
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
            out.append("（客户端世界）move_speed → %.0f（读回 %.0f）"
                       % (SLOW_SPEED, t.get_editor_property("move_speed")))
        except Exception as exc:  # noqa: BLE001
            out.append("设 move_speed 失败: %s" % exc)
        loc = t.get_actor_location()
        out.append("当前 loc=(%.1f,%.1f,%.1f) 距坡底(X=651) %.0fcm"
                   % (loc.x, loc.y, loc.z, 651.0 - loc.x))
