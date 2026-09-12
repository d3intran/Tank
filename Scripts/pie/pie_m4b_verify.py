"""M4b 验证：整车缩放 / 地形跟随（爬坡、坠落）/ 名牌（名牌靠截图）。

每个世界各报一遍：
  - 本机车的碰撞盒缩放后尺寸（期望 380 × 175 × 118）
  - 位置含 Z（爬坡/坠落最直接的观测量）、车身 pitch/roll（贴地应随坡面倾斜）
  - 血量（坠落前后应不变 —— 本工程无坠落伤害）

约定：结果收集进 out 列表；严禁对 out 重新赋值，开头清空用 out.clear()。
"""

import unreal

out.clear()

FLOOR_TOP_Z = -40.0


def fnum(v, nd=0):
    try:
        return ("%." + str(nd) + "f") % v
    except Exception:  # noqa: BLE001
        return str(v)


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

    for w in worlds:
        tag = "服务端" if w is server else "客户端"
        out.append("--- %s" % tag)
        local_names = set()
        for pc in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController):
            try:
                if pc.is_local_player_controller():
                    local_names.add(pc.get_name())
            except Exception:  # noqa: BLE001
                pass

        for t in sorted(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.TankPawn),
                        key=lambda a: a.get_name()):
            c = t.get_controller()
            cname = c.get_name() if c else "NONE"
            kind = "本机" if cname in local_names else ("远端" if c else "无主")
            loc = t.get_actor_location()
            rot = t.get_actor_rotation()

            ext = None
            box = getattr(t, "collision_box", None)
            if box is not None:
                try:
                    e = box.get_scaled_box_extent()
                    ext = "%.0fx%.0fx%.0f" % (e.x * 2, e.y * 2, e.z * 2)
                except Exception:  # noqa: BLE001
                    ext = "?"

            hp = "?"
            th = getattr(t, "tank_health", None)
            if th is not None:
                for attr in ("current_health",):
                    v = getattr(th, attr, None)
                    if v is not None:
                        hp = fnum(v)
                        break

            off_ground = loc.z - FLOOR_TOP_Z - (118.0 if ext is None else 0.0)
            out.append("  %-12s %s 盒=%-12s loc=(%7.0f,%7.0f,%6.1f) 姿态(p/y/r)=%s/%s/%s 血=%s"
                       % (t.get_name(), kind, ext or "?", loc.x, loc.y, loc.z,
                          fnum(rot.pitch, 1), fnum(rot.yaw, 1), fnum(rot.roll, 1), hp))
        out.append("")
