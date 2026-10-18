# Source-bound model-motion curves

The two inspected player LMT67 resources contain **122 populated actions and 125 model channels**. The current joint-only exporter still accepts 44 actions and rejects 78 with `unsupported_scene_motion`. All 125 model channels can be sampled by the existing, source-derived codec subset; that result does **not** establish a controller, gameplay speed, event behavior or permission to bind them to a joint.

`tools/resources/root_motion_curves.py` exports a separate diagnostic package. It does not change frozen glTF exports, the viewer, the C++ application/clock library or its fixtures. `animation_probe.py` remains the owner of LMT parsing, decoding, clipping and point sampling. The adapter selects and packages its results; it contains no second LMT decoder.

## Binding and coverage

The retained binder at `0x95aa90` selects **type 3 (model rotation)** and **type 4 (model translation)** for samplers in the model's animation layer. Types 0/1/2 use the model's joint map. Every model channel in these two resources happens to have raw bone ID 255, but the type is the proven selector; bone 255 is not treated as a universal model binding. See [the source investigation](root-motion-findings.md), Evidence `ev_0efaabf914e4e2147baee4244d36d37940dfa9f09102fe65c14cb6348d918fcd`.

| Indexed entry | ARC / resource | Populated / slots | Joint-export accepted / rejected |
|---|---|---:|---:|
| 54913 | `/data/pl_mot_lco.arc` / `player\mot\co\lco_0` | 19 / 68 | 10 / 9 |
| 92608 | `/loc/data/pl_base_0.arc` / `player\mot\co\co_0` | 103 / 136 | 34 / 69 |

Source payload SHA-256:

- 54913: `dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a`
- 92608: `c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8`

Both were freshly extracted through the indexed extraction owner. The selected compressed hashes were verified against `research/private/resources.sqlite`; the resulting payloads also match the prior local extractions. This run did not rehash the entire ROM. Its indexed ROM identity, resource names, ranges, compressed/payload hashes and extraction flags are retained in private provenance.

| Component / codec | Count | Observed behavior |
|---|---:|---|
| Type 4 / codec 1 | 53 | Reference vector: 44 exactly zero; 9 constant nonzero |
| Type 4 / codec 4 | 65 | Variable vector, biased 16-bit components |
| Type 4 / codec 5 | 4 | Variable vector, biased 8-bit components |
| Type 3 / codec 2 | 3 | Constant raw quaternion reference |

The 78 rejected actions therefore comprise **69 variable translations plus 9 constant nonzero translations**. The constants occur in entry 92608 slots 47, 82, 95, 96, 108, 110, 111, 112 and 132. They remain explicit numeric values; no tolerance silently turns them into zero. Their constant value does not establish what an initialized controller would do with the first sample.

The only model rotations occur in entry 92608 slots 75, 76 and 87, with raw XYZW `[0, 2.4492937051703357e-16, 0, 1]`. These are approximately identity and provide no demonstrated turning motion. Missing rotation channels are recorded as absent, with `default: null`; the adapter does not fabricate identity. Unknown model codecs or duplicate component tracks fail explicitly. `inventory.json` retains every populated slot and its individual joint-export result, including all 78 rejections.

## Point-sampling contract

The owning function is:

```python
animation_probe.sample_model_channel(track, action, frames) -> list[list[float]]
```

Inputs are already parsed owner records. The accepted subset is type 4 with codec 1/4/5, or type 3 with codec 2, plain `bone_type_raw=0`, weight 1. Queries must be finite numbers in `[0, frame_count-1]`; out-of-range, boolean, nonfinite or excessive queries fail with `ProbeError`. The function reuses `raw_channel_keys(..., 1)` and evaluates the resulting vector curve. The divisor one retains native integer frame coordinates exactly; it is not a claim that the game runs at one frame per second.

Codec 1/2 samples preserve their raw reference, even when a payload exists. The model rotation reference is not normalized by this boundary. Codec 4/5 samples use the already corrected linear vector curve, including terminal holds and interpolation to frame `N-1` before clipping a stored tail. No loop advancement, previous-sample delta, axis change, scaling or model transform is performed. The new function does not change existing glTF-export functions or their results.

`assets/local/root-motion-curves-v1/curves.json` uses schema `mh4u-model-motion-curves-v1`:

- `sources[]`: indexed entry, exact ARC/resource name, LMT hash and size.
- `actions[]`: source entry/hash/slot/action offset, raw header, frame count/end, raw loop, action flags, raw end translation/rotation and unparsed event/float-track offsets.
- `sample_frames[]`: every integer and half-integer in `[0,N-1]` for that action.
- `channels.translation` / `.rotation`: present/absent status. Present channels retain raw track fields, compressed-data identity, codec, reference, scale/bias, clipped keys and one `samples[]` vector per `sample_frames[]` item. Absent channels have no sample array and no implicit default.
- `normal_binder_loop_frame_derived = max(raw_loop,0)` is separately labeled. When translation exists, `normal_binder_translation_base_derived` holds the samples at this loop frame and `N-1`, plus their difference. It is conditional on the documented normal binder branch without runtime attribute 8, not a playback decision. `runtime_attributes` is null.

