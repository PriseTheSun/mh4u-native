# BSS mapping for REA static analysis

The derived ELF now maps a 636,332-byte BSS at `[0x01081B7C, 0x0111D128)`.
This is a static inference corroborated by exact startup instructions and the
extracted exheader metadata. No game code was executed.

## Evidence and inference

The verified `code.bin` has SHA-256
`63940d7ef1fecc119f9fb820f5f6a2cf2f2a5549e4a70f00319fbd6c9c1ad8dc` and
16,261,120 bytes. Its first ARM instruction is `0xEB000007`: at text address
`0x00100000`, this branches with link to `0x00100024`.

The seven words at code offset `0x24` are:

```text
e59f0014 e59f1014 e3a02000 e1500001 34802004 3afffffc e12fff1e
```

LLVM's ARMv6 disassembler decodes them as loading two literal addresses into
`r0`/`r1`, setting `r2` to zero, comparing `r0` with `r1`, conditionally storing
zero with a four-byte post-increment, looping while unsigned-lower, and
returning. The literal words at code offsets `0x40` and `0x44` are
`0x01081B7C` and `0x0111D128`. They are data, despite a linear disassembler's
attempt to interpret the final word as an instruction.

The interval is therefore the zeroing loop's statically derived memory range:

```text
data virtual address      0x00EC0000 = 15,466,496
initialized data size     0x001C1B7C =  1,842,044
start of zeroing range    0x01081B7C = 17,308,540
end of zeroing range      0x0111D128 = 17,944,872
range size                0x0009B5AC =    636,332
```

The start equals the exheader-derived data address plus initialized size; the
range size equals the declared BSS size. The generator retains the source
manifest's successful exheader-hash check, but does not independently re-read
the ROM exheader. Startup addresses and loop behavior are static observations
and derivations, not proof that execution reached this routine.

## Generator behavior

`tools/make_analysis_elf.py` defaults to profile `mh4u-63940d7e`. Before
allocating ELF content it verifies the manifest's code digest/size, source
bounds, segment order, exact supported segment map, first branch, complete
seven-word stub, exact literal interval, declared BSS, pinned full code digest,
parent ROM identity and source exheader verification. Any mismatch returns a
structured error and produces no output. Other builds require a separately
established profile; a matching-looking loop alone is insufficient.

The writable PT_LOAD segment now has `p_filesz = 1,842,044` and
`p_memsz = 2,478,376`. `.data` contains only initialized bytes. `.bss` is
`SHT_NOBITS` with write/allocate flags and four-byte alignment. The original
data page has 1,156 trailing zero-padding bytes; these are verified as zero and
omitted from file-backed data, with their source range and digest recorded in
provenance. `.text` and `.rodata` headers, addresses and payloads are unchanged.

The explicit `--bss-profile none` option retains the old mapping without BSS
and records that omission. It reproduces the earlier ELF byte for byte. This
option does not infer a different BSS range. Existing output and provenance
files are never overwritten.

## Generated artifacts and checks

The new private artifact is `research/private/mh4u-analysis-bss.elf`:

- Size: 16,264,336 bytes.
- SHA-256: `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
- Provenance: `research/private/mh4u-analysis-bss.provenance.json`.
- Previous ELF retained: SHA-256
  `2a720851f228ba183073e3c9eed7028d18e7e51901a56f277178e1707beeeef2`.

LLVM `llvm-readobj --file-headers --program-headers --sections` accepted the
new artifact; its complete output is `research/private/bss-elf-headers.txt`.
The startup disassembly is `research/private/bss-startup-disassembly.txt`.
`research/private/bss-verify.py` passed 23 checks, retained in
`research/private/bss-verification.json`: independent ELF header/range checks,
source-byte equality, unchanged earlier artifact, explicit no-BSS equivalence,
and seven rejection cases. Those cases include changing the stub while also
updating the manifest digest, changing a literal, changing an unrelated code
byte, and changing initialized/BSS sizes. Rejections were exercised through the
actual CLI and verified to leave neither ELF nor provenance output.

Reproduce the generator with a new destination:

```sh
python3 tools/make_analysis_elf.py \
  ../rea-main/.cache/mh4u-native-2026-10-17/extracted \
  research/private/mh4u-analysis-bss.elf
```

This artifact is a synthetic ARM analysis carrier for REA/Ghidra. It is not a
native PC executable, an original game ELF, or a claim about the game's ABI.
The verification records are ordinary local-tool evidence, not REA Evidence
records. Ghidra import and function analysis are separate validation steps.

## Profile label correction

The source identity is `CTR-P-BFGP` / title `0004000000126100` (EU). The profile
now uses only the code-digest prefix, without a regional suffix. This corrects
the earlier US label; it does not change the supported bytes, ELF, mapping or
verification conclusions. The prior sidecar was preserved as
`research/private/bss-provenance.before-profile-label-correction.json` before
the two profile-label fields were corrected. The sidecar records this as a
label-only metadata change. No extraction, generation or analysis was repeated.
