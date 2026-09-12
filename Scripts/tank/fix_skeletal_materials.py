"""给骨骼网格重指材质槽并**真正写盘**。

为什么单独来一遍：`Scripts/tank/import_tank_skeletal.py` 里用的
`unreal.EditorAssetLibrary.save_loaded_asset` 在本工程失效（详见项目记忆），
材质指派只留在内存 → 编辑器一重启就回到 WorldGridMaterial（实测：车体渲染成默认灰、
履带 MID 也从 WorldGridMaterial 派生）。这里改用 EditorLoadingAndSavingUtils 按包保存。

用法（编辑器开着、PIE 停掉）：execfile 本脚本即可。
"""

import unreal

out.clear()

MESH_PATH = "/Game/tank/ztz-88a/ztz88a_skeletal.ztz88a_skeletal"
MAT_MAP = {"MAT_HULL": "/Game/tank/ztz-88a/mat_61.mat_61",
           "MAT_TRACKS": "/Game/tank/ztz-88a/M_TrackScroll.M_TrackScroll"}

mesh = unreal.load_asset(MESH_PATH)
if mesh is None:
    out.append("!! 骨骼网格加载失败")
    raise SystemExit

materials = mesh.get_editor_property("materials")
for i, sm in enumerate(materials):
    slot_name = str(sm.get_editor_property("material_slot_name"))
    target = MAT_MAP.get(slot_name)
    if target is None:
        out.append("槽[%d] %s 无映射，跳过" % (i, slot_name))
        continue
    mat = unreal.load_asset(target)
    sm.set_editor_property("material_interface", mat)
    out.append("槽[%d] %s ← %s" % (i, slot_name, target.split("/")[-1]))
mesh.set_editor_property("materials", materials)

# 真正写盘：按包保存（EditorAssetLibrary 那条路在本工程失效）
pkg = mesh.get_outer()
try:
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([pkg], False)
    out.append("save_packages → %s（包 %s）" % (ok, pkg.get_name()))
except Exception as exc:  # noqa: BLE001
    out.append("save_packages 失败: %s" % exc)

# 回读确认（内存里）
mats = mesh.get_editor_property("materials")
out.append("回读: %s" % [str(m.get_editor_property("material_interface").get_name())
                       for m in mats])
