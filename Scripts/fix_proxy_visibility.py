import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    if a.get_actor_label() == "SmokeWall_Proxy":
        # 隐藏 Actor 会让包围盒塌成 0（朝向标定曾因此选中错误轴）——
        # 改为：Actor 保持可见、仅隐藏网格组件，包围盒完整保留
        a.set_actor_hidden_in_game(False)
        mesh = a.get_components_by_class(unreal.StaticMeshComponent)[0]
        mesh.set_visibility(False)
        out.append("proxy: actor visible, mesh hidden")
        break
out.append("level saved: %s" % unreal.EditorLevelLibrary.save_current_level())
