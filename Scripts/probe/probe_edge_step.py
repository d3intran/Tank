"""边界台阶碰撞复核：车刚刚在 PIE 里越过了 Bound_East（X 2969~3169，高 100）。

查三件事：
  1) 沿 Y=3000、Z=60 从 X=2400 水平打一条 ECC_VISIBILITY 射线穿过台阶 —— 有没有命中？命中在哪个 X、哪个 actor
  2) (3069, 3000) 竖直向下打 —— 顶面 Z 是多少、命中谁
  3) 所有 Bound_ 前缀 actor 的位置/缩放/碰撞开关（CollisionEnabled / Mobility）
  4) 客户端车当前位置 + 车头前方一小段向下探针（看它为什么停在 3257.9）
"""

import unreal

out.clear()

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
acts = unreal.EditorLevelLibrary.get_all_level_actors()


def hit_brief(hit):
    if hit is None:
        return "无命中"
    try:
        z = hit.impact_point.z
        n = hit.impact_normal
        try:
            lbl = hit.to_tuple()[11].get_actor_label()
        except Exception:  # noqa: BLE001
            lbl = "?"
        return "Z=%.1f normal=(%.2f,%.2f,%.2f) <%s>" % (z, n.x, n.y, n.z, lbl)
    except Exception as exc:  # noqa: BLE001
        return "解析失败: %s" % exc


# 1) 水平射线：X 2400 → 3600，Z=60（车心高度）
h = unreal.SystemLibrary.line_trace_single(
    world, unreal.Vector(2400.0, 3000.0, 60.0), unreal.Vector(3600.0, 3000.0, 60.0),
    unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
out.append("1) 水平射线(2400→3600)@Y3000,Z60: %s" % hit_brief(h))

# 2) 竖直射线：台阶顶上
v = unreal.SystemLibrary.line_trace_single(
    world, unreal.Vector(3069.0, 3000.0, 400.0), unreal.Vector(3069.0, 3000.0, -200.0),
    unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
out.append("2) 竖直射线@(3069,3000) 400→-200: %s" % hit_brief(v))

# 3) 近处的地面高度（车心 3257.9 / 车头 3448 下方）
for x in (3257.0, 3448.0, 3600.0):
    g = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, 3000.0, 400.0), unreal.Vector(x, 3000.0, -400.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
    out.append("3) 竖直射线@(%.0f,3000): %s" % (x, hit_brief(g)))

# 4) Bound_ 件全列
out.append("4) Bound_ 件：")
for a in sorted([a for a in acts if a.get_actor_label().startswith("Bound_")],
                key=lambda a: a.get_actor_label()):
    l = a.get_actor_location()
    s = a.get_actor_scale3d()
    lines = ["  %-14s @ (%.0f,%.0f,%.0f) 尺寸 %.0fx%.0fx%.0f"
             % (a.get_actor_label(), l.x, l.y, l.z, s.x * 100, s.y * 100, s.z * 100)]
    try:
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        if smc is not None:
            lines.append("碰撞=%s 移动性=%s 网格=%s"
                         % (smc.get_collision_enabled(), smc.get_editor_property("mobility"),
                            smc.get_editor_property("static_mesh")))
    except Exception as exc:  # noqa: BLE001
        lines.append("读组件失败: %s" % exc)
    out.append(" ".join(lines))

# 5) PIE 里客户端车状态
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
        server = max(worlds, key=lambda w: len(
            unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)))
        for w in sorted((x for x in worlds if x is not server), key=lambda x: x.get_name()):
            for pc in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController):
                try:
                    if pc.is_local_player_controller() and pc.get_controlled_pawn():
                        t = pc.get_controlled_pawn()
                        l = t.get_actor_location()
                        out.append("5) 客户端车 %s loc=(%.1f,%.1f,%.1f) 车头X=%.1f"
                                   % (t.get_name(), l.x, l.y, l.z, l.x + 190.0))
                except Exception:  # noqa: BLE001
                    pass
except Exception as exc:  # noqa: BLE001
    out.append("5) 读 PIE 车失败: %s" % exc)
