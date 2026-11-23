"""Author tiny, redistributable FBX material regression fixtures (stdlib only)."""

from pathlib import Path
import hashlib
import json
import math
import struct
import zlib


def png(rgb):
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data)))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes([0, *rgb]))) + chunk(b"IEND", b""))


def png_image(width, height, pixel):
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data)))
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel(x, y))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))


def fabric_pillow_fbx():
    stacks, slices = 32, 64
    vertices = []
    vertex_normals = []
    polygons = []
    uvs = []
    pieces = (
        ((0.0, 0.48, 0.0), (0.96, 0.16, 0.58)),
        ((-0.415, 0.71, -0.08), (0.43, 0.22, 0.53)),
        ((0.415, 0.71, -0.08), (0.43, 0.22, 0.53)),
        ((0.0, 1.14, 0.34), (0.88, 0.52, 0.19)),
        ((-0.84, 0.77, 0.0), (0.20, 0.43, 0.58)),
        ((0.84, 0.77, 0.0), (0.20, 0.43, 0.58)),
    )
    for center, radius in pieces:
        first_vertex = len(vertices)
        for stack in range(stacks + 1):
            theta = math.pi * stack / stacks
            st, ct = math.sin(theta), math.cos(theta)
            for slice_index in range(slices + 1):
                phi = 2.0 * math.pi * slice_index / slices
                cp, sp = math.cos(phi), math.sin(phi)
                power = 0.42
                x = (1.0 if cp >= 0 else -1.0) * abs(cp) ** power * st
                z = (1.0 if sp >= 0 else -1.0) * abs(sp) ** power * st
                y = (1.0 if ct >= 0 else -1.0) * abs(ct) ** power
                vertices.append((center[0] + x * radius[0],
                                 center[1] + y * radius[1],
                                 center[2] + z * radius[2]))
                power = 2.0 / 0.42
                normal = [
                    math.copysign(abs(x) ** (power - 1.0) / radius[0], x),
                    math.copysign(abs(y) ** (power - 1.0) / radius[1], y),
                    math.copysign(abs(z) ** (power - 1.0) / radius[2], z),
                ]
                normal_length = math.sqrt(sum(component * component
                                              for component in normal))
                vertex_normals.append(tuple(component / normal_length
                                            for component in normal))
        for stack in range(stacks):
            for slice_index in range(slices):
                a = first_vertex + stack * (slices + 1) + slice_index
                b = a + slices + 1
                uv0 = (slice_index / slices, stack / stacks)
                uv1 = ((slice_index + 1) / slices, stack / stacks)
                uv2 = ((slice_index + 1) / slices, (stack + 1) / stacks)
                uv3 = (slice_index / slices, (stack + 1) / stacks)
                polygons.extend(((a, a + 1, b + 1), (a, b + 1, b)))
                uvs.extend((uv0, uv1, uv2, uv0, uv2, uv3))
    polygon_indices = []
    polygon_normals = []
    for a, b, c in polygons:
        polygon_indices.extend((a, b, -(c + 1)))
        polygon_normals.extend((*vertex_normals[a], *vertex_normals[b],
                                *vertex_normals[c]))
    vertex_text = ",".join(f"{v:.7g}" for point in vertices for v in point)
    index_text = ",".join(map(str, polygon_indices))
    normal_text = ",".join(f"{v:.7g}" for v in polygon_normals)
    uv_text = ",".join(f"{v:.7g}" for point in uvs for v in point)
    pbr = '''ShadingModel: "unknown"
        Properties70: {
            P: "3dsMax|ClassIDa", "int", "Integer", "",3490651648
            P: "3dsMax|ClassIDb", "int", "Integer", "",3195528448
            P: "3dsMax|main|base_color", "Color", "", "A",1,1,1
            P: "3dsMax|main|metalness", "Number", "", "A",0
            P: "3dsMax|main|roughness", "Number", "", "A",0.82
        }'''
    texture_blocks = "\n".join(
        texture(2100 + i, name) for i, name in enumerate(
            ("fabric-base.png", "fabric-roughness.png", "fabric-normal.png")))
    return f'''; FBX 7.4.0 project-authored upholstered sofa look-dev fixture
FBXHeaderExtension: {{ FBXHeaderVersion: 1003 FBXVersion: 7400 }}
Objects: {{
 Geometry: 1000, "Geometry::FabricPillow", "Mesh" {{
  Vertices: *{len(vertices) * 3} {{ a: {vertex_text} }}
  PolygonVertexIndex: *{len(polygon_indices)} {{ a: {index_text} }}
  LayerElementNormal: 0 {{ Version: 101 Name: "" MappingInformationType: "ByPolygonVertex" ReferenceInformationType: "Direct" Normals: *{len(polygon_normals)} {{ a: {normal_text} }} }}
  LayerElementUV: 0 {{ Version: 101 Name: "UV0" MappingInformationType: "ByPolygonVertex" ReferenceInformationType: "Direct" UV: *{len(uvs) * 2} {{ a: {uv_text} }} }}
  LayerElementMaterial: 0 {{ Version: 101 MappingInformationType: "AllSame" ReferenceInformationType: "IndexToDirect" Materials: *1 {{ a: 0 }} }}
  Layer: 0 {{ Version: 100 LayerElement: {{ Type: "LayerElementNormal" TypedIndex: 0 }} LayerElement: {{ Type: "LayerElementUV" TypedIndex: 0 }} LayerElement: {{ Type: "LayerElementMaterial" TypedIndex: 0 }} }}
 }}
 Model: 1001, "Model::FabricPillow", "Mesh" {{ Version: 232 }}
 Material: 1002, "Material::WarmWovenLinen", "" {{ Version: 102 {pbr} }}
 {texture_blocks}
}}
Connections: {{
 C: "OO",1000,1001 C: "OO",1001,0 C: "OO",1002,1001
 C: "OP",2100,1002,"3dsMax|main|base_color"
 C: "OP",2101,1002,"3dsMax|main|roughness"
 C: "OP",2102,1002,"NormalMap"
}}
'''


