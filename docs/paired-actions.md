# Paired joint clips and model-motion curves

`tools/resources/paired_actions.py` exports the seven selected source actions as **7 joint clips with 140 joint channels**, together with their **8 separate model channels**. Each source track has exactly one declared destination. Model motion is not applied to any glTF node, and a missing model component remains absent/null in the preserved curve package.

The local result is `assets/local/paired-player-actions-v1/manifest.json`, SHA-256 `30f85c1c2707cad8406b42d3e07bc2be2945f20ee781dbec3c05de075ffca261`. It uses the existing `player-assembly-probe/seven-piece-candidate` rest asset: 39 nodes, 19 joints, 20 meshes/primitives. Nodes and skins are unchanged; this export does not improve or establish the correctness of the candidate's equipment assembly, materials or appearance.

## Ownership and explicit separation

`animation_probe.py` continues to own LMT parsing, channel classification, sampling and glTF construction. Its default `prepare_action` and `export_gltf` behavior is retained. The old joint-only profile still rejects nonzero/unrecognized scene tracks and still omits only its previously accepted zero translations.

The new keyword-only `model_curve_bindings` argument to `export_gltf` accepts one `ModelCurveBinding` per selected action. A binding contains source LMT hash, slot, indexed entry, curve index, local sidecar URI/hash and the exact ordered model-track ordinals. The common preparation boundary verifies the identity and full model-track partition. In this explicit profile, **type 3/4 selects model channels**, independent of raw bone ID; types 0/1/2 must resolve through the existing joint map. Duplicate model components, unsupported model samplers, missing joint mappings or incomplete bindings fail the complete paired export. No failed action is silently skipped.

The producer validates the caller-pinned curve-package hash, verifies each indexed resource name/archive/type, re-extracts through the existing extraction owner and checks payload hashes. It then reproduces every curve action through `root_motion_curves.action_curve` and compares all fields, samples and absent/null markers. No decoder, compressed-key equation or skin parser is duplicated. Only after those checks does it copy the original curve-package bytes unchanged and export joint clips with explicit external-model references.

Source actions are 54913:3, 54913:5, 92608:18, 92608:75, 92608:82, 92608:101 and 92608:1. No semantic names such as walk/run are assigned. The raw curves and their evidence boundaries remain described in [root-motion-curves.md](root-motion-curves.md). The rotation channel is present only for 92608:75; the other six actions still have no source rotation track. The package does not insert one, even if a runtime consumer later implements a separately proven missing-track fallback.

## Consumer contract

Schema `mh4u-paired-actions-v1` links:

- `joint_gltf`: `joints/animated.gltf`, containing only local joint animation channels.
- `model_curves`: `model-curves.json`, byte-identical to the prior source-bound curve package, SHA-256 `4fb9a97a7383044ab00140f22581d4bd894543d332a056d1a72628e7593231ee`.
- `files[]`: relative URI, SHA-256 and byte length of every packaged file other than the manifest itself. A consumer must verify these identities before trusting a pair.
- `pairs[]`: LMT hash, indexed entry and slot; glTF clip index; curve action index; source frame count/end and raw loop; exact joint and model track ordinals; absent model components; `runtime_default_binding: null`.

Indices are positions within these pinned files, not stable game identities. Use `(source_lmt_sha256, source_slot)` together with the indexed entry to identify an action. Every glTF clip repeats the source hash/slot and its sidecar URI/hash/curve index in `extras.model_channels_external`; a mismatch is an error, not a cue to choose a similarly named clip.

glTF time is formally in seconds. For this diagnostic package, the declared conversion is **one source frame per glTF second**: `source_frame = gltf_seconds × 1`. This preserves integer/half-frame coordinates without assuming the game's rate; it is intentionally not a playback-speed claim. A future consumer must consciously map its source-frame clock into the clip's glTF time domain. Endpoints are source `N-1`. The raw loop field is preserved; this export does not advance loops or assign stop/loop behavior.

The model curves stay in original XYZ/XYZW and remain separate from joints and the glTF root. Reading the glTF alone yields only the joint portion. Applying a model sample still needs a separately specified previous state, clock, model transform, attributes, defaults and controller/event policy. The manifest records `model_motion_applied: false`, `runtime_defaults_applied: false`, `controller_policy: null` and `enabled_for_playback: false`.

The manifest is written only after every selected action and track partition succeeds. Failed runs may retain private extraction/partial diagnostic outputs for inspection, but do not emit a success manifest. Existing output/research directories are rejected rather than overwritten.

## Reproduction

Use new directories:

```sh
python3 -B tools/resources/paired_actions.py \
  --index research/private/resources.sqlite \
  --curves assets/local/root-motion-curves-v1/curves.json \
  --curves-sha256 4fb9a97a7383044ab00140f22581d4bd894543d332a056d1a72628e7593231ee \
  --model assets/local/player-assembly-probe/seven-piece-candidate/scene.gltf \
  --output assets/local/paired-player-actions-recheck \
  --research research/private/paired-player-actions-recheck
```

The resulting joint glTF SHA-256 is `e058f9074f34bbabd07e47ab4e8345d08981e1b6c55fc4a737cd1b46dd3feec4`; its animation binary is `6068f5a758083e2273135b0e8bf4e3b9e70c33f7adbd1122843530637805ce00`. Regeneration into another directory reproduced all **13 content files** byte for byte and retained identical source/pair contracts. The diagnostic `animation-provenance.json` contains absolute extraction/output paths, so its hash—and the manifest hash that includes it—changes with output location. The report does not claim byte-identical location-dependent provenance.

## Validation and limits

32 tests passed: 8 new paired-boundary tests plus 24 curve/vector/quaternion regressions. Negative cases cover wrong hash/slot, incomplete track partitions, missing bindings, duplicate model components, unresolved joints, modified samples and fabricated fallback rotation. A source snapshot of the five producer/owner modules is retained under `research/private/paired-actions-v1/production/producer-snapshot/`.

Khronos glTF Validator **2.0.0-dev.3.10** reported zero errors, warnings, infos or hints. The existing macOS native loader/sampler then validated:

| Comparison | Queries | Maximum measured error |
|---|---:|---:|
| Joint rotation: independent source-byte decoder/raw nlerp versus native glTF sampler | 19,229 | `0.000014083561564830538°` |
| Joint translation: owning source-vector decode plus linear arithmetic versus native glTF sampler | 1,847 | `0.000012056807065619068` per component, source units |

Rotation verification reused `verify_rotation_curves.py`; translation verification retained the owning decode and checked native results at the paired integer/half-frame timeline. These are discrete checks of source-derived mathematical curves against the native glTF sampler, not VFP bit equivalence, original gameplay timing or an observation of the game/controller. No original ARM instructions or game runtime were executed for this work. Windows execution, full rendering/appearance and applying model motion together with joints were not tested here.

Legacy exports were separately regenerated for `body001-actions`, `male001-actions` and `seven-piece-actions`. Each still produced **44 accepted actions**, byte-identical glTF and animation binary, and the exact same **78 rejected-action records**. The frozen files themselves still match their original hashes. No viewer, CMake, package recipe, root-motion C++ implementation or fixture was changed.

Evidence is under `research/private/paired-actions-v1/`: `validation.json`, `khronos-validation.json`, `native-rotations.json`, `native-translations.json`, `legacy-preservation.json`, `legacy-assembly-preservation.json`, `paired-reproducibility.json` and test logs. Extraction/producer provenance is `production/provenance.json`. CPU/RSS, cold/warm performance and speed improvements were not measured or claimed.
