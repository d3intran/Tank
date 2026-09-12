"""M3 状态巡检：计分（PlayerState.Kills）、胜负（GameState）、重生保护（TankPawn）。

注意 Python 侧取名：
  - `Kills` / `Deaths` / `WinnerName` / `KillsToWin` 无 b 前缀 → 直接用同名 snake_case
  - `bMatchOver` / `bSpawnProtected` 有 b 前缀 → UHT 反射名保留 b，用 `b_match_over` 试，失败再试无前缀
"""

import unreal

out.clear()


def prop(obj, *names):
    """按候选名依次尝试读属性，返回 (值, 命中的名字)。"""
    for n in names:
        try:
            return obj.get_editor_property(n), n
        except Exception:  # noqa: BLE001
            continue
    return "<N/A>", None


def find_server_world():
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            return w
    return None


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    server = find_server_world()
    if server is None:
        out.append("没找到服务端世界")
    else:
        # ---- GameState ----
        gss = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankGameState)
        out.append("TankGameState 数量 = %d（应为 1）" % len(gss))
        for gs in gss:
            mo, _ = prop(gs, "b_match_over", "match_over")
            wn, _ = prop(gs, "winner_name")
            ktw, _ = prop(gs, "kills_to_win")
            out.append("   MatchOver=%s  Winner='%s'  KillsToWin=%s" % (mo, wn, ktw))

        # ---- PlayerState（计分板数据源）----
        pss = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPlayerState)
        out.append("TankPlayerState 数量 = %d（应为 3）" % len(pss))
        for ps in sorted(pss, key=lambda p: p.get_player_name()):
            k, _ = prop(ps, "kills")
            d, _ = prop(ps, "deaths")
            out.append("   %-22s Kills=%s Deaths=%s" % (ps.get_player_name(), k, d))

        # ---- 坦克 + 重生保护 ----
        tanks = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPawn)
        out.append("TankPawn 数量 = %d" % len(tanks))
        for t in tanks:
            c = t.get_controller()
            sp, _ = prop(t, "b_spawn_protected", "spawn_protected")
            h, _ = prop(t, "tank_health")
            hp = "<n/a>"
            if h is not None and not isinstance(h, str):
                cur, _ = prop(h, "current_health")
                mx, _ = prop(h, "max_health")
                hp = "%s/%s" % (cur, mx)
            out.append("   %-14s ctrl=%-20s HP=%-12s SpawnProtected=%s"
                       % (t.get_name(), c.get_name() if c else "NONE", hp, sp))
