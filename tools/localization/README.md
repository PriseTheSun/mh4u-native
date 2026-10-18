# LMD localization tools

Python 3 standard library only. These commands read explicitly supplied,
already extracted LMD files. They do not execute the game, modify the ROM,
repack ARC containers, or install a translation.

```sh
python3 tools/localization/lmd.py verify /absolute/path/message-030.bin
python3 tools/localization/lmd.py export /absolute/path/message-030.bin \
  --output research/private/localization/Title.export.json
python3 tools/localization/lmd.py rebuild /absolute/path/message-030.bin \
  --catalog localization/pt-BR/title.seed.json \
  --output research/private/localization/Title.pt-BR.experimental.lmd
python3 tools/localization/verify_samples.py /absolute/path/message-030.bin \
  /absolute/path/message-031.bin --catalog localization/pt-BR/title.seed.json
```

Use `rebuild` without `--catalog` for an unchanged reconstruction. Every CLI
operation first confirms that decoding and reconstructing the source produces
identical bytes. The output is reconstructed from parsed header, label, command
and text tables, rather than copied from the input buffer.

The observed format profile is deliberately narrow. Unknown variants, invalid
offsets, differing repeated lengths, nonzero padding, malformed UTF-16 and
unsupported table layouts return JSON errors with a code, message and offset
when known. A local input resource limit of 64 MiB is configurable through
`--max-bytes`; it is not a claim about the format's maximum size.

Exports include all original text and belong under ignored `research/private/`.
Translation catalogs bind to an exact source SHA-256, internal filename, text
index and original string. Label identifiers and command words are preserved.
The editor guard preserves control/private-use codepoints and conservatively
recognized printf, brace and angle-bracket tokens; this is not a complete
description of the game's formatting grammar. The eight seeded entries contain
plain text only.

Outputs are created exclusively. Repeating an identical output is allowed;
different existing content causes `output_exists` rather than replacement.
Choose a new output filename for a revised translation. No proprietary samples
are needed in version control: tests accept the analyst's own local paths.

See [the findings](../../docs/localization-findings.md) for provenance, observed
layout, tested coverage and remaining unknowns.
