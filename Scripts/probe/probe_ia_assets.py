"""探测输入动作资产的可用路径形式（Python 加载路径对不上时用）。"""

import unreal

out.clear()

root = "/Game/tank/inputs"
out.append("list_assets(%s) 直接列：" % root)
try:
    for p in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
        out.append("  %s" % p)
except Exception as exc:  # noqa: BLE001
    out.append("  list_assets 失败: %s" % exc)

out.append("")
out.append("逐种路径形式尝试 load_asset：")
for form in (
    "/Game/tank/inputs/IA_MoveForward.IA_MoveForward",
    "/Game/tank/inputs/IA_MoveForward",
    "/Game/Tank/inputs/IA_MoveForward.IA_MoveForward",
    "/Game/tank/Inputs/IA_MoveForward.IA_MoveForward",
):
    try:
        a = unreal.EditorAssetLibrary.load_asset(form)
    except Exception as exc:  # noqa: BLE001
        a = None
        out.append("  %-50s 异常 %s" % (form, exc))
        continue
    out.append("  %-50s -> %s" % (form, a.get_name() if a else "None"))
