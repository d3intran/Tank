"""M4c-2 实测 · 摆位器：按 Scripts/_place.txt 把 Client 1 本机车摆到指定位姿，并做一次
「箱体向下 25cm 扫掠」诊断（看贴地下移会不会被几何挡住、被谁挡住）。

_place.txt 一行：x,y,z,pitch,yaw[,speed]

配合 pie_m4c2_sample.py 循环使用（每次摆位后隔几帧再采样）。
"""

import math
import os

import unreal

out.clear()

# 桥进程的 CWD 不是工程根，必须用绝对路径
PLACE = os.path.join(unreal.Paths.project_dir(), "Scripts", "_place.txt")

with open(PLACE, "r", encoding="utf-8") as f:
    parts = [p.strip() for p in f.read().strip().split(",")]
x, y, z, pitch, yaw = (float(v) for v in parts[:5])
speed = float(parts[5]) if len(parts) > 5 else None


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
    client_world = tank = None
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
        ext = box.get_scaled_box_extent() if box is not None else unreal.Vector(190.0, 87.5, 59.0)
        tank.set_actor_location_and_rotation(
            unreal.Vector(x, y, z), unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0), False, True)
        if speed is not None:
            try:
                tank.set_editor_property("move_speed", speed)
            except Exception:  # noqa: BLE001
                pass
        out.append("摆位 → (%.1f,%.1f,%.1f) pitch=%.1f yaw=%.1f%s"
                   % (x, y, z, pitch, yaw, "" if speed is None else " speed=%.0f" % speed))

        # 箱体向下 25cm 扫掠（全关键字传参：本引擎 Python 的位置参数序不可信）。
        # 注意：CollisionBox 就是根组件，箱中心 = actor 位置，不要减半高
        hit = unreal.SystemLibrary.box_trace_single(
            world_context_object=client_world,
            start=unreal.Vector(x, y, z),
            end=unreal.Vector(x, y, z - 25.0),
            orientation=unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0),
            half_size=unreal.Vector(ext.x - 0.5, ext.y - 0.5, ext.z - 0.5),
            trace_channel=unreal.TraceTypeQuery.ECC_VISIBILITY,
            trace_complex=False,
            actors_to_ignore=[tank],
            draw_debug_type=unreal.DrawDebugTrace.NONE,
            ignore_self=True)
        if hit is None:
            out.append("箱体向下 25cm 扫掠: 未命中（可自由下移 25）")
        else:
            t = hit.to_tuple()
            try:
                who = t[9].get_actor_label()
            except Exception:  # noqa: BLE001
                who = "?"
            out.append("箱体向下 25cm 扫掠: 命中 <%s> Z=%.1f 法线Z=%.2f Time=%.2f"
                       % (who, t[5].z, t[6].z, t[2]))

        # 全尺寸（不内缩）零长度 + 5cm 下扫：查「悬空冻结」到底碰到了什么（M4c-3）
        for tag, half_use in (("全尺寸", ext), ("内缩1cm", unreal.Vector(ext.x - 1.0, ext.y - 1.0, ext.z - 1.0))):
            h2 = unreal.SystemLibrary.box_trace_single(
                world_context_object=client_world,
                start=unreal.Vector(x, y, z), end=unreal.Vector(x, y, z),
                orientation=unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0),
                half_size=half_use, trace_channel=unreal.TraceTypeQuery.ECC_VISIBILITY,
                trace_complex=False, actors_to_ignore=[tank],
                draw_debug_type=unreal.DrawDebugTrace.NONE, ignore_self=True)
            if h2 is None:
                out.append("%s 零长度: 不阻塞" % tag)
                continue
            t2 = h2.to_tuple()
            try:
                who2 = t2[9].get_actor_label()
            except Exception:  # noqa: BLE001
                who2 = "?"
            out.append("%s 零长度: 命中 <%s> StartPenetrating=%s 命中点Z=%.1f 法线Z=%.2f"
                       % (tag, who2, t2[1], t2[5].z, t2[6].z))
        h3 = unreal.SystemLibrary.box_trace_single(
            world_context_object=client_world,
            start=unreal.Vector(x, y, z), end=unreal.Vector(x, y, z - 5.0),
            orientation=unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0),
            half_size=ext, trace_channel=unreal.TraceTypeQuery.ECC_VISIBILITY,
            trace_complex=False, actors_to_ignore=[tank],
            draw_debug_type=unreal.DrawDebugTrace.NONE, ignore_self=True)
        if h3 is None:
            out.append("全尺寸 下扫 5cm: 未命中（未接触任何东西）")
        else:
            t3 = h3.to_tuple()
            try:
                who3 = t3[9].get_actor_label()
            except Exception:  # noqa: BLE001
                who3 = "?"
            out.append("全尺寸 下扫 5cm: 命中 <%s> StartPenetrating=%s Time=%.3f 命中点Z=%.1f 法线Z=%.2f"
                       % (who3, t3[1], t3[2], t3[5].z, t3[6].z))
