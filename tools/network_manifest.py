#!/usr/bin/env python3
"""Create a deterministic, locally verifiable content manifest beside a glTF."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
from urllib.parse import unquote


def field(value: str) -> str:
    if not value or any(c in value for c in "\r\n\t\0"):
        raise ValueError("manifest fields cannot contain controls or be empty")
    return value


def build_document(scene: Path, root: Path) -> bytes:
    root = root.resolve()
    scene = scene.resolve()
    scene_name = scene.relative_to(root).as_posix()
    if scene.stat().st_size > 8 * 1024 * 1024:
        raise ValueError("scene exceeds 8 MiB")
    data = json.loads(scene.read_text(encoding="utf-8"))
    if data.get("asset", {}).get("version") != "2.0" or data.get("extensionsRequired"):
        raise ValueError("manifest v1 supports external core glTF 2.0")
    references = {scene_name: scene}
    for resource in [*data.get("buffers", []), *data.get("images", [])]:
        if "uri" not in resource:
            if "bufferView" in resource:
                continue
            raise ValueError("external glTF buffer required")
        uri = unquote(resource["uri"], encoding="utf-8", errors="strict")
        field(uri)
        parts = PurePosixPath(uri)
        if parts.is_absolute() or any(x in uri for x in (":", "\\", "?", "#")) or ".." in parts.parts or "." in uri.split("/"):
            raise ValueError("resource URI must be a canonical local relative path")
        resource_path = scene.parent.joinpath(*parts.parts)
        resource_path.resolve().relative_to(root)
        references[resource_path.relative_to(root).as_posix()] = resource_path
    clips = data.get("animations", [])
    if len(clips) > 4096 or len(references) > 256:
        raise ValueError("manifest structure budget exceeded")
    lines = ["MH4U-LAB-MANIFEST 1", "scene\t" + field(scene_name), f"clips\t{len(clips)}"]
    for index, clip in enumerate(clips):
        name = field(clip.get("name", f"animation_{index}"))
        if len(name.encode("utf-8")) > 512:
            raise ValueError("clip name exceeds 512 bytes")
        lines.append(f"clip\t{index}\t{name}")
    total = 0
    for name, resource in sorted(references.items()):
        size = resource.stat().st_size
        total += size
        if total > 512 * 1024 * 1024:
            raise ValueError("content exceeds 512 MiB")
        digest = hashlib.sha256()
        with resource.open("rb") as stream:
            for block in iter(lambda: stream.read(65536), b""):
                digest.update(block)
        lines.append(f"file\t{digest.hexdigest()}\t{size}\t{field(name)}")
    encoded = ("\n".join(lines) + "\n").encode("utf-8")
    return encoded


def generate(scene: Path, output: Path) -> str:
    encoded = build_document(scene, output.resolve().parent)
    if output.exists() and output.read_bytes() != encoded:
        raise ValueError("manifest exists with different content; choose a new output path")
    output.write_bytes(encoded)
    return hashlib.sha256(encoded).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scene", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(generate(args.scene, args.output))


if __name__ == "__main__":
    main()
