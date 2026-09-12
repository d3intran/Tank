"""探测 2：资产注册表取物路径（list_assets 空返回时用）。"""

import unreal

out.clear()

out.append("does_asset_exist:")
for p in ("/Game/tank/inputs/IA_MoveForward", "/Game/tank", "/Game"):
    try:
        out.append("  %-40s %s" % (p, unreal.EditorAssetLibrary.does_asset_exist(p)))
    except Exception as exc:  # noqa: BLE001
        out.append("  %-40s 异常 %s" % (p, exc))

out.append("")
out.append("list_assets 各级：")
for p in ("/Game", "/Game/tank", "/Game/tank/inputs", "/Game/tank/inputs/"):
    try:
        r = unreal.EditorAssetLibrary.list_assets(p, recursive=True, include_folder=False)
        out.append("  %-24s 返回 %d 个" % (p, len(r)))
        for x in list(r)[:6]:
            out.append("      %s" % x)
    except Exception as exc:  # noqa: BLE001
        out.append("  %-24s 异常 %s" % (p, exc))

out.append("")
out.append("AssetRegistry 直查: /Game/tank/inputs")
try:
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = ar.get_assets_by_path("/Game/tank/inputs", recursive=True)
    out.append("  get_assets_by_path 返回 %d 个" % len(assets))
    for d in list(assets)[:8]:
        out.append("      %s" % d.package_name)
except Exception as exc:  # noqa: BLE001
    out.append("  异常 %s" % exc)

out.append("")
out.append("全局 load_asset 是否存在: %s" % hasattr(unreal, "load_asset"))
if hasattr(unreal, "load_asset"):
    try:
        a = unreal.load_asset("/Game/tank/inputs/IA_MoveForward")
        out.append("  unreal.load_asset -> %s" % (a.get_name() if a else None))
    except Exception as exc:  # noqa: BLE001
        out.append("  异常 %s" % exc)
