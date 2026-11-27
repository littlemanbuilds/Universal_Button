"""Check the Universal_Button public inventories and PlatformIO export rule."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile


ROOT = Path(__file__).resolve().parent.parent
PLATFORMIO = {
    "CHANGELOG.md", "LICENSE", "README.md", "keywords.txt", "library.json",
    "library.properties",
    "examples/01_BasicButton/01_BasicButton.ino",
    "examples/02_PressType/02_PressType.ino",
    "examples/03_LocalEnum/03_LocalEnum.ino",
    "examples/04_PortExpander/04_PortExpander.ino",
    "examples/05_CachedRead/05_CachedRead.ino",
    "examples/06_Latching/06_Latching.ino",
    "src/ButtonCompatibility.h", "src/ButtonHandler.h", "src/ButtonHandler_Config.h",
    "src/ButtonTypes.h", "src/IButtonHandler.h", "src/Universal_Button.h",
    "src/Universal_Button_Utils.h",
}
ARDUINO = PLATFORMIO | {
    "RELEASE_CHECKLIST.md", "platformio.ini", "test/README.md",
    "test/check_distribution.py", "test/check_examples_host.sh", "test/check_linkage.sh",
    "test/check_release_contracts.sh", "test/host_stubs/Adafruit_MCP23X17.h",
    "test/host_stubs/Arduino.h", "test/host_stubs/Wire.h", "test/linkage/a.cpp",
    "test/linkage/b.cpp", "test/portable_compile/portable_compile.ino",
    "test/run_host_checks.sh", "test/run_native_tests.sh", "test/run_sanitizers.sh",
    "test/test_button_handler.cpp",
}
GITHUB = ARDUINO | {".clang-format", ".github/workflows/ci.yml", ".gitignore"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_paths(expected, observed, label):
    observed = list(observed)
    actual = set(observed)
    require(len(actual) == len(observed), f"{label} has duplicate paths")
    require(actual == expected,
            f"{label} inventory mismatch (missing {sorted(expected - actual)}, "
            f"unexpected count {len(actual - expected)})")


def check_archive(root, archive):
    with tarfile.open(archive, "r:gz") as package:
        members = package.getmembers()
        check_paths(PLATFORMIO, (member.name for member in members), "PlatformIO")
        for member in members:
            require(member.isfile() and not member.sparse, f"invalid member {member.name}")
            source = root / member.name
            require(member.mode == (0o755 if source.stat().st_mode & 0o111 else 0o644),
                    f"wrong permissions for {member.name}")
            require(package.extractfile(member).read() == source.read_bytes(),
                    f"modified contents for {member.name}")


def check_git(checkout):
    result = subprocess.check_output(
        ["git", "--no-optional-locks", "-C", checkout, "ls-files", "--stage", "-z"]
    )
    paths = []
    for row in result.decode().split("\0")[:-1]:
        metadata, path = row.split("\t", 1)
        mode, _, stage = metadata.split()
        require(mode in {"100644", "100755"} and stage == "0", "non-regular Git index entry")
        paths.append(path)
    check_paths(GITHUB, paths, "Git index")


def regression(root, pio):
    manifest = json.loads((root / "library.json").read_text())
    check_paths(PLATFORMIO, manifest["export"]["include"], "PlatformIO export")
    with tempfile.TemporaryDirectory(prefix="ub-package-") as scratch:
        scratch = Path(scratch)
        fixture = scratch / "library"
        fixture.mkdir()
        for name in GITHUB:
            target = fixture / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / name, target)
        sentinels = (
            ".lmb/evidence.txt", "AGENTS.md", "src/private-snapshot.h",
            "examples/01_BasicButton/private-report.txt",
        )
        for name in sentinels:
            target = fixture / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("Synthetic private file for distribution testing\n")

        def pack(name):
            archive = scratch / name
            subprocess.run([pio, "pkg", "pack", str(fixture), "-o", str(archive)], check=True)
            return archive

        check_archive(root, pack("approved.tar.gz"))
        del manifest["export"]
        (fixture / "library.json").write_text(json.dumps(manifest))
        leaked = pack("unrestricted.tar.gz")
        with tarfile.open(leaked, "r:gz") as package:
            check_paths(set(sentinels), (name for name in package.getnames() if name in sentinels),
                        "unrestricted export reproduction")
        try:
            check_archive(root, leaked)
        except ValueError as error:
            require("inventory mismatch" in str(error), "wrong unrestricted-export failure")
        else:
            raise ValueError("unrestricted export passed the distribution check")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("regression", "platformio", "index"))
    parser.add_argument("archive", nargs="?", type=Path)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--pio", default="pio")
    args = parser.parse_args()
    if args.mode == "regression":
        regression(args.root, args.pio)
    elif args.mode == "index":
        check_git(os.fspath(args.root))
    else:
        if args.archive is None:
            parser.error("platformio mode requires an archive")
        check_archive(args.root, args.archive)
    print(f"PASS: {args.mode} distribution check")


if __name__ == "__main__":
    main()
