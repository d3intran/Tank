"""载具（ATankVehicle）PIE 自检报告：物理资产 / 接地 / 轮位 / 视觉轮。

验证目标（阶段 1 验收）：
  1. 物理资产真的挂在骨骼网格上、刚体是 root 骨，碰撞盒尺寸 = 380×175×118（世界）
  2. 12 个物理轮全部接地，接触点位置 = 网格空间 (X/2, ±72, 地面)（证明轮骨位置/有效半径正确）
  3. 视觉负重轮的相对 Z 反映悬挂行程、相对 X 转角随行驶滚动（证明仿真→视觉链路通）
  4. 整车 Z ≈ 履带底面贴地（网格原点在履带底面，静止时 actor.z ≈ 0 + 悬挂静态压缩）

用法：PIE 起好后执行；驾驶对比再跑一次即可看位移/转角变化。
"""

import unreal

out.clear()

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.is_in_play_in_editor():
    out.append("PIE 未运行")
else:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    # 服务器世界 = PlayerController 最多的那个（沿用本项目既有约定）
    server = None
    for w in worlds:
        pcs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerController)
        if server is None or len(pcs) > len(unreal.GameplayStatics.get_all_actors_of_class(server, unreal.PlayerController)):
            server = w

    for wi, w in enumerate(worlds):
        vehicles = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.TankVehicle)
        tag = "SERVER" if w is server else "client"
        out.append("---- 世界[%d] %s：载具 %d 辆 ----" % (wi, tag, len(vehicles)))
        for v in vehicles:
            loc = v.get_actor_location()
            rot = v.get_actor_rotation()
            vel = v.get_velocity()
            speed = (vel.x * vel.x + vel.y * vel.y + vel.z * vel.z) ** 0.5
            ctrl = v.get_controller()
            out.append("  %s loc=(%.1f, %.1f, %.1f) yaw=%.1f |v|=%.0f ctrl=%s"
                       % (v.get_name(), loc.x, loc.y, loc.z, rot.yaw, speed,
                          ctrl.get_name() if ctrl else "NONE"))

            mesh = v.get_editor_property("vehicle_mesh")
            if mesh is None:
                out.append("    !! 没有 vehicle_mesh")
                continue

            # 1. 物理资产 & 刚体包围盒
            pa = None
            try:
                pa = mesh.get_editor_property("physics_asset_override")
            except Exception:  # noqa: BLE001
                pass
            if pa is None:
                try:
                    skm = mesh.get_editor_property("skeletal_mesh_asset")
                    pa = skm.get_editor_property("physics_asset") if skm else None
                except Exception:  # noqa: BLE001
                    pass
            out.append("    物理资产=%s  组件模拟物理=%s"
                       % (pa.get_name() if pa else "NONE", mesh.is_simulating_physics()))
            origin, extent = v.get_actor_bounds(True)
            out.append("    碰撞包围盒 半尺寸=(%.1f, %.1f, %.1f)（期望 ≈190, 87.5, 59 → 全尺寸 380×175×118）"
                       % (extent.x, extent.y, extent.z))

            # 2. 物理轮状态（UChaosVehicleWheel 的 getter 都是 BlueprintCallable，Python 可调）
            movement = v.get_editor_property("vehicle_movement")
            if movement is None:
                out.append("    !! 没有 vehicle_movement")
                continue
            wheels = movement.get_editor_property("wheels")
            out.append("    物理轮 %d 个" % len(wheels))
            in_air = 0
            rows = []
            for i, wheel in enumerate(wheels):
                air = wheel.is_in_air()
                if air:
                    in_air += 1
                rows.append("      轮[%2d] 离地=%-5s 半径=%.1f 悬挂行程=%6.2f 转角=%7.1f°"
                            % (i, air, wheel.get_wheel_radius(),
                               wheel.get_suspension_offset(), wheel.get_rotation_angle()))
            out.append("    离地轮数 %d / %d" % (in_air, len(wheels)))
            for r in rows[:2] + rows[5:8] + rows[11:12]:
                out.append(r)

            # 3. 视觉负重轮（相对位姿就是仿真→视觉的落地结果）
            visual_wheels = []
            for comp in v.get_components_by_class(unreal.StaticMeshComponent):
                name = comp.get_name()
                if name.startswith("RoadWheel"):
                    visual_wheels.append((name, comp))
            visual_wheels.sort(key=lambda t: int(t[0].replace("RoadWheel", "")) if t[0].replace("RoadWheel", "").isdigit() else 999)
            out.append("    视觉负重轮 %d 个" % len(visual_wheels))
            for name, comp in visual_wheels[:3]:
                rl = comp.get_editor_property("relative_location")
                rr = comp.get_editor_property("relative_rotation")
                out.append("      %-12s rel=(%.1f, %.1f, %.1f) spin=%.1f°（z 含悬挂行程，spin 为滚动角）"
                           % (name, rl.x, rl.y, rl.z, rr.roll))

            # 4. 履带 UV 参数（MID 存在即材质槽指向正确；Transient 属性可能不在 Python 暴露面上）
            try:
                mid = v.get_editor_property("track_material_instance")
                out.append("    履带 MID=%s" % ("有" if mid else "无"))
            except Exception as exc:  # noqa: BLE001
                out.append("    履带 MID 读取不可用（%s）" % str(exc)[:60])
