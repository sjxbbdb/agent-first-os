#!/usr/bin/env python3
"""Build the bounded Agent-First OS initrd container.

The format is intentionally boring: a fixed little-endian header followed by
UTF-8 Supervisor manifest bytes, a service table, and bounded ELF images. A
legacy single-service manifest remains accepted; a multi-service manifest uses
``{\"version\": 1, \"services\": [...]}``. It is a teaching artifact, not a
filesystem; the kernel validates every range before using it.
"""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

MAGIC = 0x41474F53494E4954
VERSION = 2
HEADER_SIZE = 40
MAX_SIZE = 0x80000
SERVICE_ENTRY_SIZE = 16
MAX_SERVICES = 8


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: make-initrd.py MANIFEST SERVICE_ELF OUTPUT", file=sys.stderr)
        return 2
    manifest_path, service_path, output_path = map(Path, sys.argv[1:])
    manifest_obj = json.loads(manifest_path.read_text(encoding="utf-8"))
    # Keep schema validation in the existing protocol validator. This tool
    # still enforces the fields needed by the native handoff and canonicalizes
    # the bytes so the digest is reproducible across hosts.
    if isinstance(manifest_obj, dict) and isinstance(manifest_obj.get("services"), list):
        services = manifest_obj["services"]
        if not services or len(services) > MAX_SERVICES:
            raise SystemExit(f"services must contain 1..{MAX_SERVICES} entries")
        manifest_obj = {"version": manifest_obj.get("version", 1),
                        "services": services}
    else:
        services = [manifest_obj]
    for service_manifest in services:
        for field in ("version", "service_id", "entrypoint", "dependencies",
                      "capabilities", "heartbeat", "restart"):
            if field not in service_manifest:
                raise SystemExit(f"manifest missing {field}")
    ids = [service_manifest["service_id"] for service_manifest in services]
    if len(ids) != len(set(ids)):
        raise SystemExit("manifest service_id values must be unique")
    manifest = (json.dumps(manifest_obj, sort_keys=True, separators=(",", ":"),
                           ensure_ascii=True) + "\n").encode("utf-8")
    service = service_path.read_bytes()
    manifest_offset = HEADER_SIZE
    service_offset = manifest_offset + len(manifest)
    table_size = SERVICE_ENTRY_SIZE * len(services)
    image_offset = service_offset + table_size
    image_bytes = service * len(services)
    total_size = image_offset + len(image_bytes)
    if total_size > MAX_SIZE:
        raise SystemExit(f"initrd exceeds {MAX_SIZE} bytes: {total_size}")
    table = b"".join(struct.pack("<IIII", image_offset + index * len(service),
                                  len(service), index, 0)
                     for index in range(len(services)))
    header = struct.pack("<QHHIIIIIII", MAGIC, VERSION, HEADER_SIZE, total_size,
                         manifest_offset, len(manifest), service_offset,
                         table_size + len(image_bytes), len(services), 0x3)
    assert len(header) == HEADER_SIZE
    Path(output_path).write_bytes(header + manifest + table + image_bytes)
    print(f"built {output_path} ({total_size} bytes; manifest={len(manifest)}, services={len(services)}, image={len(service)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
