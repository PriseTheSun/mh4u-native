# MH4U animation probe

## Concrete result

`tools/resources/animation_probe.py` catalogs motion-list resources from the
existing verified index, extracts selected members through its hash-checking
reader, decodes LMT version 67, and exports supported local channels into glTF2.
It uses Python's standard library and does not execute game code.

The complete ARC index contains **1,466 rMotionList members**. This probe decoded
only three members from `/data/em062.arc`: **36 actions, 468 tracks and 17,149
stored keys**. The selector artifact contains **28 supported actions** using the
actual model, skeleton and motion data:
`assets/local/animation-probe/em062-actions/animated.gltf`. Eight actions are
individually rejected because scene-root binding is unresolved. All observed
codecs decode; no codec coverage is inferred for the remaining 1,463 members.

All consumed animation artifacts were regenerated under decoder profile
`mh4u-lmt67-arm-sampler-v2`. Shipped ARM evidence corrected vector equations
and timing in v1, followed by quaternion codecs and interpolation in v2.
The complete v1 producer, documentation and four outputs are preserved under
`research/private/animation-corrections/20261017-arm-sampler-v2/before/`, with
a digest manifest. No original-game execution is claimed.

The clip is identified as `entry-9521 / ação 002`, with 27 source frames and
source loop value 0. Neither names nor structure establish that it is an idle,
walk or run, so no gameplay label was assigned. Its 60 FPS conversion rate is an
explicit preview convention, not a recovered original playback speed. The
resulting preview domain is frames 0–26, or 26/60 = 0.433333 seconds.

## Identity and retained artifacts

The source ROM is the user-supplied `CTR-P-BFGP`, title `0004000000126100` (EU),
SHA-256 `c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`.
The selected ARC's SHA-256 is
`2795486192ad9f1a6689078068fd7656e183b3e5149383facd4892782760d3c9`.

| Index ID | Internal member | Bytes | Actions | Decompressed SHA-256 |
| --- | --- | ---: | ---: | --- |
| 9521 | `enemy\em062\mot\em062_0` | 50,208 | 17 | `ab3cf09bc5cd2ef93ef2a98e2a54634b1910fea38d7898b264f8d9345668aafa` |
| 9522 | `enemy\em062\mot\em062_2` | 57,852 | 9 | `736d7e075b79af484d669ddc0f45f8878391accc13fba2dde88dc7c28d490f83` |
| 9523 | `enemy\em062\mot\em062_3` | 56,464 | 10 | `e3ae4b66c760c18deb1cf9c72cbe12acabfce0883631092cb1fe075375f2e06c` |

`research/private/animation-probe/indexed/` retains the complete motion-member
catalog, three extracted LMTs, extraction provenance and decoded JSON. Every
track retains its original ordinal, bone ID, record offset/raw bytes, payload
offset/size/digest, reference values, quantization values and stored frame
deltas. These are ordinary local-tool records, not REA Evidence IDs.

The single-action glTF SHA-256 is
`9b2aec8091b055efb9f71a281bece40f71c2e0cb0abc184139d078681a0caa01`.
The selector glTF SHA-256 is
`0d8bfba19e56f5b3b50ccd02cb9bf0dc794b2b7fc4dc63515c2b4d251838aa33`.
Both directories contain copied model dependencies, `animation.bin` and
`animation-provenance.json`. The latter lists each rejected action with a reason.
The original `assets/local/actor-probe/scene.gltf`
was not modified. All proprietary input and derived model/animation data remain
under ignored local/private directories.

## Observed LMT67 layout

All three members begin with `LMT\0`, a little-endian 16-bit version 67 and a
16-bit action-slot count. A table of 32-bit file-relative offsets follows at
offset 8. Slot 0 is empty in each sample; empty slots are retained.

The action fields occupy 60 bytes: track-table offset at `+0`, track count at
`+4`, frame count at `+8`, signed loop frame at `+0x0c`, scene-end position at
`+0x10`, scene-end quaternion at `+0x20`, flags at `+0x30`, event pointer at
`+0x34` and float-track pointer at `+0x38`. Action starts in the observed tables
are normally 64 bytes apart, but the final header's following data begins after
its 60 fields; the reader does not invent trailing fields. All observed flags
are `0x00800000`, all float-track pointers are zero, and event payloads remain
unparsed.