Source coordinates are unchanged XYZ for translation and XYZW for rotation. The evidence links these values to the model layer; **distance units and model-parent/world-space composition remain unknown**. The field name `absolute_translation` inherited from the parser does not establish world coordinates. Source frames and the `N-1` endpoint are corroborated by the binder; conversion to seconds and the game's effective rate remain unknown. Runtime attributes differ from action flags and cannot be inferred from `0x800000` in these action headers.

## Representative export

Seven actions yield **1,847 timeline points and 1,940 channel values**. Selection is technical coverage, without assigning names such as idle, walk or run.

| Entry:slot | Purpose | N / raw loop | Translation codec | Model rotation |
|---|---|---:|---:|---|
| 54913:3 | Short nonzero vector, negative loop metadata | 29 / -1 | 4 | Absent |
| 54913:5 | Positive loop start, zero loop displacement | 275 / 144 | 4 | Absent |
| 92608:18 | 8-bit vector codec | 125 / -1 | 5 | Absent |
| 92608:75 | Both model components, nonzero X curve | 47 / -1 | 4 | Constant codec 2 |
| 92608:82 | Small constant nonzero vector | 51 / 0 | 1 | Absent |
| 92608:101 | Positive loop start, negative Z curve | 201 / 68 | 4 | Absent |
| 92608:1 | Exactly zero control | 199 / 0 | 1 | Absent |

Two distinctions matter for integration. For 54913:5, the final sampled Z is `148.26978422387708`, but `sample(end)-sample(loop144)` is zero and the raw header end is zero. For 92608:101, the derived loop displacement Z is `-1015.0889341176103`, while the raw header is `-1015.1830444335938`. The export retains both; neither is silently substituted for the other. For 54913:3, the corrected final Z is `159.90368079099616`; the earlier historical inventory used a superseded sampler and is preserved as historical evidence.

The package SHA-256 is `4fb9a97a7383044ab00140f22581d4bd894543d332a056d1a72628e7593231ee`. Detailed provenance is `research/private/root-motion-curves-v1/provenance.json`, SHA-256 `9945592fde0a9da0f38173ce9224975d867fc95910d495b20deec9ecaa806619`.

Reproduce into new, separate directories (existing outputs are rejected):

```sh
python3 -B tools/resources/root_motion_curves.py \
  --index research/private/resources.sqlite --entry 54913 --entry 92608 \
  --select 54913:3 --select 54913:5 --select 92608:18 --select 92608:75 \
  --select 92608:82 --select 92608:101 --select 92608:1 \
  --model assets/local/player-probe/body001/scene.gltf \
  --output assets/local/root-motion-curves-recheck \
  --research research/private/root-motion-curves-recheck
```

## Verification and remaining boundary

Ten synthetic boundary tests passed, covering type-based binding independent of bone ID, absent components, nonunit raw references, half-frame interpolation, tail clipping, raw versus derived loop/header values, tiny nonzero constants, duplicates, unsupported samplers, invalid frames, limits and empty slots. The existing 8 vector/timeline and 6 quaternion regressions also passed. A first test exposed error-precedence inconsistency for an unsupported model codec; the adapter now checks the model sampling profile before attempting generic curve preparation.

The real package was regenerated into a separate private directory with byte-identical `curves.json` and inventory. Readback exactly matches the owning point sampler for all 1,940 values; this is a serialization/ownership check, not an independent ARM oracle. Fresh payloads match previous extraction hashes. Existing-output rejection was exercised without replacing bytes. Both frozen 44-action body glTF and binary still match their retained provenance hashes.

For a future float32 bridge, conversion of the 5,913 exported numeric components alone has maximum absolute error `6.054236791896983e-5` and RMS `4.852459154272753e-6`. These are measured serialization-rounding values for this sample, not position/application error bounds or an ARM-runtime comparison. The report, test logs and reproduction are retained in `research/private/root-motion-curves-validation/run-1/validation.json`. Cold performance, CPU/RSS and before/after comparison were not measured.

No original instructions or game runtime ran during this export. The independent isolated ARM application/clock investigation remains described in [root-motion-runtime.md](root-motion-runtime.md); its explicit previous state, attributes, templates, excluded pre-update/events and layer limits are still required. Connecting these curves to that API requires a declared controller policy. Joint channels for the rejected actions, missing-component fallback, collision response, action scheduling, playback speed and the original player controller are not supplied or inferred here.
