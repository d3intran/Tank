import os
import struct
import zlib

SCALE = 0.5  # Model is authored in half-centimeters; 0.5 brings it to 1:1 true Unreal Engine cm

def parse_node(buf, offset):
    if offset + 13 > len(buf):
        return None
    end_offset, prop_count, prop_len, name_len = struct.unpack('<IIIB', buf[offset:offset+13])
    if end_offset == 0:
        return None
    name = buf[offset+13:offset+13+name_len].decode('ascii', errors='ignore')
    return end_offset, prop_count, prop_len, name, offset+13+name_len

def read_array_from_buf(buf, offset):
    type_char = chr(buf[offset])
    offset += 1
    arr_len, enc, comp_len = struct.unpack('<III', buf[offset:offset+12])
    offset += 12
    if enc == 1:
        raw = zlib.decompress(buf[offset:offset+comp_len])
        offset += comp_len
    else:
        byte_len = arr_len * {'d': 8, 'i': 4, 'f': 4}[type_char]
        raw = buf[offset:offset+byte_len]
        offset += byte_len
    fmt = '<' + str(arr_len) + {'d': 'd', 'i': 'i', 'f': 'f'}[type_char]
    return struct.unpack(fmt, raw), offset

def parse_geometry(data, geom_name):
    g_pos = data.find(geom_name.encode('ascii') + b'\x00\x01Geometry')
    if g_pos == -1:
        raise ValueError(f"{geom_name} Geometry not found in FBX")
    
    node_start = data.rfind(b'Geometry', 0, g_pos) - 13
    end_off, p_cnt, p_len, name, cur = parse_node(data, node_start)
    cur = cur + p_len

    verts = None
    indices = None
    normals = None
    normals_index = None
    uvs = None
    uv_index = None

    while cur < end_off:
        res = parse_node(data, cur)
        if not res: break
        c_end, c_pcnt, c_plen, c_name, c_pstart = res
        if c_name == 'Vertices':
            verts, _ = read_array_from_buf(data, c_pstart)
        elif c_name == 'PolygonVertexIndex':
            indices, _ = read_array_from_buf(data, c_pstart)
        elif c_name == 'LayerElementNormal':
            n_cur = c_pstart + c_plen
            while n_cur < c_end:
                n_sub = parse_node(data, n_cur)
                if not n_sub: break
                if n_sub[3] == 'Normals':
                    normals, _ = read_array_from_buf(data, n_sub[4])
                elif n_sub[3] == 'NormalsIndex':
                    normals_index, _ = read_array_from_buf(data, n_sub[4])
                n_cur = n_sub[0]
        elif c_name == 'LayerElementUV' and uvs is None:
            u_cur = c_pstart + c_plen
            while u_cur < c_end:
                u_sub = parse_node(data, u_cur)
                if not u_sub: break
                if u_sub[3] == 'UV':
                    uvs, _ = read_array_from_buf(data, u_sub[4])
                elif u_sub[3] == 'UVIndex':
                    uv_index, _ = read_array_from_buf(data, u_sub[4])
                u_cur = u_sub[0]
        cur = c_end

    return verts, indices, normals, normals_index, uvs, uv_index

def export_polygons_to_obj(filepath, object_name, material_name, verts, normals, normals_index, uvs, uv_index, target_polys, pivot=(0.0, 0.0, 0.0)):
    px, py, pz = pivot
    with open(filepath, 'w', encoding='utf-8') as out:
        out.write(f"# Exported by split_tank_mesh.py\n")
        out.write(f"o {object_name}\n")
        out.write(f"usemtl {material_name}\n")
        
        # 1-based vertex map
        v_map = {}
        for poly_verts, _ in target_polys:
            for v in poly_verts:
                if v not in v_map:
                    v_map[v] = len(v_map) + 1
                    x = (verts[v*3] - px) * SCALE
                    y = (verts[v*3+1] - py) * SCALE
                    z = (verts[v*3+2] - pz) * SCALE
                    out.write(f"v {x:.4f} {y:.4f} {z:.4f}\n")

        vn_map = {}
        for _, corners in target_polys:
            for c in corners:
                n_idx = normals_index[c] if normals_index else c
                if n_idx not in vn_map:
                    vn_map[n_idx] = len(vn_map) + 1
                    nx = normals[n_idx*3]
                    ny = normals[n_idx*3+1]
                    nz = normals[n_idx*3+2]
                    out.write(f"vn {nx:.4f} {ny:.4f} {nz:.4f}\n")

        vt_map = {}
        for _, corners in target_polys:
            for c in corners:
                u_idx = uv_index[c] if uv_index else c
                if u_idx not in vt_map:
                    vt_map[u_idx] = len(vt_map) + 1
                    u = uvs[u_idx*2]
                    v = uvs[u_idx*2+1]
                    out.write(f"vt {u:.6f} {v:.6f}\n")

        for poly_verts, corners in target_polys:
            face_tokens = []
            for v, c in zip(poly_verts, corners):
                v_id = v_map[v]
                n_id = vn_map[normals_index[c] if normals_index else c]
                t_id = vt_map[uv_index[c] if uv_index else c]
                face_tokens.append(f"{v_id}/{t_id}/{n_id}")
            out.write(f"f {' '.join(face_tokens)}\n")

    print(f"Exported {os.path.basename(filepath)}: {len(v_map)} verts, {len(target_polys)} faces (Pivot: {pivot}, Scale: {SCALE})")

