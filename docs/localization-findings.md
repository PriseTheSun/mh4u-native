# MH4U localization findings — 2026-10-17

## Result and scope

The initial two samples from `/eng/data/core_title.arc` decode and rebuild byte
for byte: 136 UTF-16LE fragments in `Title` and 179 in `CharMake`. The subsequent
[English inventory](localization-inventory.md) covers 42,196 fragments in 285
unique supported payloads, present in 352 of 366 English members. Eight unique
payloads remain explicitly unsupported; this is not complete game text coverage.

A PT-BR draft translates eight title labels. A derived ARC override now loads
in the separate reference runtime; two headers were visually confirmed. It is
an experimental local patch, not a complete translation or a localization
implementation in the native engine.

The local verification run passed 40 checks, including public CLI
verify/export/rebuild on both actual samples, unchanged byte equality, translated
text reparse, UTF-16 surrogate-pair length handling, unchanged identifiers and
control words, malformed table/string rejection and catalog source-digest
rejection. These initial checks were offline. Later reference checks below
establish acceptance and rendering only for the observed headers.

## Provenance

These findings come from ordinary local Python parsing of already extracted
bytes; they are not REA Evidence records. The extraction manifest, complete text
exports, reconstructed files and verification report remain under ignored
`research/private/localization/`.

| Input | Size | SHA-256 |
| --- | ---: | --- |
| ROM supplied by the user | 2,766,950,400 | `c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666` |
| `eng\msg\common\Title_eng`, ARC ordinal 30 | 13,506 | `c88fa845d4a64374c3998da43f84feed14d407a72c951c5c118cd1aabae13eed` |
| `eng\msg\common\CharMake_eng`, ARC ordinal 31 | 15,737 | `1e8ae9bb6afe30949e50e17a7b636584926425645f88395b2162e48ba624c052` |

The retained source manifest preserves the caller-selected ROM path, ARC entry
names, raw entry-name bytes, type ID, archive offsets, compressed hashes and
decompressed hashes. The ROM itself was not changed.

## Observed format

All integer fields below are little endian. This profile applies only to the
two samples, both beginning `6c 6d 64 00 0b 01 01 00`.

| Header offset | Observed role | Title | CharMake |
| --- | --- | ---: | ---: |
| `0x00` | Magic | `lmd\0` | `lmd\0` |
| `0x04` | Variant word, preserved | `0x0001010b` | `0x0001010b` |
| `0x08` | Label-table record count | 133 | 178 |
| `0x0c` | 16-byte command-record count | 285 | 368 |
| `0x10` | Text descriptor count | 136 | 179 |
| `0x14` | Label-table offset | 36 | 36 |
| `0x18` | Command-table offset | 1100 | 1460 |
| `0x1c` | Text-descriptor-table offset | 5660 | 7348 |
| `0x20` | Internal filename offset | 13500 | 15728 |

Each label record is 8 bytes: a 32-bit identifier, a 16-bit command start index
and an unknown 16-bit field (64 in both samples). Each command is four 32-bit
words. Every text descriptor is 12 bytes: absolute offset and two equal UTF-16
code-unit lengths. Text is UTF-16LE followed by a NUL and zero padding to four
bytes. The trailing filename is NUL-terminated ASCII. Newlines are actual U+000A
characters, not literal backslash sequences.

Opcode `1` consistently carries an in-range text index in its second word;
these references are exported as a candidate interpretation. Opcode `22`
occurs at the end of each label sequence. Other observed first words are `2`,
`5` and `1073741829` (`0x40000005`). Their rendering/control meanings are
unconfirmed. Commands and label fields are retained exactly, including unknowns.
Some labels reference multiple fragments, so a fragment is not necessarily a
whole displayed message. The public seed uses the exact file hash and fragment
index and retains candidate label references.

## Source cross-check

