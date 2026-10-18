# Native model-channel sampler

`src/model_motion.hpp/.cpp` reads a bounded, source-bound native package and samples optional model translation/rotation in **source frames**. It does not decode LMT, select a clock, advance a loop, apply model motion, blend layers or choose missing/degenerate-channel defaults. The seven paired actions are available through this sampler without changing their frozen glTF or curve files.

Use `assets/local/model-motion-native-v2/model-motion.bin`, SHA-256 `ffb34e83d3f807272ca41b3a9d4d4cf4414735203a3cceaa4cd5d7ec2166d139`. It contains 7 actions, 8 present channels and 113 keys in 5,012 bytes. The earlier v1 binary, source snapshot and tests remain under `assets/local/model-motion-native-v1/` and `research/private/model-motion-native-v1/`; v1 pinned only glTF/curves and was superseded because external BIN/texture changes were not covered. The current parser accepts **v2 only**.

## API and integration identity

```cpp
#include "model_motion.hpp"
#include "model_motion_identity.hpp" // generated in the selected local asset directory

auto expected = mh4u::model_motion::generated::expected_package();
auto loaded = mh4u::model_motion::Package::load(binary_path, expected, paired_asset_root);
// Handle Result<Package> = std::variant<Package, Error> before sampling.
auto sample = package.sample(package.actions()[i].identity, source_frame);
```

The stable generated header is `assets/local/model-motion-native-v2/model_motion_identity.hpp`; its function is `mh4u::model_motion::generated::expected_package()`. It contains paths, hashes, IDs, clip indices, counts and presence metadata, with no animation values. Choose it independently of untrusted package bytes; deriving the expected hash from the file being loaded would remove the identity check. `identity.json` exposes the same fields for tooling.

`ExpectedPackage` owns the binary SHA-256, primary glTF/curve `AssetBinding`s, ordered `ActionInfo`s, and a `dependencies` vector. The 11 dependents plus glTF/curves cover **all 13 content files** in the paired package: animation/model BIN and PNG textures included. Only the location-dependent diagnostic animation provenance is excluded. This inventory comes from the paired producer; C++ does not reinterpret glTF to discover dependencies.

`Package::load(binary, expected, paired_asset_root, limits)` verifies the binary, serialized identities and every bound file under the explicitly selected root. Paths must be canonical relative ASCII without empty, dot, dot-dot, absolute, URL or backslash components. A symlink resolving outside the root is rejected. `Package::decode(bytes, expected, limits)` validates serialized bindings against the expected list but does **not** access the bound files; consumers loading real assets should use `load` or independently provide that verification.

`actions()[i]` exposes `identity.entry_id`, `identity.slot`, `identity.lmt_sha256`, `frame_count`, `loop_frame_raw`, `presence_mask` and **`gltf_clip_index`**. The clip index belongs to the hash-bound glTF; the source identity is the entry/slot/LMT hash tuple, not the display name or vector position. Wrong hash for an existing entry/slot yields `action_identity_mismatch`; an absent entry/slot yields `missing_action`.

`sample(identity, double source_frame)` returns `Result<Sample>`, where `translation` is `optional<array<double,3>>` and `rotation` is `optional<array<double,4>>`. The raw quaternion is XYZW and is not normalized. Absent channels remain `nullopt`. A present zero quaternion remains present zero; it is not a missing-channel signal or an instruction to substitute a reset template. A future application stage must choose its separately proven fallback policy; see [root-motion-defaults.md](root-motion-defaults.md).

Queries must be finite and within inclusive `[0,N-1]`; no clamping, extrapolation, looping or inferred FPS occurs. The paired glTF's one-source-frame-per-glTF-second convention is separate metadata for the caller, not a clock rule in this API. Existing raw coordinate-space limitations from [root-motion-curves.md](root-motion-curves.md) still apply.

## Binary contract: version 2

All integers are explicitly little endian. Hashes are 32 raw SHA-256 bytes. Floating fields are IEEE-754 **binary64**, read by byte assembly and bit-cast, independent of host byte order. No packed C++ structs or native pointer layouts are serialized.

| Record | Fields in order |
|---|---|
| Header, 24 bytes | ASCII `MH4UMC02`; u32 marker `0x01020304`; u32 total byte length; u32 action count; u32 reserved zero |
| Primary bindings | glTF binding, then model-curve binding |
| Binding | u32 path byte length; canonical ASCII path bytes; 32 hash bytes |
| Dependents | u32 count; ordered dependent bindings |
| Action, 64 bytes | u32 entry; u32 slot; 32 LMT hash bytes; u32 frame count; i32 raw loop; u32 presence mask; u32 channel count; u32 glTF clip index; u32 reserved zero |
| Channel, 16 bytes | u32 component (1 translation / 2 rotation); u32 interpolation (0 constant reference / 1 linear vector); u32 original track ordinal; u32 key count |
| Key | f64 source frame, then 3 translation or 4 raw quaternion f64 components |