Frame counts range from 3 to 363. Source loop-frame values are -1 for 22 actions,
0 for 10 actions, and 32, 38, 42 and 60 for one action each. Positive source loop
frames are retained in glTF extras; a viewer looping the entire clip does not
implement these source loop positions.

Each bone-track record is 36 bytes: codec/type/bone-type/bone-ID bytes,
float weight, buffer byte count and pointer, four reference floats, and a
quantization-block pointer. All 468 observed bone-type bytes are zero and weights
are one. Every action has the same 13 bone IDs: scene ID 255, followed by
`0, 1, 140, 141, 142, 143, 144, 200, 4, 3, 100, 2`.

| Codec | Interpretation used | Tracks in the three files |
| --- | --- | ---: |
| 1 | Vector reference only in these samples | 35 |
| 2 | Quaternion reference only in these samples | 66 |
| 4 | Three biased unsigned 16-bit components + 16-bit frame delta | 32 |
| 5 | Three biased unsigned 8-bit components + 8-bit frame delta | 5 |
| 6 | Four signed two-complement 14-bit quaternion components + 8-bit frame delta | 240 |
| 7 | Four biased unsigned 7-bit quaternion components + 4-bit frame delta | 70 |
| 11 | X/W biased 14-bit components + 4-bit frame delta | 20 |

Quantized records use four scale floats followed by four bias floats.
For vector codecs 4/5, the shipped consumer uses
`component = bias + scale * ((integer - 8) * literal)` without clamping.
The exact float32 literal bits are `0x37800801` for codec 4 and `0x3b888889`
for codec 5, approximately 1/65520 and 1/240. These replace the previously
assumed integer/65535 and integer/255 formulas. The quaternion consumers were
then audited against the same shipped ELF; see [quaternion-codecs.md](quaternion-codecs.md)
for addresses, bytes, operand order and static provenance. Codec 6 uses signed
14-bit two-complement fields divided by 4096. Codec 7 subtracts 8 with uint32
wrap, converts to float32, multiplies by literal bits `0x3c124925` and applies
scale/bias. Codecs 11/12/13 subtract 8 from their axis/W fields and multiply by
literal bits `0x38802008`, retaining the other components' bias.

Codecs 1/2 always sample their track reference, even if a buffer is present;
stored payloads remain inspectable but are ignored by export sampling.
Nonempty codecs 1/2 and codec 3 have no real-corpus coverage here. Unknown
codecs remain undecoded and cannot be exported. The player investigation
adds codecs 12/13 (YW14/ZW14) to the same decoder boundary; they do not occur
in these em062 samples. Across all five sampled LMT, the quaternion audit
covers 2,598 tracks and 73,813 keys. Player coverage is in
[docs/player-findings.md](player-findings.md).

## Primary-source cross-check

The format is independently implemented from the bytes, checked against
PredatorCZ/RevilLib commit `e17f9718ea571f040a7e1f331c28a4d45a6066b5`:

- [Generated action layout](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/animation.inl) and
  [bone-track layout](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/bone_track.inl)
  corroborate field offsets for version 67 with 32-bit pointers.
- [Track decoder selection and sampling](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/bone_track.cpp)
  associates transform types with local rotation/position/scale and separate
  scene-space variants. Its generic reference-frame insertion convention was
  superseded here by the shipped MH4U sampler evidence below.
- [Codec equations](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/codecs.cpp)
  and [codec field definitions](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/codecs.hpp)
  informed bit order, frame deltas and linear/spherical interpolation families.
  The shipped vector and quaternion consumers supersede the generic quantization
  and interpolation interpretation.

Pinned source files, their individual SHA-256 values and upstream license are
retained privately under `research/private/animation-probe/references/`. No
Blender add-on or downloaded source code was executed. These checks establish a
format interpretation, not exact game-runtime equivalence.

