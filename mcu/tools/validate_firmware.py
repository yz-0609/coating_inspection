"""Validate firmware artifacts, provenance and injection mode; build and CTest separately."""
import argparse
import datetime
import hashlib
import json
import pathlib
import re
import struct
import subprocess
import tempfile


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(*args):
    return subprocess.check_output(args, text=True, encoding="utf-8", errors="replace").strip()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def parse_hex(path):
    memory, base, eof = {}, 0, False
    for line in path.read_text(encoding="ascii").splitlines():
        require(not eof and line.startswith(":"), "Malformed HEX record")
        data = bytes.fromhex(line[1:])
        require(len(data) >= 5 and len(data) == data[0] + 5 and sum(data) % 256 == 0,
                "HEX length/checksum failure")
        count, address, kind = data[0], int.from_bytes(data[1:3], "big"), data[3]
        payload = data[4:-1]
        if kind == 0:
            for index, byte in enumerate(payload):
                absolute = base + address + index
                require(absolute not in memory, "Overlapping HEX data")
                memory[absolute] = byte
        elif kind == 1:
            require(count == 0 and address == 0, "Invalid HEX EOF")
            eof = True
        elif kind in (2, 4):
            require(count == 2 and address == 0, "Invalid HEX base address")
            base = int.from_bytes(payload, "big") << (4 if kind == 2 else 16)
        elif kind in (3, 5):
            require(count == 4 and address == 0, "Invalid HEX entry point")
        else:
            raise RuntimeError("Unsupported HEX record")
    require(eof and memory, "Missing HEX data or EOF")
    return memory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build-release")
    parser.add_argument("--expected-injection", choices=("0", "1"), default="0")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    build = root / args.build_dir
    driver = root / "Drivers/STM32F4xx_HAL_Driver"
    vendor = json.loads((driver / "M0_VENDOR_MANIFEST.json").read_text(encoding="utf-8"))
    require(vendor["tag"] == "v1.8.2", "HAL import version mismatch")
    for item in vendor["files"]:
        path = driver / item["path"]
        require(path.stat().st_size == item["bytes"] and sha256(path) == item["sha256"],
                "Vendor hash mismatch: " + item["path"])

    host = root.parent / "host"
    host_files = ("include/inspection/protocol.hpp", "src/workstation/protocol.cpp")
    require(all((host / path).is_file() for path in host_files),
            "Missing host protocol sources in the integrated repository")
    host_protocol_sources = {path: sha256(host / path) for path in host_files}
    version = re.search(r'#define FW_VERSION "([^"]+)"',
                        (root / "Module/Protocol/protocol.h").read_text(encoding="utf-8")).group(1)
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    sources = [pathlib.Path(item["file"]).resolve() for item in commands]
    require(len(sources) == len(set(sources)), "Duplicate build sources")
    require(all(path.is_file() and path.is_relative_to(root) for path in sources),
            "Missing or out-of-project source")
    require(not any(path.name == "cmsis_os.c" for path in sources), "CMSIS wrapper still built")
    require(all("-DCOATING_DEBUG_INJECTION=" + args.expected_injection in item["command"]
                for item in commands), "Firmware injection mode mismatch")

    elf_path = build / "STM32F407.elf"
    elf = elf_path.read_bytes()
    require(elf[:6] == b"\x7fELF\x01\x01", "Expected ELF32 little endian")
    require(struct.unpack_from("<H", elf, 18)[0] == 40, "Expected ARM ELF")
    entry = struct.unpack_from("<I", elf, 24)[0]
    flags = struct.unpack_from("<I", elf, 36)[0]
    require(flags & 0x200 and not flags & 0x400, "Expected soft-float ABI")
    phoff = struct.unpack_from("<I", elf, 28)[0]
    phsize, phcount = struct.unpack_from("<HH", elf, 42)
    for index in range(phcount):
        header = struct.unpack_from("<8I", elf, phoff + index * phsize)
        require(not (header[0] == 1 and header[6] & 3 == 3), "Writable/executable LOAD segment")
    require(not run("arm-none-eabi-nm", "-u", str(elf_path)), "Undefined ELF symbols")

    binary = (build / "STM32F407.bin").read_bytes()
    injection_strings = (b"pause comm 1", b"inputs auto", b"rx_fault 1")
    require(all((value in binary) == (args.expected_injection == "1") for value in injection_strings),
            "Injection entry strings do not match selected mode")
    symbols = {}
    for line in run("arm-none-eabi-nm", "-n", str(elf_path)).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    for name in ("rxDMA", "txDMA", "rxRing", "commStack", "workStack", "healthStack"):
        require(name in symbols and 0x20000000 <= symbols[name] < 0x2001C000,
                "Buffer/stack outside ordinary SRAM: " + name)
    for name in ("USART1_IRQHandler", "USART2_IRQHandler", "DMA1_Stream5_IRQHandler",
                 "DMA1_Stream6_IRQHandler", "TIM3_IRQHandler", "BSP_WatchdogInit", "xTaskCreateStatic"):
        require(name in symbols and symbols[name] != symbols.get("Default_Handler"),
                "Required handler/function missing: " + name)
    sp, reset = struct.unpack_from("<II", binary)
    require(sp == 0x2001C000 and reset == entry and reset & 1, "Invalid startup vectors")
    require(0x08000000 <= (reset & ~1) < 0x08080000 and len(binary) <= 512 * 1024,
            "Firmware outside target Flash")
    with tempfile.TemporaryDirectory(prefix="mcu-m0-verify-") as temporary:
        regenerated = pathlib.Path(temporary) / "firmware.bin"
        run("arm-none-eabi-objcopy", "-O", "binary", str(elf_path), str(regenerated))
        require(regenerated.read_bytes() == binary, "ELF/BIN mismatch")
    memory = parse_hex(build / "STM32F407.hex")
    require(min(memory) == 0x08000000 and max(memory) < 0x08000000 + len(binary),
            "HEX outside BIN address range")
    require(all(binary[address - 0x08000000] == byte for address, byte in memory.items()),
            "HEX/BIN mismatch")
    # HEX records encode allocated sections, omitting padding between sections.
    shoff = struct.unpack_from("<I", elf, 32)[0]
    shsize, shcount = struct.unpack_from("<HH", elf, 46)
    for section_index in range(shcount):
        _, kind, section_flags, _, offset, length, _, _, _, _ = struct.unpack_from(
            "<10I", elf, shoff + section_index * shsize)
        if not (section_flags & 2) or kind == 8 or not length:
            continue
        physical = None
        for index in range(phcount):
            load_kind, load_offset, _, load_physical, file_size, _, _, _ = struct.unpack_from(
                "<8I", elf, phoff + index * phsize)
            if load_kind == 1 and load_offset <= offset and offset + length <= load_offset + file_size:
                physical = load_physical + offset - load_offset
                break
        require(physical is not None, "Allocated section missing a LOAD segment")
        require(all(memory.get(physical + i) == byte
                    for i, byte in enumerate(elf[offset:offset + length])),
                "ELF/HEX missing or mismatched section data")

    report = {
        "validation_scope": "firmware artifacts and source provenance",
        "firmware_version": version, "injection_enabled": args.expected_injection == "1",
        "host_protocol_sources": host_protocol_sources,
        "checked_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source_count": len(sources), "bin_bytes": len(binary),
        "initial_sp": hex(sp), "reset_vector": hex(reset),
        "vendor_tag": vendor["tag"], "vendor_hashes_verified": True,
        "elf_bin_hex_consistent": True, "undefined_symbols": 0,
        "dma_buffers_in_sram": True, "injection_binary_verified": True,
        "size": run("arm-none-eabi-size", str(elf_path)),
        "artifacts": {suffix: sha256(build / ("STM32F407." + suffix))
                      for suffix in ("elf", "bin", "hex", "map")}
    }
    output = build / "firmware-validation.json"
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    print("Saved:", output)


if __name__ == "__main__":
    main()