def split_tank(fbx_path, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    with open(fbx_path, 'rb') as f:
        data = f.read()

    # 1. Parse mesh_317 (Hull, Turret, Gun)
    v317, i317, n317, ni317, u317, ui317 = parse_geometry(data, 'mesh_317')
    num_verts_317 = len(v317) // 3
    print(f"mesh_317 loaded: {num_verts_317} verts, {len(i317)} indices")

    # Group polygons
    polygons_317 = []
    cur_v = []
    cur_corners = []
    for corner_idx, idx in enumerate(i317):
        if idx < 0:
            cur_v.append(~idx)
            cur_corners.append(corner_idx)
            polygons_317.append((cur_v, cur_corners))
            cur_v = []
            cur_corners = []
        else:
            cur_v.append(idx)
            cur_corners.append(corner_idx)

    # Union-Find for connected components in mesh_317
    parent = list(range(num_verts_317))
    def find(i):
        path = []
        while parent[i] != i:
            path.append(i)
            i = parent[i]
        for p in path: parent[p] = i
        return i

    def union(i, j):
        ri, rj = find(i), find(j)
        if ri != rj: parent[ri] = rj

    for poly_verts, _ in polygons_317:
        p0 = poly_verts[0]
        for p in poly_verts[1:]:
            union(p0, p)

    components = {}
    for v_idx in range(num_verts_317):
        r = find(v_idx)
        components.setdefault(r, []).append(v_idx)

    gun_roots = set()
    turret_roots = set()
    hull_roots = set()

    for root, v_indices in components.items():
        xs = [v317[v*3] for v in v_indices]
        ys = [v317[v*3+1] for v in v_indices]
        zs = [v317[v*3+2] for v in v_indices]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        min_z, max_z = min(zs), max(zs)
        cx = (min_x + max_x) / 2.0
        cy = (min_y + max_y) / 2.0
        cz = (min_z + max_z) / 2.0

        if (min_x >= 200 and abs(cy) < 50 and 310 <= cz <= 400) or (max_x > 350 and abs(cy) < 60 and 310 <= cz <= 400):
            gun_roots.add(root)
        elif min_z >= 290.8 and min_x >= -400.0 and max_x <= 310.0 and min_y >= -315.0 and max_y <= 315.0:
            turret_roots.add(root)
        else:
            hull_roots.add(root)

    print(f"mesh_317 classification: Hull={len(hull_roots)}, Turret={len(turret_roots)}, Gun={len(gun_roots)} components")

    # Turret rotation pivot in raw coords: X=-5.0, Y=0.0, Z=291.0
    turret_pivot = (-5.0, 0.0, 291.0)
    # Gun pitch pivot in raw coords: X=335.0, Y=0.0, Z=353.0
    gun_pivot = (335.0, 0.0, 353.0)

    # 1. Export ztz88a_hull.obj
    hull_polys = [p for p in polygons_317 if find(p[0][0]) in hull_roots]
    export_polygons_to_obj(os.path.join(output_dir, "ztz88a_hull.obj"), "ztz88a_hull", "mat_61",
                           v317, n317, ni317, u317, ui317, hull_polys, pivot=(0.0, 0.0, 0.0))

    # 2. Export ztz88a-turret.obj
    turret_polys = [p for p in polygons_317 if find(p[0][0]) in turret_roots]
    export_polygons_to_obj(os.path.join(output_dir, "ztz88a-turret.obj"), "ztz88a_turret", "mat_61",
                           v317, n317, ni317, u317, ui317, turret_polys, pivot=turret_pivot)

    # 3. Export ztz88a-gun.obj
    gun_polys = [p for p in polygons_317 if find(p[0][0]) in gun_roots]
    export_polygons_to_obj(os.path.join(output_dir, "ztz88a-gun.obj"), "ztz88a_gun", "mat_61",
                           v317, n317, ni317, u317, ui317, gun_polys, pivot=gun_pivot)

    # 4. Parse & Export mesh_318 (Tracks)
    v318, i318, n318, ni318, u318, ui318 = parse_geometry(data, 'mesh_318')
    num_verts_318 = len(v318) // 3
    print(f"mesh_318 loaded: {num_verts_318} verts, {len(i318)} indices")

    polygons_318 = []
    cur_v = []
    cur_corners = []
    for corner_idx, idx in enumerate(i318):
        if idx < 0:
            cur_v.append(~idx)
            cur_corners.append(corner_idx)
            polygons_318.append((cur_v, cur_corners))
            cur_v = []
            cur_corners = []
        else:
            cur_v.append(idx)
            cur_corners.append(corner_idx)

    # Export ztz88a_tracks.obj (with material mat_60, exact same origin 0,0,0)
    export_polygons_to_obj(os.path.join(output_dir, "ztz88a_tracks.obj"), "ztz88a_tracks", "mat_60",
                           v318, n318, ni318, u318, ui318, polygons_318, pivot=(0.0, 0.0, 0.0))

if __name__ == '__main__':
    fbx = r"E:\UE\Assets\tank\source\ztz-88a\ztz-88a.fbx"
    out = r"E:\UE\Tank\Content\tank\ztz-88a"
    split_tank(fbx, out)
