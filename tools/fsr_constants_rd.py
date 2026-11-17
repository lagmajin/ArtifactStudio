#!/usr/bin/env python3
"""FSR1 の EASU / RCAS 定数生成を CPU 側で再現する R&D ツール（読み取り専用）。

一次情報:
  ArtifactCore/include/FSR/ffx_fsr1.h
    FsrEasuCon()  : L156-202
    FsrRcasCon()  : L662-672
  ArtifactCore/include/FSR/ffx_a.h
    AU1_AF1()     : L141        float -> uint32 bitcast
    AU1_AH2_AF2() : L552        half2 -> uint32 pack
  libs/DiligentEngine/.../SuperResolution/shaders/FSRStructures.fxh:18-26
    FSRAttribs    : 4x uint4 (EASU) + uint4 (RCAS) + float4 (SourceSize) = 96 bytes

使い方:
    python tools/fsr_constants_rd.py --self-test
    python tools/fsr_constants_rd.py --easu 960 540 1920 1080 1920 1080
    python tools/fsr_constants_rd.py --easu 960 540 1920 1080 1920 1080 --rcas 0.5 --cpp
"""

from __future__ import annotations

import argparse
import math
import struct
import sys

U32_MASK = 0xFFFFFFFF


def f32_bits(value: float) -> int:
    """AU1_AF1: float のビット列を符号なし 32bit 整数として返す。"""
    return struct.unpack("<I", struct.pack("<f", float(value)))[0] & U32_MASK


def half_bits(value: float) -> int:
    """AU1_AH1_AF1 相当。float -> IEEE 754 binary16 のビット列。"""
    return struct.unpack("<H", struct.pack("<e", float(value)))[0]


def pack_half2(a: float, b: float) -> int:
    """AU1_AH2_AF2: 下位 16bit に a、上位 16bit に b を詰める。"""
    return (half_bits(a) | (half_bits(b) << 16)) & U32_MASK


def fsr_easu_con(
    input_viewport_x: float,
    input_viewport_y: float,
    input_size_x: float,
    input_size_y: float,
    output_size_x: float,
    output_size_y: float,
) -> tuple[tuple[int, int, int, int], ...]:
    """FsrEasuCon の 4 つの uint4 を返す。引数順は ffx_fsr1.h と同じ。"""
    if min(input_viewport_x, input_viewport_y, input_size_x, input_size_y,
           output_size_x, output_size_y) <= 0.0:
        raise ValueError("all sizes must be positive")

    rcp_out_x = 1.0 / output_size_x
    rcp_out_y = 1.0 / output_size_y
    rcp_in_x = 1.0 / input_size_x
    rcp_in_y = 1.0 / input_size_y

    con0 = (
        f32_bits(input_viewport_x * rcp_out_x),
        f32_bits(input_viewport_y * rcp_out_y),
        f32_bits(0.5 * input_viewport_x * rcp_out_x - 0.5),
        f32_bits(0.5 * input_viewport_y * rcp_out_y - 0.5),
    )
    con1 = (
        f32_bits(rcp_in_x),
        f32_bits(rcp_in_y),
        f32_bits(1.0 * rcp_in_x),
        f32_bits(-1.0 * rcp_in_y),
    )
    con2 = (
        f32_bits(-1.0 * rcp_in_x),
        f32_bits(2.0 * rcp_in_y),
        f32_bits(1.0 * rcp_in_x),
        f32_bits(2.0 * rcp_in_y),
    )
    con3 = (
        f32_bits(0.0 * rcp_in_x),
        f32_bits(4.0 * rcp_in_y),
        0,
        0,
    )
    return con0, con1, con2, con3


def fsr_rcas_con(sharpness_stops: float) -> tuple[int, int, int, int]:
    """FsrRcasCon の uint4 を返す。sharpness は stops (0 = 最大シャープ)。"""
    value = math.pow(2.0, -float(sharpness_stops))
    return (f32_bits(value), pack_half2(value, value), 0, 0)


def fsr_attribs_bytes(
    easu: tuple[tuple[int, int, int, int], ...],
    rcas: tuple[int, int, int, int],
    source_size: tuple[float, float, float, float],
) -> bytes:
    """FSRAttribs (96 bytes) をリトルエンディアンで組み立てる。"""
    all_u32 = [v for vec in easu for v in vec] + list(rcas)
    result = bytearray()
    for value in all_u32:
        result += struct.pack("<I", value & U32_MASK)
    result += struct.pack("<4f", *source_size)
    assert len(result) == 96, len(result)
    return bytes(result)


