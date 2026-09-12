import unreal
import os

# 从零引导整个坦克资产管线（最终版）：
# 1. 导入车体（无负重轮版）/ 炮塔 / 炮管 / 完整履带 / 12 负重轮
# 2. 构建 M_TrackScroll（TrackOffset UV 滚动链）并把 mat_60(MIC) 的 parent 指向它
# 3. 构建 M_TrackStatic（轮盘静态材质，取履带贴图）
# 4. 材质指派：履带=mat_60，负重轮/车体/炮塔/炮=mat_61
# 运行：UE 编辑器 > Tools > Execute Python Script。
# OBJ 由 Scripts/tools/split_tank_mesh.py 从 FBX 源生成。

DEST = "/Game/tank/ztz-88a"
TRACKS_TEX = f"{DEST}/ztz88a-tracks"

MESHES = [
    ("ztz88a_hull_body.obj", "ztz88a_hull_body"),
    ("ztz88a-turret.obj", "ztz88a-turret"),
    ("ztz88a-gun.obj", "ztz88a-gun"),
    ("ztz88a_tracks.obj", "ztz88a_tracks_full"),
    ("ztz88a_road_r0.obj", "ztz88a_road_r0"),
    ("ztz88a_road_r1.obj", "ztz88a_road_r1"),
    ("ztz88a_road_r2.obj", "ztz88a_road_r2"),
    ("ztz88a_road_r3.obj", "ztz88a_road_r3"),
    ("ztz88a_road_r4.obj", "ztz88a_road_r4"),
    ("ztz88a_road_r5.obj", "ztz88a_road_r5"),
    ("ztz88a_road_l0.obj", "ztz88a_road_l0"),
    ("ztz88a_road_l1.obj", "ztz88a_road_l1"),
    ("ztz88a_road_l2.obj", "ztz88a_road_l2"),
    ("ztz88a_road_l3.obj", "ztz88a_road_l3"),
    ("ztz88a_road_l4.obj", "ztz88a_road_l4"),
    ("ztz88a_road_l5.obj", "ztz88a_road_l5"),
]


def log(msg):
    unreal.log(f"[TankSetup] {msg}")


def import_meshes():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    ztz_dir = os.path.join(unreal.Paths.project_content_dir(), "tank", "ztz-88a")
    tasks = []
    for filename, asset_name in MESHES:
        filepath = os.path.join(ztz_dir, filename)
        if not os.path.exists(filepath):
            log(f"Warning: {filepath} missing, skipped")
            continue
        task = unreal.AssetImportTask()
        task.filename = filepath
        task.destination_path = DEST
        task.destination_name = asset_name
        task.replace_existing = True
        task.automated = True
        task.save = True
        tasks.append(task)
    if tasks:
        asset_tools.import_asset_tasks(tasks)
        for t in tasks:
            log(f"Imported {t.destination_name}")


def has_track_offset_param(mat):
    for exp in mat.get_editor_property("expressions"):
        if isinstance(exp, unreal.MaterialExpressionScalarParameter):
            if str(exp.get_editor_property("parameter_name")) == "TrackOffset":
                return True
    return False


def build_scroll_material():
    mat = unreal.EditorAssetLibrary.load_asset(f"{DEST}/M_TrackScroll")
    if mat and isinstance(mat, unreal.Material):
        if has_track_offset_param(mat):
            log("M_TrackScroll already wired")
            return mat
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_material(DEST, "M_TrackScroll")

    lib = unreal.MaterialEditingLibrary
    tex_coord = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -600, -100)
    param = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 100)
    param.set_editor_property("parameter_name", "TrackOffset")
    param.set_editor_property("default_value", 0.0)
    const_zero = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -600, 250)
    const_zero.set_editor_property("r", 0.0)
    append_node = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -420, 150)
    lib.connect_material_expressions(const_zero, "", append_node, "A")
    lib.connect_material_expressions(param, "", append_node, "B")
    add_node = lib.create_material_expression(mat, unreal.MaterialExpressionAdd, -250, 0)
    lib.connect_material_expressions(tex_coord, "", add_node, "A")
    lib.connect_material_expressions(append_node, "", add_node, "B")
    sample = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -60, 0)
    tex = unreal.EditorAssetLibrary.load_asset(TRACKS_TEX)
    if tex:
        sample.set_editor_property("texture", tex)
    lib.connect_material_expressions(add_node, "", sample, "Coordinates")
    lib.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(f"{DEST}/M_TrackScroll")
    log("M_TrackScroll wired")
    return mat


def build_static_material():
    mat = unreal.EditorAssetLibrary.load_asset(f"{DEST}/M_TrackStatic")
    if mat and isinstance(mat, unreal.Material) and mat.get_editor_property("expressions"):
        log("M_TrackStatic already wired")
        return mat
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_material(DEST, "M_TrackStatic")
    lib = unreal.MaterialEditingLibrary
    sample = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -200, 0)
    tex = unreal.EditorAssetLibrary.load_asset(TRACKS_TEX)
    if tex:
        sample.set_editor_property("texture", tex)
    lib.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(f"{DEST}/M_TrackStatic")
    log("M_TrackStatic wired")
    return mat


def main():
    import_meshes()
    scroll = build_scroll_material()
    static_mat = build_static_material()
    if not scroll or not static_mat:
        return

    # mat_60 是 OBJ 导入产生的 MaterialInstanceConstant：把 parent 指向滚动材质，
    # C++ 的 SetScalarParameterValue(TrackOffset) 才能真正生效
    mat_60 = unreal.EditorAssetLibrary.load_asset(f"{DEST}/mat_60")
    if mat_60 and not isinstance(mat_60, unreal.Material):
        mat_60.set_editor_property("parent", scroll)
        log("mat_60 reparented to M_TrackScroll")

    def assign(asset_name, material):
        mesh = unreal.EditorAssetLibrary.load_asset(f"{DEST}/{asset_name}")
        if mesh:
            mesh.set_material(0, material)

    assign("ztz88a_tracks_full", mat_60)
    for s in "rl":
        for i in range(6):
            assign(f"ztz88a_road_{s}{i}", static_mat)
    for name in ("ztz88a_hull_body", "ztz88a-turret", "ztz88a-gun"):
        assign(name, unreal.EditorAssetLibrary.load_asset(f"{DEST}/mat_61"))

    unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=True)
    log("Done. C++ binds ztz88a_hull_body / ztz88a_tracks_full / ztz88a_road_* + /Game/tank/inputs input assets.")


if __name__ == "__main__":
    main()