Presence-mask bits correspond to component tags. Both may be absent in the native representation; current real resources contain translation in every selected action and rotation only in slot 92608:75. Constant raw rotations are supported; a linear-rotation tag is rejected until a separate contract is established. Source track ordinals are retained in binary provenance but are not used as array indices or runtime binding instructions.

Key frames start at zero, increase strictly, and end exactly at `N-1`. All frames/components must be finite. Channel counts and presence agree; duplicate components are rejected. Constant channels must retain identical numeric key values. Linear translation uses the surrounding owner-produced keys and `(1-t)A + tB`; exact keys and terminal endpoints return their stored values. No normalization, axis conversion or scale factor is applied.

Default local limits are 8 MiB package bytes, 64 actions, 250,000 keys **in aggregate**, 1,024 bytes per binding path, 64 asset bindings and 64 MiB bound-file bytes **in aggregate**. Counts are checked against both limits and remaining serialized bytes before allocating key arrays. File sizes are checked before allocation. Invalid expected identities, malformed bytes, limits, duplicate fields, nonfinite values and invalid timelines return tagged errors; no partial package is returned.

## Producer and preservation

`tools/resources/native_model_motion.py` verifies the caller-pinned paired manifest and all its declared files, re-extracts hash-checked LMT resources through the index owner, and revalidates each curve through `paired_actions.verify_curve_action` and `root_motion_curves.action_curve`. It serializes their already-decoded key facts; it contains no second LMT decoder.

The producer writes binary, generated identity header, JSON identity and `sample-oracle.tsv`. The oracle uses `animation_probe.sample_model_channel` and covers all **1,847 original integer/half-frame queries**, plus adjacent representable frame values around source keys, for **2,055 queries** total. Additional native tests reject queries just outside every action's bounds.

```sh
python3 -B tools/resources/native_model_motion.py \
  --paired-manifest assets/local/paired-player-actions-v1/manifest.json \
  --paired-sha256 30f85c1c2707cad8406b42d3e07bc2be2945f20ee781dbec3c05de075ffca261 \
  --index research/private/resources.sqlite \
  --output assets/local/model-motion-native-recheck \
  --research research/private/model-motion-native-recheck
```

Choose new output directories. Reproduction in a separate directory yielded byte-identical binary, header, JSON identity and oracle. Binary64 serialization introduces **zero extra numeric quantization** from the Python owner's double values. No double-to-float conversion is performed here; any future float application boundary must record its own narrowing error.

## Executed verification

Apple clang on macOS ARM64 compiled with C++20, warnings as errors and `-ffp-contract=off`. The normal and AddressSanitizer/UndefinedBehaviorSanitizer builds each passed **35,079 assertions**, including 2,055 real queries / 6,705 numeric components. Maximum absolute difference from the owner oracle was `8.470329472543003e-22`, RMS `1.791684513184274e-23`. This tiny difference comes from the Python point helper's arithmetic between identical reference keys; the native constant mode returns the raw reference directly. It is not serialization loss, an ARM comparison or a gameplay error bound.

Synthetic checks cover fractional interpolation, nonunit and zero raw quaternions, both absent channels, source/clip identity, every truncation point, count/size bounds, duplicate components, unsupported interpolation/version/endian flags, nonfinite data, timeline errors, unsafe paths, hash mismatches, aggregate file limits and a symlink escape. Four Python producer tests additionally cover binary64 values that float32 would lose, raw/absent distinction, path domains and hash rejection before side effects.

For all **13 actual content bindings**, a private copied file was altered individually and `load` rejected it with `binding_mismatch`. Originals remained hash-identical. Thus an unchanged glTF JSON cannot conceal an altered animation BIN, model BIN or texture in this package. The first compile exposed a signed-count warning, which was corrected before passing the warning-as-error build; the path test was corrected to account for macOS `/var` → `/private/var` canonicalization.

Standalone reproduction, without changing CMake:

```sh
clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off \
  -DMH4U_MODEL_MOTION_GENERATED -Isrc -Ivendor/picosha2 \
  -Iassets/local/model-motion-native-v2 src/model_motion.cpp src/model_motion_tests.cpp \
  -o research/private/model-motion-native-v2/model-motion-tests
research/private/model-motion-native-v2/model-motion-tests \
  assets/local/model-motion-native-v2/model-motion.bin \
  assets/local/paired-player-actions-v1 assets/local/model-motion-native-v2/sample-oracle.tsv
```

The tests also build without the generated-header define for synthetic checks alone. Evidence, compiler/test logs, negative controls, reproduction, extraction provenance and executed source snapshots are retained in `research/private/model-motion-native-v2/`, notably `validation.json` and `all-bindings-negative-controls.json`.

No original ARM instructions, game runtime, model application, clock advancement, viewer behavior or reset-template initialization was executed or inferred in this sampler work. Windows execution and CPU/RSS/performance comparisons were not measured. The parent task separately owns any CMake integration, composed tick experiment or viewer use.