REA/Ghidra analysis of the shipped ARM executable independently corroborates the
container structure. Function `0x23e3d8` validates LMT magic `0x544d4c` and version
`0x43`, reads the count at `+6` and pointer table at `+8`, then relocates track
records with stride `0x24`, buffers at `+0x0c` and quantization blocks at `+0x20`.
The quantization relocation excludes codecs 0, 1, 2, 3, 6, 8 and 9. Track
relocation is skipped for action flag `0x01000000`; event relocation has its own
flag and nested pointers. The export accepts only the observed `0x00800000`
action flags. This loader evidence alone establishes parsing; the later sampler evidence
below establishes the narrower vector and timeline corrections.

REA Evidence `ev_2341ca4a57100d3cb50d1e37279681b559e3e148d294bc09ad4206286775a533`
is retained in
`research/private/rea-session-1792271307255/023-procedure_pseudo_code.json`,
associated with analysis ELF SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
The separate `rMotion` loader at `0xaed2a8` validates MOT version 65. Its header
and getter offsets are not substituted for this LMT layout.

## glTF binding and conversion

The selected model's MOD-derived IDs are retained in node extras as
`sourceFunctionIdCandidate`. Every non-scene motion ID maps uniquely to this
field, corroborating the binding for this model. Nodes 0–11 and their hierarchy
remain unchanged. Their rest rotations are identity, and their unanimated local
translations are preserved.

Every exported action has one local translation channel targeting bone ID 0 and
eleven local rotation channels. No scale channel is invented. Animated channels
replace their respective local TRS component; unanimated components keep the
original rest value. The scene-root track for bone 255 is identically zero in
each exported action and is omitted with an explicit provenance record. The
exporter rejects nonzero scene motion until it has an explicit scene binding.
The selector rejects these exact slots, with error `unsupported_scene_motion`:

| Member ID | Rejected populated action slots |
| --- | --- |
| 9521 | 4 |
| 9522 | 1, 2, 5, 6, 8 |
| 9523 | 1, 2 |

Names use `entry-NNNN / ação NNN`; they do not imply gameplay semantics. At the
explicit 60 FPS preview convention, supported durations range from 2/60 to
362/60 seconds (0.033333 to 6.033333), before float32 serialization.

Stored deltas accumulate from frame zero with no synthetic reference key or
one-frame shift. The common initializer starts the buffer cursor at zero;
codecs 1/2 instead return the track reference directly. Source loop metadata
does not alter this initial sampling rule. The controller domain ends at
`frame_count - 1`. Export samples that endpoint using any following raw key
before trimming the tail; a terminal zero-delta key holds its value until the
endpoint. All raw keys and deltas remain in decoded JSON. Non-reference codecs
with empty buffers or nonzero terminal delta are rejected, rather than falling
back to the reference or guessing a missing next key.

Quaternion values use XYZW order. Buffered quaternion channels use glTF
`CUBICSPLINE` with unit key values and separate incoming/outgoing tangents per
second. The tangents represent the derived native curve: raw quaternion lerp,
with the second endpoint negated only when the original raw dot is negative,
followed by normalization of the result. A positive scalar quadratic preserves
that direction while satisfying glTF's unit-key constraint. This construction,
sign transport including exact-zero dot, and raw endpoint clipping are derived
in [quaternion-codecs.md](quaternion-codecs.md). Tangents are not normalized.

Reference quaternion tracks and single-frame channels remain constant LINEAR;
vector channels remain LINEAR. Generic glTF LINEAR/slerp semantics in the native
consumer were not changed. Times are rounded to their stored float32 values
once; adjacent equal or nonfinite times reject export. Tangents use those stored
second intervals. Quantization uses exact float32 constants with Python double
arithmetic, except codec 7's explicit uint32→float32 conversion. Per-instruction
VFP rounding and the original FP environment are not emulated. This preserves
the derived mathematical curve to measured serialization/runtime precision,
not bit-exact ARM behavior.


## Historical v1: vector and timeline correction

