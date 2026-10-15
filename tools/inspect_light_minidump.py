"""Read exception registers and selected memory from a Windows x64 minidump."""
import argparse
import json
from pathlib import Path
import struct


def inspect(path, addresses):
    data = path.read_bytes()
    if data[:4] != b"MDMP":
        raise ValueError("Not a Windows minidump")
    count, directory = struct.unpack_from("<II", data, 8)
    streams = {}
    for index in range(count):
        kind, size, offset = struct.unpack_from("<III", data, directory + index * 12)
        if offset + size > len(data):
            raise ValueError("Truncated stream")
        streams[kind] = (size, offset)
    if 6 not in streams or 7 not in streams:
        raise ValueError("Dump has no exception or system information")
    if struct.unpack_from("<H", data, streams[7][1])[0] != 9:
        raise ValueError("Only AMD64 contexts are supported")
    offset = streams[6][1]
    thread = struct.unpack_from("<I", data, offset)[0]
    code, flags, nested, fault, parameters = struct.unpack_from("<IIQQI", data, offset + 8)
    context_size, context_offset = struct.unpack_from("<II", data, offset + 160)
    if context_size < 256 or context_offset + context_size > len(data):
        raise ValueError("Truncated AMD64 context")
    register_names = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip")
    registers = {name: struct.unpack_from("<Q", data, context_offset + 120 + index * 8)[0]
                 for index, name in enumerate(register_names)}
    ranges = []
    if 5 in streams:
        offset = streams[5][1]
        count = struct.unpack_from("<I", data, offset)[0]
        ranges = [struct.unpack_from("<QII", data, offset + 4 + index * 16)
                  for index in range(count)]

    def memory(address, length):
        for start, size, file_offset in ranges:
            if start <= address < start + size:
                available = min(length, start + size - address)
                begin = file_offset + address - start
                return data[begin:begin + available]
        return b""

    stack = memory(registers["rsp"], 512)
    return {
        "dump": str(path.resolve()), "bytes": len(data), "threadId": thread,
        "exceptionCode": hex(code), "exceptionAddress": hex(fault),
        "exceptionParameters": [hex(struct.unpack_from("<Q", data, streams[6][1] + 40 + i * 8)[0])
                                for i in range(min(parameters, 15))],
        "registers": {name: hex(value) for name, value in registers.items()},
        "stackWords": [{"address": hex(registers["rsp"] + index),
                        "value": hex(struct.unpack_from("<Q", stack, index)[0])}
                       for index in range(0, len(stack) - 7, 8)],
        "memory": [{"address": hex(address), "hex": memory(address, 160).hex(" ")}
                   for address in addresses],
        "limitations": "Raw context and captured memory only; no symbol resolution or root-cause inference."
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path)
    parser.add_argument("--memory", type=lambda value: int(value, 0), action="append", default=[])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = json.dumps(inspect(args.dump, args.memory), indent=2)
    if args.output:
        args.output.write_text(report + "\n", encoding="utf-8")
    else:
        print(report)


if __name__ == "__main__":
    main()
