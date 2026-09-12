"""报告各世界里每辆坦克的炮塔状态（本机权威值 / 收到复制值 / Tick 开关）。

`current_turret_yaw` = 实际应用到 TurretPivot 的 yaw（玩家看到的朝向）
`net_turret_yaw`     = 网络复制进来的权威 yaw（COND_SkipOwner，拥有者本端收不到，恒 0）

判定「敌方视角能不能看到我的炮塔转向」，就看：
  本机 pawn 的 current 变了 → net 在别人那边应该跟着变
  别人那边的 current 是否也变 —— 取决于那辆远端车有没有 Tick
（炮塔是在 Tick 里 SetRelativeRotation 落地的，Tick 关了就只更新变量、不转向）

约定：结果收集进 out 列表；严禁对 out 重新赋值，开头清空用 out.clear()。
"""

import unreal

out.clear()


def fnum(v):
    try:
        return "%.1f" % v
    except Exception:  # noqa: BLE001
        return str(v)


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    ranked = []
    for w in worlds:
        n = len(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController))
        ranked.append((w, n))
    server = max(ranked, key=lambda kv: kv[1])[0] if ranked else None

    for w, n_pc in ranked:
        tag = "服务端" if w is server else "客户端"
        out.append("--- %s (PC=%d)" % (tag, n_pc))
        local_names = set()
        for pc in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController):
            try:
                if pc.is_local_player_controller():
                    local_names.add(pc.get_name())
            except Exception:  # noqa: BLE001
                pass

        tanks = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.TankPawn)
        for t in sorted(tanks, key=lambda a: a.get_name()):
            c = t.get_controller()
            cname = c.get_name() if c else "NONE"
            kind = "本机" if cname in local_names else ("远端" if c else "无主")
            loc = t.get_actor_location()
            try:
                tick = "Tick开" if t.is_actor_tick_enabled() else "Tick关"
            except Exception:  # noqa: BLE001
                tick = "Tick?"
            out.append("  %-12s %s %s loc=(%7.0f,%7.0f) 车体yaw=%6s current_turret=%6s net_turret=%6s net_gun=%6s"
                       % (t.get_name(), kind, tick, loc.x, loc.y,
                          fnum(t.get_actor_rotation().yaw),
                          fnum(getattr(t, "current_turret_yaw", None)),
                          fnum(getattr(t, "net_turret_yaw", None)),
                          fnum(getattr(t, "net_gun_pitch", None))))
        out.append("")
