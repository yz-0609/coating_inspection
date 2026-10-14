"""Rebuild firmware variants, run all native tests, verify and package release."""
import datetime
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
import zipfile


def main():
    root = pathlib.Path(__file__).resolve().parents[1]
    logs = root / "build-validation"
    logs.mkdir(exist_ok=True)
    started = time.monotonic()

    def execute(name, command):
        print(name, flush=True)
        result = subprocess.run(command, cwd=root, text=True, encoding="utf-8",
                                errors="replace", capture_output=True)
        text = result.stdout + result.stderr
        (logs / (name + ".log")).write_text(text, encoding="utf-8")
        if result.returncode:
            print(text)
            raise RuntimeError(f"{name} failed: {result.returncode}")
        return text

    variants = {}
    for preset, directory, injection in (("debug", "build", "0"),
                                          ("release", "build-release", "0"),
                                          ("debug-inject", "build-inject", "1")):
        execute(preset + "-configure", ["cmake", "--preset", preset])
        log = execute(preset + "-build", ["cmake", "--build", "--preset", preset,
                                         "--clean-first", "--parallel", "8"])
        warnings = [line for line in log.splitlines() if "warning:" in line]
        own = [line for line in warnings if "/Drivers/" not in line.replace("\\", "/")
               and "/Middlewares/" not in line.replace("\\", "/")]
        if own:
            raise RuntimeError("Application warning(s): " + "\n".join(own))
        execute(preset + "-artifacts", [sys.executable, "tools/validate_firmware.py",
                "--build-dir", directory, "--expected-injection", injection])
        variants[preset] = dict(warnings=len(warnings), application_warnings=len(own),
            validation=json.loads((root / directory / "firmware-validation.json").read_text(encoding="utf-8")))
    execute("native-configure", ["cmake", "-S", "tests", "-B", "build-host", "-G", "Ninja",
            "-DCMAKE_C_COMPILER=gcc", "-DCMAKE_CXX_COMPILER=g++", f"-DPython3_EXECUTABLE={sys.executable}"])
    execute("native-build", ["cmake", "--build", "build-host", "--clean-first", "--parallel", "8"])
    test_log = execute("ctest", ["ctest", "--test-dir", "build-host", "--output-on-failure",
                      "--output-junit", str(logs / "ctest.xml")])
    replay = logs / "offline-replay.json"
    executable = root / "build-host" / ("mcu_emulator.exe" if sys.platform == "win32" else "mcu_emulator")
    execute("replay", [sys.executable, "tools/offline_replay.py", "--emulator", str(executable),
                      "--output", str(replay)])
    tasks = json.loads((root.parent / ".vscode/tasks.json").read_text(encoding="utf-8"))["tasks"]
    labels = [t["label"] for t in tasks]
    if len(labels) != len(set(labels)):
        raise RuntimeError("Duplicate VS Code tasks")
    for task in tasks:
        dependencies = task.get("dependsOn", [])
        if isinstance(dependencies, str): dependencies = [dependencies]
        if any(label not in labels for label in dependencies):
            raise RuntimeError("Unknown task dependency")
    summary = dict(version=variants["release"]["validation"]["firmware_version"],
        checked_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        actual_elapsed_seconds=round(time.monotonic()-started, 3), variants=variants,
        native_tests=test_log, offline_replay=json.loads(replay.read_text(encoding="utf-8")),
        hardware_accepted=False, physical_24h_completed=False, vscode_tasks=len(tasks))
    (logs / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    checks = ET.parse(logs / "ctest.xml").getroot().findall(".//testcase")
    release = variants["release"]["validation"]
    lines = ["# MCU 离线验证结果", "", "固件版本：" + summary["version"] + "。",
             "检查 UTC 时间：" + summary["checked_at"] + "。",
             "本次全量检查实际耗时：" + str(summary["actual_elapsed_seconds"]) + " 秒。", "",
             "## 构建与产物", "", "| 变体 | 源文件 | BIN字节 | 应用警告 | 依赖警告 | 注入 |",
             "|---|---:|---:|---:|---:|---|"]
    for name, data in variants.items():
        v = data["validation"]
        lines.append(f"| {name} | {v['source_count']} | {v['bin_bytes']} | {data['application_warnings']} | "
                     f"{data['warnings']} | {'开启' if v['injection_enabled'] else '关闭'} |")
    lines += ["", "ELF/BIN/HEX一致，HEX校验、启动向量、ARM ABI、段权限、未解析符号、官方依赖哈希、普通SRAM缓冲和注入隔离检查通过。", "",
              "交付BIN SHA-256：`" + release["artifacts"]["bin"] + "`。", "",
              "## 自动测试", "", "CTest：" + str(len(checks)) + "/" + str(len(checks)) + " 通过。", ""]
    lines += ["- " + item.attrib["name"] for item in checks]
    lines += ["", "压力：100000个随机ADU、1000次保护/结果交错、10000次虚拟工位闭环。",
              "上游契约：未修改的C++编码、解码及command_satisfied，提交 `" + release["upstream_host_commit"] + "`。",
              "独立进程回放：" + str(len(summary["offline_replay"]["steps"])) + " 个事务/步骤，虚拟时间2880 ms，实际耗时另记JSON。", "",
              "## 证据位置和未验证项目", "", "build-validation 保存完整构建日志、ctest.xml、summary.json及offline-replay.json。",
              "固件与源代码包位于 delivery；SHA256SUMS.json记录交付文件哈希。", "",
              "未连接或烧录真实MCU；GPIO、电平、时钟、UART/DMA/DE波形、Linux完整服务、真实BLE仪器、实际保护延迟、栈余量和实机24 h未验收。",
              "这些结果仅为离线软件验证，不能作为物理现场或24 h报告。"]
    (root.parent / "Doc/10_MCU离线验证结果.md").write_text("\n".join(lines)+"\n", encoding="utf-8")
    delivery = root / "delivery" / ("firmware-" + summary["version"])
    delivery.mkdir(parents=True, exist_ok=True)
    for suffix in ("elf", "bin", "hex", "map"):
        shutil.copyfile(root / "build-release" / ("STM32F407." + suffix), delivery / ("STM32F407." + suffix))
    shutil.copyfile(root / "build-release/firmware-validation.json", delivery / "firmware-validation.json")
    shutil.copyfile(logs / "summary.json", delivery / "offline-validation.json")
    for name in ("README.md", "CMAKE_BUILD.md"):
        shutil.copyfile(root / name, delivery / name)
    for document in (root.parent / "Doc").glob("*.md"):
        shutil.copyfile(document, delivery / document.name)
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
              for p in delivery.iterdir() if p.is_file() and p.name != "SHA256SUMS.json"}
    (delivery / "SHA256SUMS.json").write_text(json.dumps(hashes, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    archive = delivery.parent / (delivery.name + ".zip")
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for path in sorted(delivery.iterdir()):
            if path.is_file(): package.write(path, arcname=delivery.name + "/" + path.name)
    source_archive = delivery.parent / ("source-" + summary["version"] + ".zip")
    with zipfile.ZipFile(source_archive, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for directory, subdirectories, filenames in os.walk(root):
            subdirectories[:] = [d for d in subdirectories if not d.startswith("build")
                                 and d not in ("delivery", ".git", "__pycache__")]
            for filename in sorted(filenames):
                path = pathlib.Path(directory) / filename
                package.write(path, arcname="coating_inspection_mcu/coating_inspection_mcu/" +
                              path.relative_to(root).as_posix())
        for directory in (root.parent / "Doc", root.parent / ".vscode"):
            for path in sorted(directory.rglob("*")):
                if path.is_file(): package.write(path, arcname="coating_inspection_mcu/" + path.relative_to(root.parent).as_posix())
    print("All offline checks passed. Release:", delivery)
    print("Archive:", archive)
    print("Source archive:", source_archive)
    print("Physical hardware/instrument/24-hour verification remains pending.")


if __name__ == "__main__":
    main()
