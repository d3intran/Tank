"""M4c 爬坡实测 · 准备：把**主机本机**坦克瞬移到 Ramp_SE_X 的西侧起跑线，朝 +X（坡底方向）。

为什么瞬移主机车：主机车是服务端本地控制，瞬移不会被 50Hz 上报覆盖（M2 结论）。
为什么选 Ramp_SE_X：SE 掩体 (1600,3000) 的 -X 面坡，坡顶边 X=1220、坡底 X=651，
从 X=-600 沿 Y=3000 向东是空旷路面，跑道 1251cm，无遮挡（SW 掩体东面在 X=-1200）。

同时把 move_speed 调到 400 cm/s（默认 1600）：坡面段 ~743cm 能跑 1.9s，
外部采样循环（每轮 ~0.6s）来得及取 3~4 个坡上样本。

用法：PIE 已运行时 `deno run -A Scripts/editor.deno.ts execfile Scripts/pie/pie_m4c_ramp_drive_setup.py`
"""

import unreal

out.clear()

RUNWAY = (-600.0, 3000.0)   # 起跑点（空旷路面）
FACE_YAW = 0.0              # +X
SLOW_SPEED = 400.0          # cm/s


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

    box = getattr(tank, "collision_box", None)
    half_h = 59.0
    if box is not None:
        try:
            half_h = box.get_scaled_box_extent().z
        except Exception:  # noqa: BLE001
            pass

    hit = unreal.SystemLibrary.line_trace_single(
        server, unreal.Vector(RUNWAY[0], RUNWAY[1], 300.0), unreal.Vector(RUNWAY[0], RUNWAY[1], -400.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [tank], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        out.append("起跑点 (%0.f,%0.f) 向下没探到地面" % RUNWAY)
        return
    try:
        ground_z = hit.impact_point.z
        nz = hit.impact_normal.z
    except Exception:  # noqa: BLE001
        t = hit.to_tuple()
        ground_z, nz = t[5].z, t[6].z
    out.append("起跑点地面 Z=%.1f 法线Z=%.2f 半高=%.1f" % (ground_z, nz, half_h))

    target = unreal.Vector(RUNWAY[0], RUNWAY[1], ground_z + half_h + 2.0)
    ok = tank.set_actor_location_and_rotation(
        target, unreal.Rotator(pitch=0.0, yaw=FACE_YAW, roll=0.0), False, True)
    out.append("瞬移 %s → (%.0f, %.0f, %.1f) yaw=%.0f ok=%s"
               % (tank.get_name(), target.x, target.y, target.z, FACE_YAW, ok))

    try:
        tank.set_editor_property("move_speed", SLOW_SPEED)
        out.append("move_speed → %.0f（读回 %.0f）"
                   % (SLOW_SPEED, tank.get_editor_property("move_speed")))
    except Exception as exc:  # noqa: BLE001
        out.append("设 move_speed 失败: %s" % exc)

    loc = tank.get_actor_location()
    out.append("当前 loc=(%.1f,%.1f,%.1f) 距坡底(X=651) %.0fcm"
               % (loc.x, loc.y, loc.z, 651.0 - loc.x))


main()
