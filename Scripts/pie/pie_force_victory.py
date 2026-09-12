"""临时把 GameState 置为「回合结束」以截图核对胜利面板。

不走正常流程（10 杀 + 5s 重开窗口太短，截图抓不到），直接改 GameState 的复制属性：
bMatchOver / WinnerName 都是 UPROPERTY，可用 set_editor_property 直接写。
HUD 的 DrawVictoryPanel 只读 IsMatchOver()/GetWinnerName()，因此能真实渲染出面板。

跑完记得调用本脚本的 revert 模式还原（或直接停 PIE——PIE 世界销毁即还原）。
"""

import unreal

out.clear()

mode = "set"  # 由调用方改；这里默认 set

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    server = None
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            server = w
            break

    if server is None:
        out.append("没找到服务端世界")
    else:
        gss = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankGameState)
        if not gss:
            out.append("没找到 TankGameState")
        else:
            gs = gss[0]
            # 取一个真实玩家名当赢家，顺便验证 HUD 的「本机是否赢家」分支
            pss = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.TankPlayerState)
            winner = pss[0].get_player_name() if pss else "TEST_WINNER"

            gs.set_editor_property("match_over", True)
            gs.set_editor_property("winner_name", winner)
            out.append("已强制设置 MatchOver=True Winner='%s'（用于截图胜利面板）" % winner)
            out.append("还原方式：停 PIE（世界销毁即还原），或把 match_over 设回 False")
