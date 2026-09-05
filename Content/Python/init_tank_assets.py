import unreal
import os

def setup_tank_assets():
    print("=== [Tank Setup] Starting automated asset setup ===")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    editor_asset_lib = unreal.EditorAssetLibrary

    content_dir = unreal.Paths.project_content_dir()
    ztz_dir = os.path.join(content_dir, "tank", "ztz-88a")
    dest_path = "/Game/tank/ztz-88a"

    # 1. Models to import
    models = [
        ("ztz88a_hull.obj", "ztz88a_hull"),
        ("ztz88a_tracks.obj", "ztz88a_tracks"),
        ("ztz88a-turret.obj", "ztz88a-turret"),
        ("ztz88a-gun.obj", "ztz88a-gun"),
    ]

    imported_meshes = {}
    for filename, asset_name in models:
        filepath = os.path.join(ztz_dir, filename)
        if not os.path.exists(filepath):
            print(f"Warning: {filepath} does not exist!")
            continue

        task = unreal.AssetImportTask()
        task.filename = filepath
        task.destination_path = dest_path
        task.destination_name = asset_name
        task.replace_existing = True
        task.automated = True
        task.save = True

        task.options = None

        asset_tools.import_asset_tasks([task])
        mesh = editor_asset_lib.load_asset(f"{dest_path}/{asset_name}")
        if mesh:
            imported_meshes[asset_name] = mesh
            print(f"Imported: {dest_path}/{asset_name}")
        else:
            print(f"Failed to load: {dest_path}/{asset_name}")

    # 2. Material setup
    # Hull material (mat_61)
    mat_hull = editor_asset_lib.load_asset(f"{dest_path}/mat_61")
    # Track material (mat_60)
    mat_track = editor_asset_lib.load_asset(f"{dest_path}/mat_60")

    # Ensure mat_track has TrackOffset parameter
    # If mat_track exists, let's configure its material expression graph
    if mat_track and isinstance(mat_track, unreal.Material):
        print("Configuring mat_track with TrackOffset UV offset parameter...")
        tex_track = editor_asset_lib.load_asset(f"{dest_path}/ztz88a-tracks")
        
        # Clear existing expressions if needed, or build UV offset network
        # Let's inspect material expressions
        has_track_offset = False
        for exp in mat_track.get_editor_property("expressions"):
            if isinstance(exp, unreal.MaterialExpressionScalarParameter):
                if str(exp.get_editor_property("parameter_name")) == "TrackOffset":
                    has_track_offset = True
                    break

        if not has_track_offset:
            # Create nodes: TextureCoordinate, ScalarParameter, AppendVector, Add, TextureSample
            tex_coord = unreal.MaterialEditingLibrary.create_material_expression(mat_track, unreal.MaterialExpressionTextureCoordinate, -400, -100)
            param_offset = unreal.MaterialEditingLibrary.create_material_expression(mat_track, unreal.MaterialExpressionScalarParameter, -400, 100)
            param_offset.set_editor_property("parameter_name", "TrackOffset")
            param_offset.set_editor_property("default_value", 0.0)

            const_zero = unreal.MaterialEditingLibrary.create_material_expression(mat_track, unreal.MaterialExpressionConstant, -400, 250)
            const_zero.set_editor_property("r", 0.0)

            # Append Vector: (0.0, TrackOffset) -> scrolls along V
            append_node = unreal.MaterialEditingLibrary.create_material_expression(mat_track, unreal.MaterialExpressionAppendVector, -250, 150)
            unreal.MaterialEditingLibrary.connect_material_expressions(const_zero, "", append_node, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(param_offset, "", append_node, "B")

            # Add node: TexCoord + (0, TrackOffset)
            add_node = unreal.MaterialEditingLibrary.create_material_expression(mat_track, unreal.MaterialExpressionAdd, -100, 0)
            unreal.MaterialEditingLibrary.connect_material_expressions(tex_coord, "", add_node, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(append_node, "", add_node, "B")

            # Texture sample node
            tex_sample = unreal.MaterialEditingLibrary.create_material_expression(mat_track, unreal.MaterialExpressionTextureSample, 100, 0)
            if tex_track:
                tex_sample.set_editor_property("texture", tex_track)
            unreal.MaterialEditingLibrary.connect_material_expressions(add_node, "", tex_sample, "Coordinates")

            # Connect to Base Color of Material
            unreal.MaterialEditingLibrary.connect_material_property(tex_sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
            unreal.MaterialEditingLibrary.recompile_material(mat_track)
            editor_asset_lib.save_asset(f"{dest_path}/mat_60")
            print("Successfully wired TrackOffset UV scroll in mat_60!")

    # 3. Assign materials to the StaticMeshes
    for name, mesh in imported_meshes.items():
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        if name == "ztz88a_tracks":
            if mat_track:
                mesh.set_material(0, mat_track)
                print(f"Assigned mat_60 to {name}")
        else:
            if mat_hull:
                mesh.set_material(0, mat_hull)
                print(f"Assigned mat_61 to {name}")
        editor_asset_lib.save_asset(f"{dest_path}/{name}")

    print("=== [Tank Setup] All assets configured and saved successfully! ===")

if __name__ == "__main__":
    setup_tank_assets()