def u4_text(vec: tuple[int, ...]) -> str:
    return "{ " + ", ".join(f"0x{v:08X}u" for v in vec) + " }"


def self_test() -> int:
    # ffx_fsr1.h L144-147 のコメントにある例。
    easu = fsr_easu_con(1920.0, 1080.0, 3840.0, 2160.0, 2560.0, 1440.0)
    con0 = easu[0]
    # 1920/2560 = 0.75 -> float32 bits 0x3F400000
    assert con0[0] == 0x3F400000, hex(con0[0])
    # 1080/1440 = 0.75
    assert con0[1] == 0x3F400000, hex(con0[1])
    # 0.5*0.75-0.5 = -0.125 -> 0xBE000000
    assert con0[2] == 0xBE000000, hex(con0[2])
    assert con0[3] == 0xBE000000, hex(con0[3])
    # 1/3840 = 0.00026041666... ; 1/2160 = 0.00046296296...
    assert easu[1][0] == f32_bits(1.0 / 3840.0)
    assert easu[1][1] == f32_bits(1.0 / 2160.0)
    assert easu[3][2] == 0 and easu[3][3] == 0

    rcas = fsr_rcas_con(0.0)
    assert rcas[0] == f32_bits(1.0), hex(rcas[0])
    assert rcas[2] == 0 and rcas[3] == 0
    # 1.0 の half は 0x3C00 -> packed 0x3C003C00
    assert rcas[1] == 0x3C003C00, hex(rcas[1])

    # 1 stop = 0.5 -> half 0x3800 -> 0x38003800
    rcas1 = fsr_rcas_con(1.0)
    assert rcas1[0] == f32_bits(0.5)
    assert rcas1[1] == 0x38003800, hex(rcas1[1])

    blob = fsr_attribs_bytes(easu, rcas, (1920.0, 1080.0, 0.0, 0.0))
    assert len(blob) == 96
    assert struct.unpack_from("<I", blob, 0)[0] == 0x3F400000
    assert struct.unpack_from("<f", blob, 80)[0] == 1920.0

    try:
        fsr_easu_con(0.0, 1080.0, 1920.0, 1080.0, 1920.0, 1080.0)
    except ValueError:
        pass
    else:
        raise AssertionError("zero viewport must be rejected")

    print("self-test OK: EASU con0/con1/con3, RCAS stops 0/1, FSRAttribs 96 bytes, input guard")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="run built-in contract checks")
    parser.add_argument("--easu", nargs=6, type=float, metavar=("VX", "VY", "IX", "IY", "OX", "OY"),
                        help="inputViewport X/Y, inputSize X/Y, outputSize X/Y")
    parser.add_argument("--rcas", type=float, default=0.0,
                        help="RCAS sharpness in stops (default: 0 = maximum)")
    parser.add_argument("--cpp", action="store_true", help="emit C++ initializer list")
    parser.add_argument("--json", action="store_true", help="emit JSON")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()
    if not args.easu:
        parser.print_help()
        return 2

    easu = fsr_easu_con(*args.easu)
    rcas = fsr_rcas_con(args.rcas)
    blob = fsr_attribs_bytes(easu, rcas, (*args.easu[0:2], 0.0, 0.0))

    if args.json:
        import json
        print(json.dumps({
            "easu": [[f"0x{v:08X}" for v in vec] for vec in easu],
            "rcas": [f"0x{v:08X}" for v in rcas],
            "attribsBytes": len(blob),
            "attribsHex": blob.hex(),
        }, indent=2))
    elif args.cpp:
        print("struct FSRAttribs { uint4 EASU[4]; uint4 RCAS; float4 SourceSize; };")
        print("// 64 bytes of EASU + 16 bytes of RCAS + 16 bytes of SourceSize = 96 bytes")
        print("static const FSRAttribs kFsrAttribs = {")
        for vec in easu:
            print(f"  {u4_text(vec)},")
        print(f"  {u4_text(rcas)},")
        print(f"  {{ {args.easu[0]}f, {args.easu[1]}f, 0.0f, 0.0f }}")
        print("};")
    else:
        for i, vec in enumerate(easu):
            print(f"EASU Const{i}: {u4_text(vec)}")
        print(f"RCAS Const0: {u4_text(rcas)}")
        print(f"FSRAttribs: {len(blob)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