The read-only REA/Ghidra session against analysis ELF SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`
retains the evidence in `research/private/rea-session-1792274318294/`:

| Observation | Address | Retained record / REA Evidence |
| --- | --- | --- |
| Codec 4 vector consumer and literal | `0xae9910` | `075-read_bytes.json`, `ev_9f30f4f3d29cfe67b5e7931b86ab1bea6a5424568cac03c2dda1d9a5692c214c` |
| Codec 5 vector consumer and literal | `0xae92e4` | `076-read_bytes.json`, `ev_dfbc336ac239e12306c696c4a0f7022e2d6d64553b55f1ae440b0fdbe70f84a2` |
| Reference-only codecs 1/2; common buffer sampler | `0xaea0e4` | `036-procedure_assembly.json`, `ev_02441f313eda39bd06c1494f9ebfa8bec85876427ca64be0dde2515738c9d660` |
| Zero cursor and buffer initialization | `0xae923c` | `035-procedure_assembly.json`, `ev_dd2d5709fb192acce83e6f7116cdeb0e0105d7ed2b39752aa6b21c562e2e8159` |
| Binder stores frame count minus one | `0x95aae0–0x95aaf4` | `030-procedure_assembly.json`, `ev_536563d3f719d8414b35f84687804ac09d3f3d0ced61bd6608dd9e918e4a0bda` |

Dispatch bytes and delta getters are retained in records 068, 069 and 074;
float32 interpolation literal 1.0 is in 077/078. The vector consumers subtract
integer 8, convert to float32, multiply by the literal and apply scale/bias.
Intermediate samples use `(1-t)*current + t*next`; zero delta selects the
current terminal key. These are static observations/derivations, not ARM
execution or original-game playback comparisons. Further ownership/controller
findings are in [root-motion-findings.md](root-motion-findings.md); no scene/root
motion was enabled by this decoder correction.

Across the five examined LMT files, 225 vector tracks contain 6,635 keys whose
values change, with maximum component difference 1.086668486 source units.
There are 2,823 buffered tracks; 2,382 previously received a synthetic reference
because of loop metadata. At that revision, quaternion equations, frame deltas, source identities,
joint/channel targets and model dependency bytes were unchanged. The v2
quaternion correction below supersedes only that equation/interpolation claim.
The accepted/rejected sets remain exactly 28/8 for em062 and 44/78 for player.
Both player exports still have 34 distinct numerical channel sets.

`verify_animation_revision.py` compares the preserved decoder/exports to current
ones and validates these invariants and every exported channel's endpoint.
The report `research/private/animation-corrections/20261017-arm-sampler-v1/revision-verification.json`
contains all four old/new glTF and animation BIN hashes. In each artifact,
`animated.gltf`, `animation.bin` and `animation-provenance.json` changed; model
geometry, textures, labels, clip order and rejected actions did not. Provenance
now includes the decoder profile, producer hash, ARM evidence IDs and animation
BIN digest, in addition to model dependency identities.

Eight new tests include analytical vector endpoints, unclamped values, common
cursor timing across loop metadata, reference-only tracks, tail interpolation,
constant holds, malformed/unterminated buffers and a public CLI export.
The three quaternion tests then passed unchanged; v2 updates their expected
values to the later shipped-consumer evidence. Real-data readbacks
passed 120 em062 checks, 53 player checks and 231 assembly checks. All four
regenerated artifacts passed Khronos Validator 2.0.0-dev.3.10 with zero errors,
warnings, infos or hints. Native scene tests passed for em062, player and the
assembly; temporary renderer captures of corrected slot002 and assembly exited
zero. Screenshots establish successful native rendering, not original fidelity.

## v2 validation and remaining work

The v2 revision changes all 73,813 stored quaternion keys in 2,598 tracks
(maximum raw-component difference 0.0492210613). Vector decode values, raw key
timing, source identities, model dependencies, clip order/channel targets and
accepted/rejected action sets remain unchanged. Four existing artifacts were
exported into staging, validated, then regenerated at their final locations.
Every glTF/BIN/model dependency is byte-identical to staging; provenance paths
were regenerated for the final directory. The fifth artifact adds the separately
validated seven-piece static candidate, using the same 44 accepted player clips.

All 63 resource tests pass, including 19 vector/timeline/quaternion cases.
The native boundary fixture includes a segment cut before its next raw key,
unequal endpoint magnitudes and a negative-dot segment followed by exact-zero
dot. Its 14 native samples differ by at most 0.000007565° from the independent
raw-nlerp oracle. Codec 7 unsigned underflow and its VCVT rounding are analytical
tests; no fields below 8 occur in the real sampled corpus.

`verify_rotation_curves.py` reads raw track records and quantized payloads
independently, without importing the animation producer. Its oracle evaluates
original pairs using raw nlerp; it does not use Hermite/tangent equations.
It queries the actual C++ `load_gltf` and `sample_animation` through
`mh4u-animation-samples`, at each exported key and quarter/mid/three-quarter
segment points. Five final artifacts account for 225,131 queries:

| Artifact | Queries | Maximum angular difference |
| --- | ---: | ---: |
| em062 slot002 | 423 | 0.000006226° |
| em062 28-action selector | 38,372 | 0.000103087° |
| Player body 44-action selector | 62,112 | 0.000053026° |
| Five-piece 44-action selector | 62,112 | 0.000053026° |
| Seven-piece 44-action selector | 62,112 | 0.000053026° |

These real accepted tracks have no internal frameMax cuts; that case is covered
by the native synthetic fixture, not inferred from real-data coverage. The
measured values are sampled differences against a double oracle with exact
float32 constants, not a continuous bound or original-game comparison.

The four original outputs also pass 120 em062, 53 player and 231 assembly
readback checks. These inspect bounds, finite arrays, increasing times, unit
quaternion key values at CUBICSPLINE rows `1::3`, complete slot accounting and
unchanged geometry/rig. Keys-only readbacks do not establish interpolation;
the independent native comparison above supplies that separate check.
All five artifacts pass Khronos Validator 2.0.0-dev.3.10 with zero errors,
warnings, infos or hints. Reports, before/replaced trees, staging and hashes
are retained under `research/private/animation-corrections/20261017-arm-sampler-v2/`.

Serialized key-only errors remain small: em062 slot002 position 1.066379e−7
source units and rotation 2.330303e−6°; the 28-action selector position
1.482283e−5 and rotation 4.789828e−6°. Times are compared after the producer's
explicit float32 storage boundary. Historical decoded JSON remains tied to its
original producer; parse the retained raw LMT or inspect into a fresh directory
to obtain v2 decoded values.

Input files are limited to 64 MiB, cumulative tracks to 4,096 and decoded keys
to 250,000 per inspection, as explicit local resource policies. File counts and
buffer bounds are checked before iterating or expanding them. Output files are
idempotent when equal and reject replacement of differing data.

The remaining work includes identifying semantic action names, verifying the
game's playback rate and end-to-end playback behavior, decoding gameplay events and
float tracks, supporting nonzero scene/root motion, and testing more skeletons
and codec families. No original game runtime, original motion playback capture,
or gameplay event execution was used to validate this probe.

Reproduce extraction and export:

```sh
python3 -B tools/resources/animation_probe.py inspect \
  --index research/private/resources.sqlite --entry 9521 --entry 9522 --entry 9523 \
  --output research/private/animation-probe/indexed-v2
python3 -B tools/resources/animation_probe.py export \
  --lmt research/private/animation-probe/indexed/entry-9521.lmt \
  --model assets/local/actor-probe/scene.gltf --slot 2 --fps 60 \
  --output assets/local/animation-probe/em062-slot002
python3 -B tools/resources/animation_probe.py export-many \
  --lmt research/private/animation-probe/indexed/entry-9521.lmt \
  --lmt research/private/animation-probe/indexed/entry-9522.lmt \
  --lmt research/private/animation-probe/indexed/entry-9523.lmt \
  --model assets/local/actor-probe/scene.gltf --fps 60 \
  --output assets/local/animation-probe/em062-actions
python3 -B research/private/animation-probe/verify_animation.py --output /tmp/em062-v2-check.json
python3 -B tools/resources/verify_rotation_curves.py \
  --native build/mh4u-animation-samples \
  --asset assets/local/animation-probe/em062-actions/animated.gltf \
  --output /tmp/em062-native-curves-v2.json
```
