#!/usr/bin/env python3
"""Reproducible format checks against explicitly supplied local LMD samples."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

import lmd


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", nargs="+", type=Path)
    parser.add_argument("--catalog", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    checks, sources = [], []

    def record(name, passed, detail=None):
        checks.append({"name": name, "passed": bool(passed), "detail": detail})
        if not passed:
            raise AssertionError(name)

    def rejected(name, data, expected):
        try:
            lmd.decode(data)
        except lmd.LmdError as error:
            record(name, error.code == expected, {"expected": expected, "actual": error.code})
        else:
            record(name, False, "Malformed input was accepted")

    with tempfile.TemporaryDirectory(prefix="mh4u-lmd-tests-") as folder:
        work = Path(folder)
        for source in args.samples:
            raw = lmd.read_bytes(source)
            doc = lmd.decode(raw)
            prefix = doc.internal_name + ": "
            sources.append({"path": str(source.resolve()), "sha256": lmd.digest(raw), "size": len(raw)})
            record(prefix + "independent table/string rebuild is byte exact", lmd.rebuild(doc) == raw)
            for operation in ("verify", "export", "rebuild"):
                command = [sys.executable, str(Path(lmd.__file__)), operation, str(source)]
                output = work / f"{doc.internal_name}.{operation}"
                if operation != "verify":
                    command += ["--output", str(output)]
                completed = subprocess.run(command, capture_output=True, text=True, check=False)
                result = json.loads(completed.stdout)
                result.pop("output", None)  # Temporary paths do not identify retained evidence.
                record(prefix + "CLI " + operation, completed.returncode == 0 and result["ok"], result)
                if operation == "export":
                    catalog = json.loads(output.read_text())
                    record(prefix + "export covers every fragment and raw control", len(catalog["texts"]) == len(doc.texts) and len(catalog["controls"]) == len(doc.controls))
                if operation == "rebuild":
                    record(prefix + "CLI rebuild bytes match source", output.read_bytes() == raw)
            for size in (0, 3, 35):
                rejected(prefix + f"reject truncated header {size}", raw[:size], "truncated_header")
            altered = bytearray(raw)
            struct.pack_into("<I", altered, 12, 0xFFFFFFFF)
            rejected(prefix + "reject oversized control table before allocation", altered, "unsupported_layout")
            altered = bytearray(raw)
            struct.pack_into("<I", altered, doc.header[7], len(raw) + 100)
            rejected(prefix + "reject descriptor outside input", altered, "unsupported_layout")
            altered = bytearray(raw)
            struct.pack_into("<I", altered, doc.header[7] + 8, doc.texts[0].units + 1)
            rejected(prefix + "reject ambiguous second length", altered, "unsupported_length_semantics")
            altered = bytearray(raw)
            altered[doc.texts[0].offset:doc.texts[0].offset + 2] = b"\x00\xd8"
            rejected(prefix + "reject unpaired UTF-16 surrogate", altered, "invalid_utf16")
            altered = bytearray(raw)
            text_control = next(i for i, control in enumerate(doc.controls) if control[0] == 1)
            struct.pack_into("<I", altered, doc.header[6] + text_control * 16 + 4, len(doc.texts))
            rejected(prefix + "reject invalid text reference", altered, "invalid_text_reference")
            # A supplementary character uses two UTF-16 units. Exercise changed
            # lengths and relocated downstream offsets, without game execution.
            plain_index = next(i for i, text in enumerate(doc.texts) if lmd.protected_tokens(text.value) == {"control_codepoints": [], "possible_placeholders": []})
            translated = lmd.rebuild(doc, {plain_index: "Opções \U0001F3AE"})
            parsed = lmd.decode(translated)
            record(prefix + "changed text reparses with UTF-16 unit lengths", parsed.texts[plain_index].value == "Opções \U0001F3AE" and parsed.texts[plain_index].units == 9)
            record(prefix + "changed text preserves all IDs and control words", parsed.labels == doc.labels and parsed.controls == doc.controls)
            record(prefix + "changed text preserves every untranslated fragment", all(text.value == doc.texts[i].value for i, text in enumerate(parsed.texts) if i != plain_index))
            record(prefix + "changed text output itself roundtrips", lmd.rebuild(parsed) == translated)
            newline = next((i for i, text in enumerate(doc.texts) if "\n" in text.value), None)
            if newline is not None:
                try:
                    lmd.rebuild(doc, {newline: doc.texts[newline].value.replace("\n", " ")})
                except lmd.LmdError as error:
                    record(prefix + "reject removed control newline", error.code == "protected_tokens_changed")
                else:
                    record(prefix + "reject removed control newline", False)
            if args.catalog and doc.internal_name == "Title":
                translations = lmd.load_catalog(args.catalog, doc)
                result = lmd.decode(lmd.rebuild(doc, translations))
                record(prefix + "PT-BR seed reparses", all(result.texts[i].value == text for i, text in translations.items()), {"translated_entries": len(translations)})
        if args.catalog:
            wrong = json.loads(args.catalog.read_text())
            wrong["source"]["sha256"] = "0" * 64
            wrong_path = work / "wrong-source.json"
            wrong_path.write_text(json.dumps(wrong))
            try:
                lmd.load_catalog(wrong_path, lmd.decode(lmd.read_bytes(args.samples[0])))
            except lmd.LmdError as error:
                record("reject catalog for another source digest", error.code == "source_identity_mismatch")
            else:
                record("reject catalog for another source digest", False)
    report = {"schema": "mh4u-lmd-verification-v1", "producer": "ordinary local Python checks; not REA Evidence",
              "sources": sources, "passed": len(checks), "failed": 0, "checks": checks,
              "game_runtime_validated": False, "font_coverage_validated": False}
    if args.report:
        lmd.write_new(args.report, (json.dumps(report, ensure_ascii=False, indent=2) + "\n").encode())
    print(json.dumps({"passed": len(checks), "failed": 0, "sources": sources, "report": str(args.report) if args.report else None}, ensure_ascii=False))


if __name__ == "__main__":
    main()