def cube(material, textures, connections, objects="", links=""):
    uv = ",".join(["0,0,1,0,1,1,0,1"] * 6)
    extra_objects = f"\n    {objects}" if objects else ""
    extra_links = f"\n    {links}" if links else ""
    return f'''; FBX 7.4.0 project-authored material test cube
FBXHeaderExtension: {{
    FBXHeaderVersion: 1003
    FBXVersion: 7400
}}
Objects: {{
    Geometry: 1000, "Geometry::Cube", "Mesh" {{
        Vertices: *24 {{ a: -1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1 }}
        PolygonVertexIndex: *24 {{ a: 4,5,6,-8,0,3,2,-2,0,4,7,-4,1,2,6,-6,0,1,5,-5,3,7,6,-3 }}
        LayerElementUV: 0 {{
            Version: 101
            Name: "UV0"
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            UV: *48 {{ a: {uv} }}
        }}
        LayerElementMaterial: 0 {{
            Version: 101
            MappingInformationType: "AllSame"
            ReferenceInformationType: "IndexToDirect"
            Materials: *1 {{ a: 0 }}
        }}
        Layer: 0 {{
            Version: 100
            LayerElement: {{ Type: "LayerElementUV"
                TypedIndex: 0 }}
            LayerElement: {{ Type: "LayerElementMaterial"
                TypedIndex: 0 }}
        }}
    }}
    Model: 1001, "Model::Cube", "Mesh" {{
        Version: 232
    }}
    Material: 1002, "Material::Test", "" {{
        Version: 102
        {material}
    }}
    {textures}{extra_objects}
}}
Connections: {{
    C: "OO",1000,1001
    C: "OO",1001,0
    C: "OO",1002,1001
    {connections}{extra_links}
}}
'''.replace("    \n", "\n")


