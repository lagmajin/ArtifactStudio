"""Author tiny, redistributable FBX material regression fixtures (stdlib only)."""

from pathlib import Path
import struct
import zlib


def png(rgb):
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data)))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes([0, *rgb]))) + chunk(b"IEND", b""))


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
    for name, data in files.items():
        (output / name).write_text(data, encoding="utf-8", newline="\n")
    external_data = cube(phong,
        texture(2003, "Z:/unavailable/source/Textures/external-base.png",
                "external-base.png"),
        'C: "OP",2003,1002,"DiffuseColor"')
    (external_output / "stale-source-path.fbx").write_text(
        external_data, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    generate(Path(__file__).resolve().parents[2] / "tests/fixtures/fbx-materials")
