"""M4b 验证辅助：把每个客户端世界的**本机**坦克瞬移到中央坡道（Ramp_L_Y）坡底，朝向坡顶。

为什么瞬移客户端的而不是主机的：客户端权威，瞬移**服务器**上的车会被 50Hz 上报立刻覆盖回去
（M2 时 Bug2 判别实验的结论）；瞬移客户端本机车则有效。

两个客户端在 X 上错开 300cm，都在坡宽（600cm）内，避免叠在一起互相推挤。
随车 yaw=90（正 Y 方向）正对坡底，之后用 win_key 投 W 即可上坡。
"""

import unreal

out.clear()

# Ramp_Center_L_Y（Cover_Center_L 的 -Y 面）：坡底 Y≈3851、坡顶 Y≈4420，坡宽 X -1900~-1300
SPOTS = [(-1750.0, 3600.0), (-1450.0, 3600.0)]
FACE_YAW = 90.0            # 朝 +Y（坡顶方向）
GROUND_TOP_Z = -40.0
BODY_HALF_H = 59.0         # 整车 1/2 后的碰撞盒半高

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

    idx = 0
    for w in worlds:
        if w is server:
            continue
        for pc in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController):
            try:
                if not pc.is_local_player_controller():
                    continue
            except Exception:  # noqa: BLE001
                continue
            pawn = pc.get_controlled_pawn() if hasattr(pc, "get_controlled_pawn") else pc.get_controlled_pawn()
            if pawn is None:
                continue
            x, y = SPOTS[idx % len(SPOTS)]
            idx += 1
            target = unreal.Vector(x, y, GROUND_TOP_Z + BODY_HALF_H + 2.0)
            ok = pawn.set_actor_location_and_rotation(
                target, unreal.Rotator(pitch=0.0, yaw=FACE_YAW, roll=0.0), False, True)
            out.append("[%s] %s → (%.0f, %.0f, %.0f) yaw=%.0f  ok=%s"
                       % (w.get_name() if False else "客户端", pawn.get_name(),
                          target.x, target.y, target.z, FACE_YAW, ok))
    if idx == 0:
        out.append("没找到可瞬移的客户端本机车")
