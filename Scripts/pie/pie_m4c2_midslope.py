"""M4c-2 实测 · 静态贴地测试：把 Client 1 本机车放到坡中段上方（悬空 30cm、水平姿态），
不投键，观察 UpdateGroundContact 是否把车「贴下来 + 摆成 30°」。

SE 坡（朝 +X）：坡底边 X=2828 / Z=-17.8，坡面 surface(X) = -17.8 + (2828 - X)·tan30。
X=2400 → 坡面 229.2。起点放 z = 229.2 + 59 + 30 = 318.2，yaw=180（车头朝 -X 上坡）。

再附带：从该位姿做一次「箱体向下 25cm 扫掠」，看会不会被什么挡住 —— 用于判定
「车心 Δz 一直差 18cm 贴不下去」到底是扫掠被挡，还是贴地代码没执行。
"""

import math

import unreal

out.clear()

X_MID, Y_MID = 2400.0, 3000.0
SURFACE = -17.8 + (2828.0 - X_MID) * math.tan(math.radians(30.0))


def local_tank(world):
    for pc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController):
        try:
            if pc.is_local_player_controller() and pc.get_controlled_pawn():
                return pc.get_controlled_pawn()
        except Exception:  # noqa: BLE001
            pass
    return None


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
    client_world = None
    tank = None
    for w in sorted([w for w in worlds if w is not server], key=lambda w: w.get_name()):
        t = local_tank(w)
        if t is not None:
            client_world, tank = w, t
            break

    if tank is None:
        out.append("没找到客户端本机车")
    else:
        box = getattr(tank, "collision_box", None)
        half = box.get_scaled_box_extent().z if box is not None else 59.0
        start_z = SURFACE + half + 30.0
        tank.set_actor_location_and_rotation(
            unreal.Vector(X_MID, Y_MID, start_z),
            unreal.Rotator(pitch=0.0, yaw=180.0, roll=0.0), False, True)
        out.append("坡面 Z=%.1f  起点 (%.0f,%.0f,%.1f)（悬空 30）  pitch=0 yaw=180" % (SURFACE, X_MID, Y_MID, start_z))

        # 箱体向下 25cm 的扫掠测试（忽略自己）
        hit = unreal.SystemLibrary.box_trace_single(
            client_world,
            unreal.Vector(X_MID, Y_MID, start_z - half),
            unreal.Vector(X_MID, Y_MID, start_z - half - 25.0),
            unreal.Rotator(pitch=0.0, yaw=180.0, roll=0.0),
            unreal.Vector(190.0, 87.5, 59.0),
            unreal.TraceTypeQuery.ECC_VISIBILITY, False, [tank],
            unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            out.append("箱体向下 25cm 扫掠: 未命中（可自由下移）")
        else:
            t = hit.to_tuple()
            try:
                who = t[9].get_actor_label()
            except Exception:  # noqa: BLE001
                who = "?"
            out.append("箱体向下 25cm 扫掠: 命中 <%s> Z=%.1f 法线Z=%.2f 时间=%.2f"
                       % (who, t[5].z, t[6].z, t[2]))
