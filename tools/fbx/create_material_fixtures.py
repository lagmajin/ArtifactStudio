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


def cube(material, textures, connections):
    uv = ",".join(["0,0,1,0,1,1,0,1"] * 6)
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
    {textures}
}}
Connections: {{
    C: "OO",1000,1001
    C: "OO",1001,0
    C: "OO",1002,1001
    {connections}
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
    files = {
        "diffuse-factor.fbx": cube(phong, texture(2000, "factor.png"),
            'C: "OP",2000,1002,"DiffuseFactor"'),
        "separate-pbr.fbx": cube(pbr.replace("METAL", "1").replace("ROUGH", "1"),
            texture(2001, "metallic.png") + "\n" + texture(2002, "roughness.png"),
            'C: "OP",2001,1002,"3dsMax|main|metalness"\n'
            '    C: "OP",2002,1002,"3dsMax|main|roughness"'),
        "scalar-reference.fbx": cube(pbr.replace("METAL", "0.2").replace("ROUGH", "0.8"), "", ""),
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
