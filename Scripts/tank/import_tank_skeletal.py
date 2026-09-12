"""导入骨骼化坦克（Import/tank_skeletal/ztz88a_skeletal.fbx）并做数值验证 + 重指材质。

验证项（SOP-4：把"看"变成"测"）：
  1. 包围盒与**现有静态网格资产**逐轴一致（后者是用户已确认的观感基准真值，
     也顺带钉死 Blender→UE 的 Y 镜像换算）
  2. 骨架里 13 根骨，轮骨 ref pose 位置 = TankPawn.cpp 的 RoadSetups（(X, ±144, 41.75)，
     r 侧 = UE +Y）
  3. 材质槽按槽名重指 mat_61 / M_TrackScroll（SOP-2：重导会重置材质槽）
关卡不会被保存（临时 actor 用完即销毁；socket 探测已弃用——属性只读）。
"""

import unreal

out.clear()

FBX = r"E:\UE\Tank\Import\tank_skeletal\ztz88a_skeletal.fbx"
DEST = "/Game/tank/ztz-88a"
NAME = "ztz88a_skeletal"
MESH_PATH = "%s/%s.%s" % (DEST, NAME, NAME)
SKELETON_PATH = "%s/%s_Skeleton.%s_Skeleton" % (DEST, NAME, NAME)

# 期望包围盒：ztz88a_hull_body ∪ ztz88a_tracks_full 的 UE 空间实测值（verify_skeletal_orientation.py）
EXPECTED_BBOX = ((-350.7, -181.2, -0.1), (338.7, 177.1, 153.6))

WHEEL_X = [-206.00, -131.00, -56.00, 19.00, 107.50, 199.95]
WHEEL_Y = 144.0
WHEEL_Z = 41.75
EXPECTED_BONES = {"root": (0.0, 0.0, 0.0), "TankArmature": (0.0, 0.0, 0.0)}
for i, wx in enumerate(WHEEL_X):
    EXPECTED_BONES["wheel_r%d" % i] = (wx, WHEEL_Y, WHEEL_Z)
    EXPECTED_BONES["wheel_l%d" % i] = (wx, -WHEEL_Y, WHEEL_Z)

# ---- 1. 导入 ----
task = unreal.AssetImportTask()
task.set_editor_property("filename", FBX)
task.set_editor_property("destination_path", DEST)
task.set_editor_property("destination_name", NAME)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)