The primary implementation in [Kuriimu's LmdSupport.cs, pinned commit
ebfbf8de](https://github.com/IcySon55/Kuriimu/blob/ebfbf8de50755cc32a7e1ea4aee394628d49d3d2/src/text/text_lmd/LmdSupport.cs)
corroborates the broad header/label/descriptor structure. Its
[LMD.cs](https://github.com/IcySon55/Kuriimu/blob/ebfbf8de50755cc32a7e1ea4aee394628d49d3d2/src/text/text_lmd/LMD.cs)
also reads UTF-16 strings and writes NUL/alignment. It treats the second section
size and label offsets as byte quantities, which does not match these samples:
`(text_offset - command_offset) / 16` equals the header field, and multiplying
the label start indices by 16 locates the corresponding commands. This decoder
was implemented independently for the observed variant and does not claim
compatibility with every LMD variant handled elsewhere.

## What changed in the experimental translation

`localization/pt-BR/title.seed.json` changes only eight fragments. Reconstruction
recalculates their two UTF-16 lengths, subsequent absolute text offsets,
alignment and the internal-filename offset. Label identifiers, label indices,
unknown label fields and every command word remain unchanged. All other strings
retain their original values. The translated file can itself be decoded and
reconstructed byte exactly. Native-engine loading and rendering remain untested.

## ARC override and reference observation

`tools/localization/arc_patch.py` reuses the ARC and LMD owners. It appends the
translated compressed payload, updates only the selected entry's size/offset
fields, and preserves unknown flag bits, the original archive bytes and all 64
other entries. Four tests cover unchanged output, unknown bytes/flags, invalid
selection and truncated streams. The ROM itself is not rewritten.

| Artifact | SHA-256 |
| --- | --- |
| Original `core_title.arc` | `15912eec3991614d92d098b240a81362af4eb593cf0fd3ca1d613279c9575227` |
| Derived ARC override | `1a5a6a58ae89b9b6265cb21eca0eb3df7ab26100575534f1847e9649aa436e7d` |
| Rebuilt translated LMD | `e891cabee236fcfeec2c235c8ad56f6f99e7159b8fad3d4fbe61ac2d896baf16` |

Azahar's log reported the LayeredFS replacement for `/eng/data/core_title.arc`.
CUA then observed **Menu principal** and **Opções de jogo** in the running
reference. The glyphs ç and õ were visible without clipping in those captures.
Main-menu buttons still displayed English, and not all eight modified fragments
were visually confirmed. A later [atlas investigation](localization-buttons.md)
located those graphical labels in TEX resources referenced by the title layout.

The original seed's draft/unverified status is retained as historical input;
this later observation is recorded separately in
`research/private/localization/title-reference/runtime-validation.json`, with
`main-menu.png`, `options.png`, ARC provenance and hashes. The separate runtime
and profile are described in [reference-runtime.md](reference-runtime.md).
This experiment does not validate fonts or layout across all game screens.

## Character creation draft and reference check

`localization/pt-BR/charmake.seed.json` adds 150 changed fragments from the
179-fragment CharMake source, with a glossary and draft status. Its 151 entries
also include the unchanged term HR; spaces, DUMMY and the colon remain outside
the seed. All 179 rebuilt strings were checked, with source identity, labels,
controls, placeholders and the order/count of newlines preserved. This is not
a completed human translation review.

The combined Title + CharMake override changes **158 fragments**. It was built
by applying the CharMake replacement to the separate Title override, keeping
both provenance reports. Its ARC SHA-256 is
`76b732cc639b2cf36bc54e254607a3e64f9ad25fd491fb3fd6f14c33919d2020`.
The translated CharMake SHA-256 is
`64991bae6d2a68891cbbc6925e7daacd68b904aaac27e1589ab87c1546f83eea`.

The reference was stopped through its UI before the project-owned override was
replaced, with a verified backup. After reloading the original ROM, captures
showed 14 translated CharMake fragments: header, rotation label, nine option
labels, the two name-help fragments and the name notice. Accents including
ç, ã, ê, á and ú appeared, with no visible clipping in these two captures.
The Confirm/Back buttons and keyboard controls remained in English in that
capture; the later Common experiment below resolved the two buttons. The other translated fragments have
not been visually confirmed, and the native engine still has no LMD renderer.

Captures, install provenance and the precise observed indices are in
`research/private/localization/title-charmake-reference/`. The earlier two-label
observation remains preserved in `title-reference/`. Newer reference evidence
does not silently rewrite the historical verification reports or input seeds.

## Shared buttons and local experimental package

A controlled replacement of Common fragments 60/61/62 displayed distinct
markers at Back/Confirm on the creation screen. It established fragments 60
and 61 as the consumers for those two buttons; fragment 62 remains unresolved
and excluded. The temporary override was removed and prior bytes verified.
The subsequent `common-buttons.seed.json` changes only 60 to **Voltar** and 61
to **Confirmar**. Both full words were observed inside the original button
bounds. Confirmar uses most of its width; this is one screen, not broad layout
coverage. See [the discriminant test and final validation](localization-common-buttons.md).

The three draft catalogs now change **160 fragments** (161 selected entries,
including unchanged HR). `localization/pt-BR/reference-package.json` pins eight
files by size and SHA-256: two generated ARC overrides, three catalogs and
three historical runtime reports. The package builder checks those identities
before copying them to `PT-BR-experimental/`. It reads frozen generated outputs,
not a running emulator profile. The native laboratory still does not consume
these game menus or LMD text; the experimental package serves the separate
reference runtime and does not constitute the complete translation.

Unknowns include the identifier/hash algorithm, the full command grammar, the
independent meaning of the repeated length field, broader font/glyph coverage,
the eight rejected LMD payloads and text outside the indexed LMD members. The total
game-wide translation coverage is unknown; no percentage is claimed.
