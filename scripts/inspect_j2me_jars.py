#!/usr/bin/env python3
"""Inventory JAR files and extract runtime compatibility signals.

This is a diagnostic helper, not a Java emulator. It does not extract or modify
the input archives. It writes a deterministic JSON report for a directory of
JAR fixtures, including class-file versions, MIDlet metadata, and platform API
references found in constant pools.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import zipfile
from collections import Counter
from pathlib import Path


CLASS_MAJOR = {
    45: "Java 1.1",
    46: "Java 1.2",
    47: "Java 1.3",
    48: "Java 1.4",
    49: "Java 5",
    50: "Java 6",
    51: "Java 7",
    52: "Java 8",
    53: "Java 9",
    54: "Java 10",
    55: "Java 11",
    56: "Java 12",
    57: "Java 13",
    58: "Java 14",
    59: "Java 15",
    60: "Java 16",
    61: "Java 17",
    62: "Java 18",
    63: "Java 19",
    64: "Java 20",
    65: "Java 21",
}
PLATFORM_MARKERS = {
    "MIDP/CLDC": (b"javax/microedition/", b"javax/microedition"),
    "Nokia UI": (b"com/nokia/mid/", b"com/nokia"),
    "Siemens API": (b"com/siemens/mp/", b"com/siemens"),
    "Sony Ericsson API": (b"com/sonyericsson/", b"com/sony"),
    "Motorola API": (b"com/motorola/",),
    "Android API": (b"android/app/", b"android/content/", b"android/view/", b"android/os/"),
    "LibGDX": (b"com/badlogic/gdx/",),
}


def class_major(data: bytes) -> int | None:
    if len(data) < 8 or data[:4] != b"\xca\xfe\xba\xbe":
        return None
    return struct.unpack(">H", data[6:8])[0]


def manifest_attributes(data: bytes) -> dict[str, str]:
    # Manifest headers can be continued on lines beginning with one space.
    text = data.decode("utf-8", errors="replace").replace("\r\n", "\n")
    unfolded = re.sub(r"\n (.*)", r"\1", text)
    result: dict[str, str] = {}
    for line in unfolded.splitlines():
        if ": " in line:
            key, value = line.split(": ", 1)
            result[key.strip()] = value.strip()
    return result


def inspect(path: Path) -> dict[str, object]:
    try:
        with zipfile.ZipFile(path) as jar:
            entries = jar.infolist()
            class_entries = [e for e in entries if e.filename.endswith(".class")]
            majors: Counter[int] = Counter()
            references: set[str] = set()
            for entry in class_entries:
                data = jar.read(entry)
                major = class_major(data)
                if major is not None:
                    majors[major] += 1
                class_has_android_reference = False
                for name, markers in PLATFORM_MARKERS.items():
                    if any(marker in data for marker in markers):
                        references.add(name)
                        if name == "Android API":
                            class_has_android_reference = True
                if entry.filename == "module-info.class" and major and major >= 53:
                    references.add("Java module system")

            manifest = {}
            manifest_entry = next(
                (e for e in entries if e.filename.upper() == "META-INF/MANIFEST.MF"),
                None,
            )
            if manifest_entry:
                manifest = manifest_attributes(jar.read(manifest_entry))

            has_android_dex = any(e.filename.endswith(".dex") for e in entries)
            if "Android API" in references or has_android_dex or any(e.filename.endswith(".aar") for e in entries):
                kind = "Android Java archive"
            elif "MIDP/CLDC" in references or "MIDlet-1" in manifest:
                kind = "J2ME MIDlet archive"
            else:
                kind = "Unknown Java archive"

            max_major = max(majors, default=None)
            return {
                "file": path.name,
                "size_bytes": path.stat().st_size,
                "kind": kind,
                "manifest": {
                    key: manifest[key]
                    for key in ("MIDlet-Name", "MIDlet-Version", "MIDlet-Vendor", "MIDlet-1", "MicroEdition-Profile", "MicroEdition-Configuration")
                    if key in manifest
                },
                "class_count": len(class_entries),
                "class_major_versions": dict(sorted(majors.items())),
                "maximum_class_major": max_major,
                "maximum_class_version": CLASS_MAJOR.get(max_major, f"major {max_major}" if max_major else None),
                "platform_references": sorted(references),
                "has_android_dex": has_android_dex,
                "archive_error": None,
            }
    except (OSError, zipfile.BadZipFile, RuntimeError) as error:
        return {
            "file": path.name,
            "size_bytes": path.stat().st_size if path.exists() else None,
            "kind": "Unreadable archive",
            "archive_error": str(error),
        }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jar_directory", type=Path, help="Directory containing .jar fixtures")
    parser.add_argument("--output", type=Path, help="Output JSON path (defaults to stdout)")
    args = parser.parse_args()

    if not args.jar_directory.is_dir():
        parser.error(f"not a directory: {args.jar_directory}")
    jars = sorted(args.jar_directory.glob("*.jar"), key=lambda p: p.name.casefold())
    report = {
        "schema_version": 1,
        "source_directory": str(args.jar_directory.resolve()),
        "jar_count": len(jars),
        "runtime_notice": "Inventory only; this project currently contains no JVM or MIDP runtime.",
        "archives": [inspect(path) for path in jars],
    }
    rendered = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