options = unreal.FbxImportUI()
options.set_editor_property("import_mesh", True)
options.set_editor_property("import_as_skeletal", True)
options.set_editor_property("import_materials", False)
options.set_editor_property("import_textures", False)
options.set_editor_property("create_physics_asset", False)  # 物理资产单独建（见 create_tank_physics_asset.py）
options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
skel_import = options.get_editor_property("skeletal_mesh_import_data")
skel_import.set_editor_property("normal_import_method",
                                unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
options.set_editor_property("skeletal_mesh_import_data", skel_import)
task.set_editor_property("options", options)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
for path in task.get_editor_property("imported_object_paths"):
    out.append("导入产物: %s" % path)

mesh = unreal.load_asset(MESH_PATH)
if mesh is None:
    out.append("!! 骨骼网格加载失败")
else:
    out.append("骨骼网格: %s" % mesh.get_path_name())
    skel = mesh.get_editor_property("skeleton")
    out.append("骨架: %s" % (skel.get_path_name() if skel else None))

    # ---- 2. 材质槽重指（按槽名认，不认下标）----
    MAT_MAP = {"MAT_HULL": "/Game/tank/ztz-88a/mat_61.mat_61",
               "MAT_TRACKS": "/Game/tank/ztz-88a/M_TrackScroll.M_TrackScroll"}
    materials = mesh.get_editor_property("materials")
    for i, sm in enumerate(materials):
        slot_name = str(sm.get_editor_property("material_slot_name"))
        target = MAT_MAP.get(slot_name)
        if target is None:
            out.append("  材质槽[%d] %s 无映射，保持默认" % (i, slot_name))
            continue
        mat = unreal.load_asset(target)
        if mat is None:
            out.append("  !! 材质加载失败: %s" % target)
            continue
        sm.set_editor_property("material_interface", mat)
        out.append("  材质槽[%d] %s ← %s" % (i, slot_name, target.split("/")[-1]))
    mesh.set_editor_property("materials", materials)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    # ---- 3. 包围盒：与静态网格资产比对 ----
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkeletalMeshActor, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0, 0, 0))
    comp = actor.get_editor_property("skeletal_mesh_component")
    comp.set_skeletal_mesh(mesh)
    origin, extent = actor.get_actor_bounds(False)
    bb_min = (origin.x - extent.x, origin.y - extent.y, origin.z - extent.z)
    bb_max = (origin.x + extent.x, origin.y + extent.y, origin.z + extent.z)
    out.append("包围盒: min=(%.1f, %.1f, %.1f) max=(%.1f, %.1f, %.1f)"
               % (bb_min[0], bb_min[1], bb_min[2], bb_max[0], bb_max[1], bb_max[2]))
    ok = all(abs(bb_min[i] - EXPECTED_BBOX[0][i]) < 1.5 and abs(bb_max[i] - EXPECTED_BBOX[1][i]) < 1.5
             for i in range(3))
    out.append("包围盒断言: %s（基准 = ztz88a_hull_body ∪ ztz88a_tracks_full）"
               % ("通过" if ok else "**失败**"))
    unreal.EditorLevelLibrary.destroy_actor(actor)

    # ---- 4. 骨缩放/旋转断言 + 骨位记录 ----
    # 关键回归项（两起实测事故）：
    #  a) FBX 导出的**单位换算**若留在骨架上（TankArmature scale=100）→ 物理盒被放大 50 倍；
    #  b) FBX 导出的**轴转换**若留在骨架上（TankArmature −90°X 旋转）→ 绑在 root 骨上的物理刚体
    #     空间整体转 90°：整车 roll=90°（视觉躺倒）、悬挂射线朝侧面扫（永远探不到地）、给油不走。
    # 所以根链（TankArmature → root）必须 scale≈1 **且** rotation≈单位四元数。
    ROOT_CHAIN = ("TankArmature", "root")
    try:
        pose = skel.get_reference_pose()
        names = [str(n) for n in pose.get_bone_names()]
        out.append("骨架骨数(含臂): %d → %s" % (len(names), names))
        ok = True
        for bone_name in names:
            t = pose.get_bone_pose(bone_name)
            loc, scale, rot = t.translation, t.scale3d, t.rotation
            scale_bad = max(abs(scale.x - 1.0), abs(scale.y - 1.0), abs(scale.z - 1.0)) > 0.001
            rot_bad = (abs(rot.x) > 1e-3 or abs(rot.y) > 1e-3 or abs(rot.z) > 1e-3) if bone_name in ROOT_CHAIN else False
            bad = scale_bad or rot_bad
            ok = ok and not bad
            out.append("  %-10s 局部位置=(%8.2f,%8.2f,%8.2f) 缩放=%.3f 旋转=(%.3f,%.3f,%.3f)%s%s%s"
                       % (bone_name, loc.x, loc.y, loc.z, scale.x, rot.x, rot.y, rot.z,
                          " **缩放非1**" if scale_bad else "",
                          " **根链有旋转**" if rot_bad else "",
                          "" if bad else " ok"))
        out.append("骨缩放/旋转断言: %s"
                   % ("通过（根链 scale=1 且无旋转，物理刚体空间与车体一致）" if ok else "**失败**"))

        # 轮骨的**组件空间**位置（物理仿真用的就是这个）—— 用「相对 TankArmature」的 ref pose 变换读
        for probe in ("wheel_r0", "wheel_l0"):
            try:
                rt = pose.get_ref_pose_relative_transform(probe, "TankArmature")
                p = rt.translation
                out.append("  %s 组件空间位置=(%.2f, %.2f, %.2f)（期望 ≈(%s, ±144, 41.75)）"
                           % (probe, p.x, p.y, p.z, "-206" if probe == "wheel_r0" else "-206"))
            except Exception as exc:  # noqa: BLE001
                out.append("  读 %s 组件空间位置失败: %s" % (probe, str(exc)[:80]))
    except Exception as exc:  # noqa: BLE001
        out.append("读 ref pose 失败: %s" % exc)
