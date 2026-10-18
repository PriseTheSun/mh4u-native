#!/usr/bin/env python3
"""Offline reader/rebuilder for the observed MH4U lmd/0x0001010b variant.

No ROM mutation or game execution. Unsupported layouts fail explicitly. This is
an independent implementation based on sample bytes and documented findings.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import unicodedata


PROFILE = "mh4u-lmd-0001010b-contiguous-v1"
DEFAULT_MAX_BYTES = 64 * 1024 * 1024  # Local resource policy, not a format limit.
PLACEHOLDER = re.compile(r"%(?:\d+\$)?[-+#0 ]*(?:\d+|\*)?(?:\.(?:\d+|\*))?[hlLjzt]*[diuoxXfFeEgGaAcspn%]|\{[^{}\n]*\}|<[^<>\n]*>")


class LmdError(Exception):
    def __init__(self, code: str, message: str, offset: int | None = None):
        super().__init__(message)
        self.code, self.offset = code, offset

    def record(self) -> dict:
        return {"code": self.code, "message": str(self), "offset": self.offset}


def require(ok: bool, code: str, message: str, offset: int | None = None):
    if not ok:
        raise LmdError(code, message, offset)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def protected_tokens(text: str) -> dict:
    """Conservative editing guard, not a complete game formatting grammar."""
    return {
        "control_codepoints": [ord(c) for c in text if unicodedata.category(c) in ("Cc", "Co", "Cs")],
        "possible_placeholders": PLACEHOLDER.findall(text),
    }


@dataclass(frozen=True)
class Text:
    offset: int
    units: int
    second_length: int
    value: str
    padding: bytes


@dataclass(frozen=True)
class Document:
    source_sha256: str
    source_size: int
    header: tuple[int, ...]
    labels: tuple[tuple[int, int, int], ...]
    controls: tuple[tuple[int, int, int, int], ...]
    texts: tuple[Text, ...]
    internal_name: str

    def references(self, index: int) -> list[dict]:
        refs = []
        for ordinal, (identifier, start, flags) in enumerate(self.labels):
            end = self.labels[ordinal + 1][1] if ordinal + 1 < len(self.labels) else len(self.controls)
            for control_index in range(start, end):
                control = self.controls[control_index]
                if control[0] == 1 and control[1] == index:
                    refs.append({"label_ordinal": ordinal, "identifier_hex": f"{identifier:08x}",
                                 "label_unknown_u16": flags, "control_index": control_index})
        return refs


def read_bytes(path: Path, max_bytes: int = DEFAULT_MAX_BYTES) -> bytes:
    require(max_bytes >= 36, "invalid_limit", "max_bytes must be at least 36")
    with path.open("rb") as stream:
        data = stream.read(max_bytes + 1)
    require(len(data) <= max_bytes, "resource_limit", f"Input exceeds the local {max_bytes}-byte limit")
    return data


def decode(data: bytes) -> Document:
    require(len(data) >= 36, "truncated_header", "LMD header requires 36 bytes", 0)
    h = struct.unpack_from("<9I", data)
    require(h[0] == 0x00646D6C, "invalid_magic", "Expected lmd followed by NUL", 0)
    require(h[1] == 0x0001010B, "unsupported_variant", "Only the observed MH4U version is supported", 4)
    labels_n, controls_n, texts_n, labels_off, controls_off, texts_off, name_off = h[2:]
    require(labels_n > 0 and controls_n > 0 and texts_n > 0, "unsupported_layout", "Empty tables are unverified", 8)
    require(labels_off == 36 and controls_off == labels_off + labels_n * 8
            and texts_off == controls_off + controls_n * 16,
            "unsupported_layout", "Tables must be contiguous with 8/16/12-byte records", 20)
    payload_off = texts_off + texts_n * 12
    require(payload_off <= name_off < len(data), "invalid_bounds", "Tables or filename exceed input", 28)
    labels = tuple(struct.unpack_from("<IHH", data, labels_off + i * 8) for i in range(labels_n))
    controls = tuple(struct.unpack_from("<4I", data, controls_off + i * 16) for i in range(controls_n))
    require(labels[0][1] == 0, "unsupported_layout", "First label must start at control index zero", labels_off + 4)
    require(all(0 <= label[1] < controls_n for label in labels), "invalid_control_index", "Label control index exceeds table")
    require(all(labels[i][1] < labels[i + 1][1] for i in range(labels_n - 1)),
            "unsupported_layout", "Label control indices must increase strictly")
    for i, control in enumerate(controls):
        if control[0] == 1:
            require(control[1] < texts_n, "invalid_text_reference", "Candidate text reference exceeds text table", controls_off + i * 16 + 4)
    texts = []
    cursor = payload_off
    for i in range(texts_n):
        descriptor = texts_off + i * 12
        offset, units, second_length = struct.unpack_from("<3I", data, descriptor)
        require(offset == cursor and offset % 4 == 0, "unsupported_layout", "String offsets must be contiguous and aligned to four bytes", descriptor)
        require(units == second_length, "unsupported_length_semantics", "The two string lengths differ; second-field meaning is unverified", descriptor + 4)
        end = offset + units * 2
        aligned_end = (end + 2 + 3) & ~3
        require(aligned_end <= name_off, "invalid_bounds", "UTF-16 string extends beyond string section", descriptor)
        require(data[end:end + 2] == b"\0\0", "missing_terminator", "Missing UTF-16 NUL terminator", end)
        padding = data[end + 2:aligned_end]
        require(not any(padding), "unsupported_padding", "Nonzero alignment bytes are unverified", end + 2)
        try:
            value = data[offset:end].decode("utf-16le", errors="strict")
        except UnicodeDecodeError as exc:
            raise LmdError("invalid_utf16", str(exc), offset + exc.start) from exc
        require("\0" not in value, "unsupported_embedded_nul", "Embedded string NUL is unverified", offset)
        texts.append(Text(offset, units, second_length, value, padding))
        cursor = aligned_end
    require(cursor == name_off, "unsupported_layout", "Unexpected bytes between strings and filename", cursor)
    require(data[-1:] == b"\0" and b"\0" not in data[name_off:-1], "invalid_filename", "Internal filename must be a final NUL-terminated string", name_off)
    try:
        name = data[name_off:-1].decode("ascii", errors="strict")
    except UnicodeDecodeError as exc:
        raise LmdError("unsupported_filename_encoding", str(exc), name_off + exc.start) from exc
    require(bool(name), "invalid_filename", "Empty internal filename is unverified", name_off)
    return Document(digest(data), len(data), h, labels, controls, tuple(texts), name)


def rebuild(doc: Document, translations: dict[int, str] | None = None) -> bytes:
    translations = translations or {}
    for index, value in translations.items():
        require(type(index) is int and 0 <= index < len(doc.texts), "invalid_translation_index", "Translation index is not present in source")
        require(isinstance(value, str), "invalid_translation_text", "Translation must be a string")
        require("\0" not in value, "invalid_translation_text", "Translation contains a NUL")
        require(protected_tokens(value) == protected_tokens(doc.texts[index].value),
                "protected_tokens_changed", f"Text {index} changes controls or potential placeholders")
    body = bytearray()
    for label in doc.labels:
        body.extend(struct.pack("<IHH", *label))
    for control in doc.controls:
        body.extend(struct.pack("<4I", *control))
    payload = bytearray()
    descriptors = bytearray()
    payload_start = 36 + len(body) + len(doc.texts) * 12
    for i, text in enumerate(doc.texts):
        value = translations.get(i, text.value)
        try:
            encoded = value.encode("utf-16le", errors="strict")
        except UnicodeEncodeError as exc:
            raise LmdError("invalid_translation_utf16", str(exc)) from exc
        offset = payload_start + len(payload)
        require(offset <= 0xFFFFFFFF and len(encoded) // 2 <= 0xFFFFFFFF,
                "format_capacity", "Output exceeds 32-bit LMD fields")
        descriptors.extend(struct.pack("<3I", offset, len(encoded) // 2, len(encoded) // 2))
        payload.extend(encoded + b"\0\0")
        payload.extend(b"\0" * (-len(payload) % 4))
    h = list(doc.header)
    h[8] = payload_start + len(payload)
    require(h[8] <= 0xFFFFFFFF, "format_capacity", "Filename offset exceeds 32-bit field")
    return struct.pack("<9I", *h) + body + descriptors + payload + doc.internal_name.encode("ascii") + b"\0"


def export(doc: Document, source_path: Path) -> dict:
    return {
        "schema": "mh4u-lmd-export-v1", "profile": PROFILE,
        "producer": "ordinary local Python parser; not REA Evidence",
        "source": {"path": str(source_path.resolve()), "sha256": doc.source_sha256, "size": doc.source_size},
        "internal_name": doc.internal_name, "header_u32": list(doc.header),
        "labels": [{"ordinal": i, "offset": doc.header[5] + i * 8,
                    "identifier_hex": f"{item[0]:08x}", "control_start_index": item[1],
                    "unknown_u16": item[2]} for i, item in enumerate(doc.labels)],
        "controls": [{"index": i, "offset": doc.header[6] + i * 16, "words_u32": list(item)}
                     for i, item in enumerate(doc.controls)],
        "texts": [{"key": f"{doc.internal_name}:{i:06d}", "text_index": i,
                   "descriptor_offset": doc.header[7] + i * 12, "offset": text.offset,
                   "length_utf16_units": text.units, "second_length_raw": text.second_length,
                   "padding_hex": text.padding.hex(), "source_text": text.value,
                   "protected_tokens": protected_tokens(text.value), "label_references": doc.references(i)}
                  for i, text in enumerate(doc.texts)],
        "unknowns": ["Meaning of identifier/hash and unknown label field",
                     "Complete control opcode grammar and placeholder semantics",
                     "Independent meaning of repeated string length",
                     "Font coverage, rendered layout and game-runtime acceptance"],
    }


def load_catalog(path: Path, doc: Document) -> dict[int, str]:
    try:
        catalog = json.loads(read_bytes(path).decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise LmdError("invalid_catalog_json", str(exc)) from exc
    require(isinstance(catalog, dict) and catalog.get("schema") == "mh4u-translation-seed-v1", "invalid_catalog_schema", "Unsupported translation catalog")
    source = catalog.get("source")
    require(isinstance(source, dict) and source.get("sha256") == doc.source_sha256
            and source.get("internal_name") == doc.internal_name, "source_identity_mismatch", "Catalog does not match this exact source")
    entries = catalog.get("entries")
    require(isinstance(entries, list), "invalid_catalog_entries", "Catalog entries must be an array")
    translations = {}
    for entry in entries:
        require(isinstance(entry, dict), "invalid_catalog_entry", "Catalog entry must be an object")
        index = entry.get("text_index")
        require(type(index) is int and 0 <= index < len(doc.texts), "invalid_translation_index", "Catalog text index is not present")
        require(index not in translations, "duplicate_translation", f"Duplicate index {index}")
        require(entry.get("source_text") == doc.texts[index].value, "source_text_mismatch", f"Source text changed at index {index}")
        require(isinstance(entry.get("target_text"), str), "invalid_translation_text", "target_text must be a string")
        translations[index] = entry["target_text"]
    return translations


def write_new(path: Path, data: bytes):
    """Idempotent for identical results; never silently replaces changed work."""
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as output:
            output.write(data)
    except FileExistsError:
        require(read_bytes(path) == data, "output_exists", f"Output already exists with different content: {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("export", "rebuild", "verify"))
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--catalog", type=Path)
    parser.add_argument("--max-bytes", type=int, default=DEFAULT_MAX_BYTES)
    args = parser.parse_args()
    try:
        raw = read_bytes(args.source, args.max_bytes)
        doc = decode(raw)
        baseline = rebuild(doc)
        require(baseline == raw, "roundtrip_mismatch", "Unchanged rebuild differs from source")
        require(args.operation == "rebuild" or args.catalog is None, "invalid_arguments", "--catalog requires rebuild")
        require(args.operation != "verify" or args.output is None, "invalid_arguments", "verify does not write an output file")
        result = {"ok": True, "profile": PROFILE, "source_sha256": doc.source_sha256,
                  "internal_name": doc.internal_name, "text_count": len(doc.texts),
                  "label_count": len(doc.labels), "control_count": len(doc.controls),
                  "unchanged_roundtrip_byte_exact": True}
        if args.operation == "export":
            require(args.output is not None, "invalid_arguments", "export requires --output")
            write_new(args.output, (json.dumps(export(doc, args.source), ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
        elif args.operation == "rebuild":
            require(args.output is not None, "invalid_arguments", "rebuild requires --output")
            translations = load_catalog(args.catalog, doc) if args.catalog else {}
            output = rebuild(doc, translations)
            reparsed = decode(output)
            require(reparsed.labels == doc.labels and reparsed.controls == doc.controls,
                    "immutable_data_changed", "Rebuild altered labels or control records")
            write_new(args.output, output)
            result.update({"output_sha256": digest(output), "output_size": len(output),
                           "translated_entries": len(translations), "labels_and_controls_unchanged": True,
                           "game_runtime_validated": False})
        if args.output:
            result["output"] = str(args.output.resolve())
        print(json.dumps(result, ensure_ascii=False))
        return 0
    except LmdError as exc:
        print(json.dumps({"ok": False, "error": exc.record()}, ensure_ascii=False))
        return 2
    except OSError as exc:
        print(json.dumps({"ok": False, "error": {"code": "io_error", "message": str(exc), "offset": None}}))
        return 2


if __name__ == "__main__":
    sys.exit(main())