def texture(index, filename, relative_filename=None):
    if relative_filename is None:
        relative_filename = filename
    return f'''Texture: {index}, "Texture::{filename}", "" {{
        Type: "TextureVideoClip"
        Version: 202
        FileName: "{filename}"
        RelativeFilename: "{relative_filename}"
    }}'''


def generate(output):
    output.mkdir(parents=True, exist_ok=True)
    for name, color in [("factor.png", (255, 255, 255)),
                        ("metallic.png", (51, 242, 217)),
                        ("roughness.png", (204, 26, 128))]:
        (output / name).write_bytes(png(color))
    external_output = output / "external" / "FBX"
    external_output.mkdir(parents=True, exist_ok=True)
    external_texture_directory = output / "external" / "Textures"
    external_texture_directory.mkdir(parents=True, exist_ok=True)
    (external_texture_directory / "external-base.png").write_bytes(
        png((38, 121, 203)))
    phong = '''ShadingModel: "phong"
        Properties70: {
            P: "DiffuseColor", "Color", "", "A",0.6,0.2,0.05
            P: "DiffuseFactor", "Number", "", "A",1
            P: "ShininessExponent", "Number", "", "A",16
        }'''
    pbr = '''ShadingModel: "unknown"
        Properties70: {
            P: "3dsMax|ClassIDa", "int", "Integer", "",CLASS_A
            P: "3dsMax|ClassIDb", "int", "Integer", "",CLASS_B
            P: "3dsMax|main|base_color", "Color", "", "A",0.6,0.2,0.05
            P: "3dsMax|main|metalness", "Number", "", "A",METAL
            P: "3dsMax|main|roughness", "Number", "", "A",ROUGH
        }'''
    # IDs come from ufbx's published 3ds Max PBR Metal/Rough shader mapping.
    pbr = pbr.replace("CLASS_A", str(0xd00f1e00)).replace("CLASS_B", str(0xbe77e500))
    # Property names match the ones ufbx reads in `ufbxi_update_camera` and
    # `ufbxi_update_light` (CameraProjectionType / FieldOfView / NearPlane /
    # FarPlane / LightType / Color / Intensity / OuterAngle / CastShadows).
    camera_and_light_objects = '''NodeAttribute: 3000, "NodeAttribute::CameraAttribute", "Camera" {
        Version: 100
        Properties70: {
            P: "CameraProjectionType", "enum", "", "",0
            P: "FieldOfView", "Number", "", "A",45
            P: "NearPlane", "Number", "", "A",1
            P: "FarPlane", "Number", "", "A",1000
        }
    }
    Model: 3001, "Model::TestCamera", "Camera" {
        Version: 232
        Properties70: {
            P: "Lcl Translation", "Lcl Translation", "", "A",10,20,30
        }
    }
    NodeAttribute: 3100, "NodeAttribute::LightAttribute", "Light" {
        Version: 100
        Properties70: {
            P: "LightType", "enum", "", "",2
            P: "Color", "Color", "", "A",1,0.8,0.6
            P: "Intensity", "Number", "", "A",150
            P: "OuterAngle", "Number", "", "A",40
            P: "CastShadows", "bool", "", "",1
        }
    }
    Model: 3101, "Model::TestLight", "Light" {
        Version: 232
        Properties70: {
            P: "Lcl Translation", "Lcl Translation", "", "A",-5,8,12
        }
    }'''
    camera_and_light_links = '''C: "OO",3001,0
    C: "OO",3000,3001
    C: "OO",3101,0
    C: "OO",3100,3101'''
    files = {
        "diffuse-factor.fbx": cube(phong, texture(2000, "factor.png"),
            'C: "OP",2000,1002,"DiffuseFactor"'),
        "diffuse-factor-weight.fbx": cube(
            phong.replace('"DiffuseFactor", "Number", "", "A",1',
                          '"DiffuseFactor", "Number", "", "A",0.5'), "", ""),
        "separate-pbr.fbx": cube(pbr.replace("METAL", "1").replace("ROUGH", "1"),
            texture(2001, "metallic.png") + "\n" + texture(2002, "roughness.png"),
            'C: "OP",2001,1002,"3dsMax|main|metalness"\n'
            '    C: "OP",2002,1002,"3dsMax|main|roughness"'),
        "scalar-reference.fbx": cube(pbr.replace("METAL", "0.2").replace("ROUGH", "0.8"), "", ""),
        "camera-light.fbx": cube(phong, "", "", camera_and_light_objects,
                                 camera_and_light_links),
    }
    blue_material = (pbr.replace("METAL", "1").replace("ROUGH", "1")
                    .replace('0.6,0.2,0.05', '0.05,0.2,0.6'))
    blue_object = f'''Material: 1003, "Material::Blue", "" {{
        Version: 102
        {blue_material}
    }}'''
    multi_material = cube(
        pbr.replace("METAL", "0").replace("ROUGH", "0.5"), "", "",
        blue_object, 'C: "OO",1003,1001')
    multi_material = multi_material.replace(
        'MappingInformationType: "AllSame"',
        'MappingInformationType: "ByPolygon"')
    multi_material = multi_material.replace(
        'Materials: *1 { a: 0 }',
        'Materials: *6 { a: 0,0,0,1,1,1 }')
    files["multi-material-pbr.fbx"] = multi_material
    for name, data in files.items():
        (output / name).write_text(data, encoding="utf-8", newline="\n")
    external_data = cube(phong,
        texture(2003, "Z:/unavailable/source/Textures/external-base.png",
                "external-base.png"),
        'C: "OP",2003,1002,"DiffuseColor"')
    (external_output / "stale-source-path.fbx").write_text(
        external_data, encoding="utf-8", newline="\n")
    fabric_dir = output / "fabric-pillow"
    fabric_dir.mkdir(parents=True, exist_ok=True)
    size = 512
    def weave(x, y):
        warp = (x % 16) < 8
        weft = (y % 16) < 8
        variation = 1 if warp != weft else 0
        fiber = 1 if ((x * 17 + y * 29) % 11) < 3 else 0
        return tuple(max(0, min(255, c + variation + fiber))
                     for c in (95, 132, 125))
    def roughness(x, y):
        crossing = 221 if ((x % 16) < 8) != ((y % 16) < 8) else 232
        return crossing, crossing, crossing
    def normal(x, y):
        # Periodic interlaced-fiber height derivative, tangent-space +Z.
        import math as _math
        phase_x = 2.0 * _math.pi * x / 16.0
        phase_y = 2.0 * _math.pi * y / 16.0
        dx = 0.32 * _math.cos(phase_x) * (0.3 + 0.7 * _math.cos(phase_y))
        dy = 0.32 * _math.cos(phase_y) * (0.3 + 0.7 * _math.cos(phase_x))
        length = _math.sqrt(dx * dx + dy * dy + 1.0)
        return tuple(int(round(127.5 * (v / length + 1.0)))
                     for v in (-dx, -dy, 1.0))
    for name, pixel in (("fabric-base.png", weave),
                        ("fabric-roughness.png", roughness),
                        ("fabric-normal.png", normal)):
        (fabric_dir / name).write_bytes(png_image(size, size, pixel))
    (fabric_dir / "fabric-pillow.fbx").write_text(
        fabric_pillow_fbx(), encoding="utf-8", newline="\n")
    manifest_files = {}
    for path in sorted(fabric_dir.iterdir()):
        if path.is_file() and path.name != "manifest.json":
            manifest_files[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    (fabric_dir / "manifest.json").write_text(json.dumps({
        "origin": "project-authored procedural fixture; no downloaded assets",
        "mesh": "six-part upholstered sofa with divided seat cushion; FBX 7.4.0 ASCII",
        "textureSize": [size, size],
        "textures": {
            "fabric-base.png": "sRGB deep teal woven cloth albedo",
            "fabric-roughness.png": "linear grayscale roughness",
            "fabric-normal.png": "linear tangent-space normal map"
        },
        "sha256": manifest_files
    }, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    generate(Path(__file__).resolve().parents[2] / "tests/fixtures/fbx-materials")
