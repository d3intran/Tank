"""让「主机自己的」坦克持续前进（注入 Enhanced Input），用于验证远端负重轮。

为什么用注入：负重轮有两条驱动路径——
  本机受控 → 输入驱动；其他实例（服务器上的他机 / 客户端的远端坦克）→ 从实际位移反推。
没人开车就永远测不到第二条路径，所以这里往主机的 LocalPlayer 注入 IA_MoveForward。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    # 服务端世界 = PC 数 > 1 的那个
    server = None
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False):
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if len(pcs) > 1:
            server = w
            break

    if server is None:
        out.append("没找到服务端世界")
    else:
        # 主机 PC（本地控制）
        host_pc = None
        for pc in unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController):
            f = getattr(pc, "is_local_player_controller", None)
            if f is not None:
                try:
                    if f():
                        host_pc = pc
                        break
                except Exception:  # noqa: BLE001
                    pass

        if host_pc is None:
            out.append("没找到主机 PC")
        else:
            # 拿 LocalPlayer → 再拿 EnhancedInput 子系统（多路尝试）
            sub = None
            lp = None
            try:
                lp = host_pc.get_local_player()
            except Exception as exc:  # noqa: BLE001
                out.append("pc.get_local_player 失败: %s" % exc)

            if lp is None:
                gi = unreal.GameplayStatics.get_game_instance(server)
                if gi is not None and hasattr(gi, "get_local_player"):
                    lp = gi.get_local_player(0)
                    out.append("经 GameInstance 取到 LocalPlayer")

            if lp is not None:
                try:
                    sub = lp.get_subsystem(unreal.EnhancedInputLocalPlayerSubsystem)
                except Exception as exc:  # noqa: BLE001
                    out.append("lp.get_subsystem 失败: %s" % exc)

            if sub is None:
                out.append("!! 拿不到 EnhancedInputLocalPlayerSubsystem (lp=%s)" % lp)
            else:
                ia = unreal.EditorAssetLibrary.load_asset("/Game/tank/inputs/IA_MoveForward.IA_MoveForward")
                if ia is None:
                    out.append("!! 加载 IA_MoveForward 失败")
                else:
                    try:
                        sub.start_continuous_input_injection_for_action(ia, unreal.InputActionValue(1.0))
                        out.append("已开始持续注入 IA_MoveForward = 1.0（主机坦克应开始前进）")
                    except Exception as exc:  # noqa: BLE001
                        out.append("注入失败: %s" % exc)
