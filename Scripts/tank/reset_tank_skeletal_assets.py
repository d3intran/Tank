"""删掉骨骼网格/骨架资产（含 v2 试验品），让后续导入从零建骨架。

为什么必须重建而不能只重导：UE 重导骨骼网格会**复用已有骨架资产**，
骨架的 ref pose（含骨缩放）不会更新 —— 旧骨架带着 FBX 单位换算的 ×100 缩放，
而物理资产要用骨架的 ref pose 建碰撞形状（×50 的元凶）。
删掉后重新导入即可拿到干净的骨架（骨缩放全 1）。

注意顺序：先删骨骼网格（引用方），再删骨架（被引用方），否则删除会被引用挡住。
"""

import unreal

out.clear()

PATHS = [
    "/Game/tank/ztz-88a/ztz88a_skeletal",
    "/Game/tank/ztz-88a/ztz88a_skeletal_Skeleton",
    "/Game/tank/ztz-88a/ztz88a_skeletal_v2",
    "/Game/tank/ztz-88a/ztz88a_skeletal_v2_Skeleton",
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
del_fn = getattr(unreal.EditorAssetLibrary, "delete_asset", None)
if del_fn is None:
    out.append("!! EditorAssetLibrary.delete_asset 不可用")
for path in PATHS:
    try:
        ok = del_fn(path)
        out.append("删除 %s → %s" % (path, "成功" if ok else "失败（可能被引用挡住）"))
    except Exception as exc:  # noqa: BLE001
        out.append("删除 %s 抛错: %s" % (path, str(exc)[:120]))

remaining = [p for p in PATHS if unreal.load_asset(p) is not None]
out.append("残留: %s" % (remaining if remaining else "无"))
