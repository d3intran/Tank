"""报告 PIE 里的比赛规则与比分（验证 KillsToWin 等 Config 改动是否真的生效）。

`KillsToWin` 是 `UPROPERTY(Config)`，只在引擎启动时读一次 —— 改 ini 后必须重启编辑器才生效，
这个脚本就是用来给出「已生效」的直接证据。

约定：结果收集进 out 列表；严禁对 out 重新赋值，开头清空用 out.clear()。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    server = max(worlds, key=lambda w: len(
        unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))

    gm = unreal.GameplayStatics.get_game_mode(server)
    out.append("GameMode = %s" % (gm.get_class().get_name() if gm else None))
    for attr in ("kills_to_win", "round_restart_delay"):
        out.append("  %-20s = %s" % (attr, getattr(gm, attr, "n/a")))

    gs = unreal.GameplayStatics.get_game_state(server)
    out.append("GameState = %s" % (gs.get_class().get_name() if gs else None))
    if gs is not None:
        for attr in ("kills_to_win", "round_restart_delay", "round_state", "match_winner"):
            if hasattr(gs, attr):
                out.append("  %-20s = %s" % (attr, getattr(gs, attr)))

    out.append("比分（PlayerState.Kills）:")
    for ps in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerState):
        out.append("  %-22s kills=%s" % (ps.get_name(), getattr(ps, "kills", "n/a")))
