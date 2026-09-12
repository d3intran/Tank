"""Chaos 载具升级前置探针：盘点 Python API 里可用的物理资产 / 骨骼 / 导入能力。

只读探针，不改任何资产。产出用于决定「物理资产怎么造」的技术路线。
"""

import unreal

out.clear()

# ---- 1. 插件状态 ----
try:
    plugin = unreal.PluginBlueprintLibrary  # noqa: F841  (存在性探测)
except Exception as exc:  # noqa: BLE001
    out.append("PluginBlueprintLibrary 缺失: %s" % exc)

try:
    enabled = unreal.Plugins  # noqa: F841
    out.append("unreal.Plugins 存在")
except Exception as exc:  # noqa: BLE001
    out.append("unreal.Plugins 缺失: %s" % exc)

try:
    mounts = unreal.PluginBlueprintLibrary.get_enabled_plugin_names()
    chaos = [n for n in mounts if "Chaos" in n or "Vehicle" in n]
    out.append("Chaos/Vehicle 相关插件: %s" % chaos)
except Exception as exc:  # noqa: BLE001
    out.append("get_enabled_plugin_names 失败: %s" % exc)

# ---- 2. 几何元素类（造物理资产刚体形状的前提）----
for name in ("KBoxElem", "KSphereElem", "KSphylElem", "KConvexElem", "KAggregateGeom",
             "SkeletalBodySetup", "BodySetup", "PhysicsAsset", "PhysicsAssetFactory",
             "PhysicsConstraintTemplate", "ConstraintInstance", "SkeletalMeshEditorSubsystem",
             "StaticMeshEditorSubsystem", "GEditor"):
    out.append("unreal.%s: %s" % (name, hasattr(unreal, name)))

# ---- 3. PhysicsAsset 可用成员 ----
if hasattr(unreal, "PhysicsAsset"):
    members = [m for m in dir(unreal.PhysicsAsset) if not m.startswith("_")]
    out.append("PhysicsAsset members: %s" % members)

if hasattr(unreal, "SkeletalBodySetup"):
    members = [m for m in dir(unreal.SkeletalBodySetup) if not m.startswith("_")]
    out.append("SkeletalBodySetup members: %s" % members)

if hasattr(unreal, "KAggregateGeom"):
    props = [m for m in dir(unreal.KAggregateGeom) if not m.startswith("_")]
    out.append("KAggregateGeom members: %s" % props)

if hasattr(unreal, "KBoxElem"):
    props = [m for m in dir(unreal.KBoxElem) if not m.startswith("_")]
    out.append("KBoxElem members: %s" % props)

# ---- 4. 骨骼网格 / 骨架读取能力 ----
for name in ("SkeletalMesh", "Skeleton", "ReferenceSkeleton", "FbxImportUI", "AssetImportTask",
             "FBXImportType", "FBXImportContentType"):
    out.append("unreal.%s: %s" % (name, hasattr(unreal, name)))

if hasattr(unreal, "Skeleton"):
    out.append("Skeleton members: %s" % [m for m in dir(unreal.Skeleton) if not m.startswith("_")])

if hasattr(unreal, "SkeletalMesh"):
    out.append("SkeletalMesh members: %s" % [m for m in dir(unreal.SkeletalMesh) if not m.startswith("_")])

if hasattr(unreal, "FbxImportUI"):
    props = [m for m in dir(unreal.FbxImportUI) if not m.startswith("_")]
    out.append("FbxImportUI members: %s" % props)

# ---- 5. 现有资产盘点：物理资产 / 骨骼网格 ----
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for cls_path in ("/Script/Engine.PhysicsAsset", "/Script/Engine.SkeletalMesh", "/Script/Engine.Skeleton",
                 "/Script/Engine.AnimBlueprint"):
    assets = ar.get_assets_by_class(unreal.TopLevelAssetPath(cls_path) if hasattr(unreal, "TopLevelAssetPath") else None, True)
    names = [str(a.package_name) for a in assets] if assets else []
    out.append("%s 资产数=%d 例=%s" % (cls_path, len(names), names[:5]))

# ---- 6. 引擎模板里有没有可参考的载具资产 ----
try:
    import os
    tmpl = r"E:\UE_5.8\Engine\Templates"
    out.append("Templates 目录: %s" % (os.listdir(tmpl) if os.path.isdir(tmpl) else "不存在"))
except Exception as exc:  # noqa: BLE001
    out.append("Templates 枚举失败: %s" % exc)
