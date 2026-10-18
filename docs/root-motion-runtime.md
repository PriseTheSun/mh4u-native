# Root motion: executable one-layer application and clock contracts

The native library implements bounded one-layer application (`0x95cab0`), pre-update (`0x95b790`), prediction and temporal advance (`0x9573b8`/`0x957f80`). `tick_one_layer` composes repeated updates without event sources; `plan_tick_with_events` additionally requests the four tables through their native owner. The original application-only and isolated clock APIs retain their narrower contracts. A separate diagnostic viewer composes seven joint clips and model movement. These APIs do not choose gameplay actions, initialize a hunter, blend multiple layers or dispatch gameplay callbacks.

This is an **isolated execution experiment with synthetic state**. Original ARM instructions run in Unicorn; the sampler is intercepted with declared values. It is not an observation of a running 3DS game, its initialized globals or its hunter controller. The engine library contains independently written C++ equations and synthetic numeric fixtures; it does not depend on Unicorn or game instructions.

## Provenance and execution boundary

- Analysis ELF: `research/private/mh4u-analysis-bss.elf`, SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
- Reused REA bundle: `research/private/rea-root-motion-evidence.json`, SHA-256 `34d03e7fe5b4daafb4e41fe50ea45d227830dbe9c63d416d959fbe98ef34b175`. No new Ghidra session was opened.
- Application assembly Evidence: `ev_489b357b29cb3198fb345b7875af1996eb4494a1b6e95353acaaddd5c420bef1`, retained in `rea-session-1792274318294/020-procedure_assembly.json`.
- Executed application range: `[0x95cab0,0x95de18)`, 4,968 bytes, SHA-256 `31b51f2ed9c7e8268d11b28101bf81b8253ec87bad1d0b609e17abc2088243d1`. Bytes and LLVM disassembly stay under `research/private/root-motion-runtime/`.
- Optional research dependency: [Unicorn 2.1.4](https://github.com/unicorn-engine/unicorn/releases/tag/2.1.4), ARM11MPCore CPU model; the [official API tutorial](https://www.unicorn-engine.org/docs/tutorial.html) describes its mapped-memory and execution hooks. Installed library hashes are retained in `source-manifest.json`.

`tools/resources/root_motion_probe.py` verifies the ELF hash before loading its initialized PT_LOAD bytes. Model/stack memory is poisoned, not implicitly zeroed. Every readable synthetic field/global must be explicitly initialized or previously written by the emulated routine. An undeclared memory read, out-of-scope write, code escape, unexpected sample/frame, instruction limit or timeout fails the case. CPU registers, VFP enablement and FPSCR=0 are declared inputs. Hardware/FPSCR equivalence beyond this profile is unverified.

The sampler at `0xaea0e4` is intercepted twice for the selected frame, returning float32 translation and quaternion. The 52 fixtures poison caller-saved scratch registers after each intercepted call. Four global templates at `0x10e62c0`, `0x10e6370`, `0x10e6560`, `0x10e6570` are explicit synthetic parameters. These fixture values remain synthetic. A later independent experiment (`root-motion-defaults/isolated-run-v2.json`, 21 cases and three negative controls) executed an initializer tail and its helpers: `0x10e62c0` and `0x10e6370` became `[0,0,0]`, `0x10e6560` became `[0,0,0,1]`, and `0x10e6570` became `[0,0,0,0]`. That is neither whole-startup execution nor proof these globals never change. The library still requires explicit reset templates and rejects the zero-quaternion fallback path. Non-default reset-template fixtures distinguish copying a template from assuming zero/identity.

## State layout and supported profile

Let `M = model + 0x240`, the first motion layer. The original application-only API accepts exactly one active layer with `M.u16(+6)=4`, `M.u16(+8)=1`, and a slot other than `0xffff`. These flags avoid the next-sample prediction side effect. They are a deliberate support boundary, not a statement that ordinary playback always uses these values.

| Source field | Meaning established by this application routine |
|---|---|
| model `+0x40`, `+0x50` | position P, quaternion Q (XYZW) |
| model `+0x60`, `+0xbd0` | component scales S, N; N is called `null_scale` by the research API |
| model `+0x234` | layer count, fixed to one |
| M `+0x0c` | runtime attributes; initial mask `0x0004a1a1`, now `0x0004a1b5` after inert clock-bit tests |
| M `+0x48`, `+0xe4`, `+0xf4` | blend, translation weight, rotation weight |
| M `+0x64` | sampler frame supplied to the intercepted sampler |
| M `+0x78`, `+0xa8` | previous quaternion Qt, translation Pt |
| M `+0x98`, `+0xc8` | current sampled quaternion, translation |
| model `+0xbe0`, `+0xbf0` | output delta position, delta quaternion |

Attributes outside that mask and other internal/previous flag values fail explicitly in C++. The mask includes only individually exercised source branches. Its numeric bits are retained; gameplay names, action selection rules and collision meanings are not assigned.

The first contributing layer bypasses weighting. Fixtures with blend 0, 0.25, 2 and -1, and different translation/rotation weights, produce the same application. The `0x40000` mixing branch is also bypassed here. This result must not be generalized to a second layer.

## Derived equations and operation order

Quaternions use XYZW Hamilton multiplication. `R(q)` is the ordinary polynomial quaternion matrix, without implicit input normalization; `inv(q) = conjugate(q)/dot(q,q)`. Let T,C be the current translation/quaternion sample. Reset position/quaternion values are caller-supplied parameters.

1. If `attr & 0x8100`, negate `T.x`, `C.y`, and `C.z`. Save these post-mirror samples, including to the previous-state fields.
2. If `attr & 1`, preserve model P,Q and return the reset delta templates. Sampling and previous-state updates still happen.
3. Otherwise, if `attr & 0x2020`, set A=T and D=C. If not, calculate `A = R(Q * inv(Qt)) * (T - Pt)` and `D = inv(Qt) * C`.
4. For `attr & 0x2020`, assign P'=T, or P'=S*T componentwise when `attr & 0x2000`; delta position is the reset template. N does not participate. Otherwise calculate `deltaP = R(Q) * diag(S*N) * transpose(R(Q)) * A`, then P'=P+deltaP.
5. If `attr & 0x80`, assign Q'=D without normalization and return the reset delta quaternion. Otherwise assign `Q' = normalize(Q * D)` and retain D as the delta quaternion without normalizing D.

The two orientation matrices around anisotropic scaling are essential. Scaling the final vector componentwise changes the result. Transpose is kept as transpose, not replaced with an inverse for nonunit input.

The nonunit fixtures discriminate the normalization branches. With Qt=(0,0,0,0.5), C=(0,0,0,2), Q=identity, ordinary application returns unit Q' but delta quaternion w=4. Attribute `0x80` returns Q'.w=4; attributes `0xa0` return Q'.w=2. No endpoint normalization is added by the library.

The native implementation uses double intermediates and finite float32 outputs. It preserves algebraic order but does not emulate each VFP rounding, FPSCR exception or fused instruction. Quaternion inputs whose float32 squared norm is zero/nonfinite are rejected instead of reproducing the original fallback path; finite reset templates are copied literally. Results exceeding float32 range also fail. These are explicit API boundaries.

## Validation and retained fixtures

52 isolated ARM fixtures passed: 49 identity/anisotropic/negative/zero-scale, noncommuting rotation, attribute, custom-reset and deterministic random cases, plus three nonunit quaternion cases. All returned through the declared sentinel without undeclared reads/calls. The 49-case independent Python algebra comparison had maximum absolute component error `6.570581593479119e-6` against ARM output (threshold `1e-4`); the three nonunit cases matched that oracle exactly.

`src/root_motion_tests.cpp` embeds only synthetic numeric inputs and observed ARM outputs from `apply-fixtures.json` and `apply-nonunit-fixtures.json`. It does not compute expected outputs through the implementation being tested. Direct clang++ execution and a separate UndefinedBehaviorSanitizer build both passed all 52 fixtures and API rejection tests.

| Native vs isolated ARM | Maximum absolute component difference | Acceptance tolerance |
|---|---:|---:|
| Position and delta position | `7.62939453125e-6` | `1e-4` |
| Model and delta quaternion | `1.1920928955078125e-7` | `1e-5` |
| Post-mirror samples and previous values | `0` | exact |

Rejection tests cover every unsupported attribute bit, unsupported internal flags, NaN/Inf, quaternion zero/underflow/overflow norms and output overflow. Scale and layer metadata must remain unchanged. These measured errors cover the retained cases; they are not a global error bound for every finite input.

The frozen application snapshot and file hashes are in `research/private/root-motion-runtime/application-v1/manifest.json`. It retains source, fixtures, complete instruction/access traces, test logs and this document before any advance-time expansion. The fixture generator reproduces the reviewed C++ test file byte for byte.

To repeat original-instruction execution using the retained local ELF and optional research venv, choose a **new** output path:

```sh
research/private/root-motion-runtime/venv/bin/python -B tools/resources/root_motion_probe.py \
  --elf research/private/mh4u-analysis-bss.elf \
  --cases research/private/root-motion-runtime/application-v1/apply-cases-all52.json \
  --output research/private/root-motion-runtime/application-recheck.json
```

This command reruns the guarded ARM execution. The committed C++ tests can run without the private ELF, Unicorn, ROM or extraction outputs.

## Isolated clock and loop evidence

After freezing the application snapshot, the harness was extended to execute `0x9573b8` and its tail body `0x957f80`. This is a separate profile: one present model/layer, zero original joints and no event pointers, explicitly intercepted `0x95b790` pre-update, and no blend transition. The intercepted call is a declared no-op, with caller-saved registers poisoned. Its real state changes are not claimed equivalent to a no-op. The original entry/body still execute; undeclared helper calls fail.

The retained REA advance assembly is Evidence `ev_e2259ed88779fc4762b5b6172fb6e28dc12984979d10585b7ab717c839131cc8`; pseudocode is `ev_4c3dbbd0c7ef787f9b9a28cb4e41842c1e5c74561232066caf921b1cfdcb5dca`. Raw ranges and hashes are in `source-manifest.json`. No additional Ghidra session was opened.

| Source field | Clock experiment meaning |
|---|---|
| model `+0x14` | delta multiplier; explicitly 1 in these fixtures |
| M `+0x5c` | speed; step = float32(model multiplier × speed) |
| M `+0x4c`, `+0x50` | frame accumulator F, previous accumulator |
| M `+0x54`, `+0x58` | end E, loop-start L; interval = float32(E − L) |
| M `+0x14`, `+0x18` | progress value, previous step |
| M `+0x88`, `+0xb8` | base quaternion Bq and base translation Bt |
| M `+0x108`, `+0x118` | four current and previous event-mask words |

The chosen multiplier value does not establish seconds, FPS or a gameplay update rate. E and L are explicit synthetic inputs here; this test does not establish how each action chooses them.

Before advancing, the routine merges the lower 24 bits of layer attributes and model `+0x23c`, retaining the high byte of model `+0x238`. It then invokes the excluded pre-update. The following clock results are conditional on the synthetic state remaining unchanged across that call.

The executed body computes progress from prior flags: with bit4 set and prior-flags bit1 clear, `progress = previousFrame + previousStep + step − E`; with bit1 set, it first tries `F + step − E` and, if negative, uses `F + previousStep − E`; otherwise progress is zero. Nonpositive progress is clamped to zero. These operations use float32 rounding after each arithmetic instruction.

It saves previousFrame=F and, unless `attr & 0x10`, adds step to F. It stores old internal flags into previous-flags and initially retains only the high byte of the internal flags. The tested high-byte value `0x200` survives; transition bit `0x100` and entry update bit `0x2` are excluded.

The source uses epsilon bits **`0x3a03126f`**, float32 `0.0005000000237487257`, and signed truncation toward zero:

- For step ≥ 0, calculate rawIndex=trunc(F), index=trunc(float32(F+epsilon)), and nextIndex=trunc(float32(F+float32(step+epsilon))). When rawIndex differs from index, replace F with float32(index). Bit1 predicts reaching E using nextIndex. With `attr & 4`, index ≥ trunc(E) clamps F=E and leaves bit4; otherwise it wraps when index ≥ trunc(E) and trunc(E)>0.
- Each forward wrap subtracts float32(E−L) from F, subtracts Bt componentwise from previous translation and assigns `previousQ = inverse(Bq) * previousQ`. It repeats while trunc(float32(F+epsilon)) ≥ trunc(E). Nonpositive forward intervals clamp at E without applying base adjustments.
- For step < 0, compare trunc(F) with trunc(float32(F−epsilon)); when they differ, replace F with float32(trunc(F)). Bit1 predicts `float32(F+step) < 0`. With `attr & 4`, a negative F clamps to zero and sets bit4. Otherwise E>0 and F<0 trigger repeated additions of float32(E−L), additions of Bt and `previousQ = Bq * previousQ` until F≥0. **The reverse boundary is zero, not L.** Bit4 marks this crossing. The quaternion products are not normalized.
- E with float32 bits `0x3f800000` additionally forces bit1. At the end, previousStep=step. With zero event pointers, unfrozen event masks are copied to previous masks and cleared. `attr & 0x10` skips that mask processing.

Consequently `attr & 0x10` suppresses the increment, but does not universally freeze the clock: an already out-of-range F is still processed by the boundary/loop logic. A zero step at E can likewise wrap. A forward case starting F=9 with step=17, E=10, L=2 wraps **three** times and finishes at F=2. Noncommuting quaternion cases corroborate multiplication order in both directions, and nonunit Bq=(0,0,0,2) distinguishes inverse from conjugate and confirms no normalization.

`advance-report-v1.json` records **55 passing original-instruction executions**, compared with independent equations in `advance_verify.py`. Clock, progress, flags, translations, attributes and empty-event-mask outputs match exactly. The maximum quaternion component difference is `4.9753042796751856e-8`, below tolerance `1e-5`. These cases include 20 deterministically generated cases, boundary epsilon values, positive/negative/zero step, end prediction, repeated wraps, clamp, freeze, progress branches, degenerate forward intervals and nonunit/noncommuting quaternions. `advance-fixtures-v1.json` retains compact input/observed-output pairs consumed by the native clock tests.

Five negative controls passed in `guard-report-v1.json`: missing explicit global initialization, wrong sampler frame, missing pre-update declaration, an excluded layer-update call, and a reverse zero interval all stop at their declared boundary. The reverse zero interval exhausted the bounded instruction budget: it must be rejected by any future native API instead of reproducing a nonterminating loop. No original error-handling policy is inferred from this diagnostic. All 52 application cases were rerun after the harness expansion with identical outputs and intercepted calls.

The original clock investigation is frozen separately under `research/private/root-motion-runtime/advance-v1/manifest.json`. That snapshot remains unchanged. Native implementation and validation are retained separately under `native-advance-v1/manifest.json`; no action has been enabled.

## Native clock API and validation

`advance_one_layer(state, parameters, scope, limits)` returns a new state, progress value and loop iteration count. It never mutates its inputs. `AdvanceState` owns the frame accumulator, previous frame/step, previous translation/quaternion, internal/previous flags, event masks and model attribute word. `AdvanceParameters` supplies the delta multiplier, speed, end/loop frames, base transform, layer attributes and model attribute override.

`AdvanceScope` and `AdvanceLimits` are required arguments without defaults. The caller must explicitly select `PreUpdate::declared_unchanged`, zero original joints and four absent event sources. This acknowledges the experimental boundary; it does not establish that the game's external pre-update is a no-op. The original joint-processing path and event pointers are not followed. A caller supplies the loop budget; zero permits a step that needs no wraps, and exceeding the budget throws without returning partial state. The budget is a native resource policy, not a recovered original limit.

```cpp
const mh4u::root_motion::AdvanceScope scope{
    mh4u::root_motion::PreUpdate::declared_unchanged,
    0, {false, false, false, false}
};
const mh4u::root_motion::AdvanceLimits limits{256}; // Explicit caller budget.
auto result = mh4u::root_motion::advance_one_layer(state, parameters, scope, limits);
```

The accepted attribute mask is `0x14`; internal and previous flags accept mask `0x205`. These retain the tested clock branches and the preserved high-byte bit `0x200`, excluding layer update bit `0x2`, blend transition bit `0x100` and unknown bits. A nonzero original joint count, any event source, unverified pre-update policy or unsupported flag fails explicitly.

End and loop-start are restricted to nonnegative integer-valued float32 inputs within signed-int32 range. Every performed float-to-int32 conversion is bounds checked. All scalar/vector inputs must be finite; input quaternion squared norms must be positive and finite in float32. The original zero-quaternion fallback remains excluded. Nonpositive reverse-loop intervals are rejected; the observed forward clamp for a nonpositive interval is preserved. A wrap that cannot change the float32 accumulator is also rejected. Float overflow and the caller's exhausted loop budget have distinct tagged errors.

Clock additions/subtractions/multiplication round separately to float32, retaining epsilon bits `0x3a03126f`. This implementation checks IEEE binary32 and round-to-nearest mode; other host rounding modes fail explicitly. Quaternion products use double intermediates and round to float32 after each loop, without normalization. It is not a complete VFP/FPSCR emulator and has no general bit-equivalence claim.

The fixture generator now embeds **55 original-instruction clock outputs plus the 52 application fixtures** in `src/root_motion_tests.cpp`. Expected clock outputs come directly from the preserved ARM report. Expected wrap counts are cross-checked against the original instruction trace's reads of the base-translation field, not just the algebra oracle's counter. Fixture selection uses names for the discriminating cases. Both generators reproduce the test file byte for byte; `apply_one_layer` itself remains unchanged from the application snapshot.

Direct clang++ normal (`-O2 -Wall -Wextra -Werror -pedantic`) and UndefinedBehaviorSanitizer (`-O1 -fsanitize=undefined -fno-sanitize-recover=all`) builds both passed:

| Native clock vs isolated ARM, 55 cases | Maximum difference | Tolerance |
|---|---:|---:|
| Frame, previous frame/step, progress, translation | `0` | exact |
| Flags, attributes, current/previous empty-event masks, loop count | `0` | exact |
| Previous quaternion | `5.960464477539063e-8` | `1e-5` |

The 52 application regressions retain their earlier error maxima. Negative tests exercise every unsupported attribute/flag bit, unverified pre-update, each event source and original joint count, NaN/Inf, fractional/negative/out-of-range clock parameters, conversion overflow after addition, multiplication overflow, invalid quaternion norms, reverse zero/negative intervals, nonprogressing float32 wraps, insufficient/exact/zero loop budgets, quaternion overflow during repeated wraps and an excluded host rounding mode. The nonunit forward/backward cases explicitly retain quaternion magnitudes 0.5 and 2.

These validations establish the isolated contracts and measured fixture coverage. They do not establish the game's timing units, full callback/event behavior, action scheduling or multi-layer policy. The repeated-tick extension below consumes actual returned flags without forcing the old application-only profile.

## Executed pre-update, first-layer profile

The original `0x95b790` entry is now executed before application and clock advancement. Retained REA pseudocode Evidence is `ev_0153463fb072a3bc5a873dd88cd891c8ac31fddcfc573bb30493e4cd8362d150`; the pinned ELF bytes at `[0x95b790,0x95b940)` have SHA256 `e2e685a1ed968ef621fc4582f735092464098238aeb5ca9355eed0b032e58c8d`. The cached LLVM words were compared with the ELF. No new REA session was opened for this investigation.

With `model+0x238 & 0x20000000` clear and transition bit `0x100` clear, this entry returns unchanged. With the gate set, one active layer and zero original joints, it sets `M+0x64 = clamp(F,0,E)` and `M+0x68 = clamp(float32(F + float32(speed*deltaMultiplier)),0,E)`, then calls the original application routine. A discriminating ARM case establishes separate rounding of the VMLA multiply/add. Attribute `0x800`, blend transitions and nonzero joint counts leave this demonstrated path and are rejected.

`pre_update_one_layer` and `pre_update_and_advance_one_layer` preserve these distinctions. Their sample is an explicitly supplied optional timed value: gate clear requires absence; gate active requires the exact prepared current frame. No missing channels are filled. Composition merges attributes, executes pre-update and advances the returned state. It does not declare the real pre-update a no-op. This first API retains flags `4/1` for the active gate, so it deliberately rejects a subsequent tick that requires prediction. The gate-clear subset can repeat. Application attributes `4` and `0x10` were verified inert in the isolated application body, expanding that accepted mask to `0x4a1b5`.

The frozen `pre-update-v1` contains 49 passing original executions, four excluded-branch negative controls, source and independent algebra, plus native normal/UBSan logs. Native maximum vector component error was `2.288818359375e-5` (tolerance `1e-4`); quaternion error was `1.1920928955078125e-7` (tolerance `1e-5`); prepared frames, clocks, flags and event masks matched exactly. All 52 application and 55 clock regressions passed. These remain synthetic states and intercepted sampler values, not execution of a gameplay action.

The same harness also retained three 12-tick original ARM sequences with no flag rewriting: forward loop, reverse loop and forward stop. Synthetic translation was `(t,2t,3t)` and quaternion identity, with explicit loop base `(8,16,24)`. Model X outputs were respectively `0..11`, `0..-11`, and `0,1,2,2,...`. Only sampler calls were intercepted. These sequences exercise prediction branches not accepted by the first native pre-update API and motivate a separate extension.

## Prediction and repeated native updates

The application routine tests internal flags at `0x95d3dc`, after computing its first-layer contribution and before storing the sampled previous state at `0x95db08`. Its prediction writes quaternion `M+0x20` and translation `M+0x30`. These are relative sampled transforms; no velocity unit or gameplay meaning is established. The source does not read the previous prediction in this one-layer application path. An explicit prior prediction is nevertheless carried through ticks because some branches preserve it.

Let C=`clamp(F,0,E)`, N=`clamp(float32(F+float32(speed*deltaMultiplier)),0,E)`, and P the raw previous-frame field `M+0x50`. Each sample is translation T and quaternion Q. The original request order and branch priority are:

| Condition after the active gate | Original sample frames, before duplicate removal | Prediction pair A→B | Source branch |
|---|---|---|---|
| internal bit1 set, including flags5 | C, E | C→E | `0x95d3e8..0x95d61c` |
| bit1 clear, bit4 set, previous bit1 clear | C, E, P | P→E | `0x95d634..0x95d8d4` |
| bit1 clear, bit4 set, previous bit1 set | C | preserve prior prediction | `0x95d628..0x95d630` |
| bits1/4 both clear | C, N | C→N | `0x95d8d8..0x95db04` |
| model gate clear | none | preserve prior prediction | pre-update early return |

For the three writing branches, `predictionT = R(inv(Qa)) * (Tb−Ta)` and `predictionQ = inv(Qa) * Qb`. Neither result is normalized. Nonunit samples discriminate inverse from conjugate, polynomial matrix from normalized rotation, and unnormalized quaternion products. The end and previous frames remain the requested source values; the previous-frame request is not clamped. Mirror logic is present in these source branches but excluded from the new profile, because its previous-frame behavior needs a separate contract. No instruction address was patched; only the declared sampler calls were intercepted.

`prediction-v1/report.json` retains 50 new original-instruction cases, covering all branches and priority, noncommuting/unit/nonunit quaternions, model scales, clock attributes `0`, `4`, `0x10`, `0x14`, and preserved high-byte flag `0x200`. Independent Python algebra has maximum prediction-translation error `3.830396721582474e-6` and prediction-quaternion error `1.396995777946941e-7`. Five negative harness controls reject missing next/end/previous samples, duplicate frame inputs, and an undeclared previous-frame field. These are execution-boundary checks, not recovered game error handling.

The C++ API adds:

- `TickState { PreUpdateState pre; Sample prediction_delta; }`, including the prior prediction as an explicit finite value. No identity is supplied for it. A preserved prediction is treated as stored data and is not silently normalized.
- `sample_requests_one_layer(pre, parameters, scope)`, which returns the selected branch and up to three distinct numeric frames, in original call order. Equal frame requests collapse so a deterministic sampler is queried once per frame. The returned count is authoritative; unused array slots carry no meaning.
- `pre_update_with_prediction_one_layer(...)`, which applies the standalone pre-update and prediction to the supplied model attribute word.
- `tick_one_layer(...)`, which first merges the attributes, executes the native pre-update/application/prediction equations, and advances that returned state. All input objects remain unchanged. Errors return no partial state.

The caller supplies exactly the requested frame/value pairs. Missing, unused, wrong-frame or duplicate samples fail explicitly, as do unsupported attributes, flags, original joints or event sources. Accepted layer attributes are restricted to mask **`0x14`**, and internal/previous flags to **`0x205`**. Quaternion input norms, finite arithmetic, clock domain, rounding mode and loop budget retain the earlier checked boundaries. The source's application arithmetic is owned by one internal function; old and new APIs validate their respective scopes without rewriting flags to fake the old `4/1` profile.

For integration, call the planner, obtain explicit samples, call `tick_one_layer`, then carry its entire `state` into the next tick. Initial flags, model gate, initial previous sample/frame, scales, reset values, end/loop/base transform, action selection and timing remain **caller-owned policy**. The experiment neither recovers nor authorizes a default walk/run initialization. It also does not derive the action's base transform: the clock receives that value explicitly. Missing-channel handling belongs to the sampler/binding layer, not this library.

Seven chains of 12 original ARM ticks preserve all returned state and flags without rewriting: three identity curves (forward loop, reverse loop, stop), plus noncommuting authored quaternion curves for forward loop, reverse loop, stop and frozen clock. The sampler inputs remain synthetic. The native sequence tests feed their own previous native result into the next tick and compare every step to the independently retained ARM result, including prediction, original sample request order, masks, frames and flags. They do not reset from the ARM expected state between ticks. This demonstrates a repeated subset across those crossings, subject to the explicit finite/domain/budget limits; it is not a proof of every future sequence or real hunter behavior.

The native suite now has **134 prediction comparisons** (50 isolated cases plus 84 sequential ticks), alongside all **52 application, 55 isolated-clock and 49 initial pre-update regressions**. Normal and UBSan builds passed. Across the 134 prediction comparisons the maximum absolute vector component difference was `3.814697265625e-6` (tolerance `1e-4`), and quaternion difference `1.1920928955078125e-7` (tolerance `1e-5`). Frames, request order, flags, masks, sampled values, progress and loop counts matched exactly. These are measured fixture maxima, not general error bounds or bit-equivalence claims. Prediction and application use double intermediates with finite float32 outputs; clock operations retain their separate float32 rounding.

`prediction-v1/manifest.json` freezes source, generators, raw branch bytes, traces, independent equations and validation logs. Earlier snapshots `application-v1`, `advance-v1`, `native-advance-v1` and `pre-update-v1` remain unchanged. Numeric fixtures are compiled into the native test; game bytes, Unicorn and private reports are not linked into the engine.

## Real model curves through the public diagnostic process

A separate oracle now combines the retained original ARM routines with the seven real model curves identified in `assets/local/paired-player-actions-v1/model-curves.json` (SHA256 `4fb9a97a7383044ab00140f22581d4bd894543d332a056d1a72628e7593231ee`). Entry/slot identities are `54913:3`, `54913:5`, `92608:18`, `92608:75`, `92608:82`, `92608:101`, `92608:1`. These identifiers have no inferred gameplay names.

`research/private/root-motion-runtime/real_curves_sequences.py` verifies both source LMT hashes and reproduces each packaged curve through `paired_actions.verify_curve_action`. It calls the owning `animation_probe.sample_model_channel`; it does not add another curve decoder. The current owner reproduces the packaged values despite its source file hash changing since the original package. Each sampled binary64 component is narrowed once to binary32 before the intercepted sampler returns it. The maximum observed narrowing error is `6.054236791896983e-5`; 861 integer-frame component queries lie between authored keyframes. The original ARM sampler/codec body is still intercepted in this combined experiment, so this is not new execution proof for the curve decoder.

The diagnostic initialization is explicit: F=previousFrame=previousStep=0; P=0, Q=identity, scale=nullScale=1; previous and sampled transform=sample(0); internal/previous flags=4/1; model gate=`0x20000000`; delta position=0 and delta quaternion=identity; initial prediction translation=0 and quaternion=identity; speed=deltaMultiplier=1; E=N−1, L=max(rawLoop,0); zero original joints and no event pointers or masks. Base translation is narrowed endSample−loopSample; base quaternion is narrowed `inv(loopQ)*endQ`, calculated from the already narrowed endpoint samples. Initial state and the binder choice are authored diagnostic policy, not a recovered hunter controller.

The template mapping is explicit: **`ResetTemplates.position` maps to `0x10e6370=[0,0,0]`; `ResetTemplates.rotation` maps to `0x10e6560=[0,0,0,1]`.** The separate `0x10e6570=[0,0,0,0]` is the inverse-quaternion fallback and is outside the nonzero-norm library profile. It must not be substituted for the reset quaternion. Aggregate translation template `0x10e62c0` is also zero. Missing rotation is bound to identity only at the diagnostic sampling boundary; the Package retains absence. Six of these seven actions lack rotation; all seven have translation.

Each action runs both a loop sequence of `E+(E−L)+3` ticks (two crossings) and a clamp sequence of `E+3` ticks. All **2,590 original ARM ticks** passed the existing memory/code guards, with only sampling intercepted and no flag rewriting between updates. Fourteen additional row-zero states are declared initial inputs rather than executed outputs. Compact inputs, sample caches, complete output states, expected CLI TSV and compressed instruction/read traces are frozen in `real-curves-sequences-v1/`; its manifest SHA256 is `f61be3310aa1f4ffc616fb517829972b7353a11eaf495bbe96577f65a402356f`.

`tools/action_motion_smoke.py` compares the public `mh4u-action-motion` process to that frozen oracle without loading Unicorn or executing ARM. It verifies the oracle files, paired curve identity, ACTION identity, row count and every printed field. It restores the CLI's nine-digit decimal values to float32 before comparing. The 2,604 rows cover sample/clock frame, internal/previous flags, loop count, position/quaternion, previous translation/quaternion and prediction translation/quaternion. Unprinted internal fields remain available in the ARM trace but are not claimed newly compared by this CLI test.

The native v2 container has stronger external-file bindings; its underlying curves and action identities remain the same as the v1 source of the oracle. On the local macOS build, all 14 process sequences passed with maximum vector error `1.1920928955078125e-7` (absolute tolerance `1e-4`), quaternion error `0` (tolerance `1e-5`), and exact numeric clock/flag/loop fields. Fourteen process negative controls also passed: invalid action/count/numbers, unsupported step/start bounds, missing/unknown options, an altered package, and an altered external BIN. Mutations occur only in temporary copies. No original game resource is changed. The current comparison is retained separately from the frozen ARM oracle under `real-curves-native-comparison-v2.json`.

Example reproduction from the project directory, using the locally retained oracle:

```sh
python3 tools/action_motion_smoke.py \
  --binary build/mh4u-action-motion \
  --package assets/local/model-motion-native-v2/model-motion.bin \
  --paired-root assets/local/paired-player-actions-v1 \
  --oracle research/private/root-motion-runtime/real-curves-sequences-v1
```

This makes the numerical composed path reviewable without putting an ARM runtime into the executable or executing it during CTest. It does not establish original event handling, timing, action scheduling, collision response or a complete native game.

## Remaining boundary

Multi-layer blending, mirror/absolute prediction, blend transitions, event callback dispatch, inactive layer behavior and original gameplay scheduling remain outside the tick contract. The original zero-quaternion fallback remains outside the native accepted domain despite the separately observed zero template. Whole-startup initialization, later global mutation, parent transforms and hunter movement/collision policy are not established here. Unsupported finite-domain conversions and nonprogressing or excessive loops fail instead of acquiring invented native behavior. The delivered viewer supplies an explicit diagnostic binding for seven actions; original controller behavior remains pending.

## Four event tables and transactional ticks

The clock owns its event interval calculation. `AdvanceOutput.event_plan` exposes at most two raw signed32 requests, derived from original code at `0x9584d0–0x9585cc`. They are kernel arguments, not normalized geometric intervals. Reverse wrapping can produce `begin > end`; an end frame of zero produces `-1`. Each endpoint at or beyond E is replaced by E−1. There is no lower clamp. Forward wrapping requests the tail and loop prefix at most once, even after multiple clock wraps; it does not invent queries for every crossed cycle. Flag1 can request a second interval before wrapping. Attribute `0x200000`, which changes that branch in the source, is outside the accepted mask `0x14`.

The native kernel preserves those signed requests; see [native-motion-events.md](native-motion-events.md). With attribute `0x10`, both current and previous masks remain unchanged and no table query is made. Otherwise previous masks copy the old current masks, current masks start at zero, and results for each present group are ORed. Declared absent pointers stay zero; unavailable/rejected package data is not treated as absence.

`plan_tick_with_events` runs the same owned arithmetic as the legacy tick, allowing explicit table presence but still requiring zero original joints. It returns a `PendingTick` with at most eight requests. Its intermediate output is private. `finish()` requires one answer per exact group/ordinal/range, rejects missing, extra, mismatched and duplicate answers, and returns a complete state. Answer order is irrelevant. The input state and pending result remain unchanged on error. This separates clock ownership from table decoding without interpreting raw LMT a second time.

The observation-only harness addition records six initialized stack locals at `0x9585cc`; it changes no instruction, register or target memory. `research/private/root-motion-runtime/events-v1/clock-event-fixtures.json` retains **465 original ARM cases**, combining 155 clock scenarios with all-present, mixed and all-absent tables. The real pre-update gate-clear branch executes. They cover zero/one end frames, epsilon boundaries, signed/reversed requests, multiple wraps, stop/freeze, source absence and a present empty table. Native tests compare the actual plans and four current/previous masks; **10,743 assertions** passed normally and under ASan/UBSan. Fixture SHA256: `61f4a93147319ba547ae95190f3146e62dd4aed4c26788957e4c75fefad659e4`. Test answers use the independently ARM-checked table owner; the expected final masks come directly from ARM output.

A separate retained oracle, `real-events-sequences-v1`, executes the active pre-update/application/prediction/clock with real tables for all seven paired actions: **2,590 ticks, 14 declared initial states, 19 ticks with nonzero masks**. All non-event outputs are identical to the earlier no-table oracle. Only model-curve sampling is intercepted with pinned values; the original joint-processing path is explicitly excluded; gameplay event consumers are not invoked. Its manifest SHA256 is `e446cdde7b2194f4225a6cce77d582586b1ac882bca7efebf1006b35d15890c7`. This is an isolated instruction experiment, not a running gameplay session or proof of event meanings.

## Correction: model+0x220 is the joint count

The earlier name `event_binding_count` was incorrect. The verified chain is
MOD header `u16+6` → rModel `+0x68` → model `+0x220` (the producer stores the
low eight bits). The native scope now names it `original_joint_count`. The
zero-count restriction and all numeric calculations remain unchanged. See
[motion-event-dispatch.md](motion-event-dispatch.md) for the loader, allocation
and consumer evidence. This field does not establish whether callbacks exist.

Frozen research artifacts retain their original bytes, hashes and historical
labels. Interpret their zero `model+0x220` initializations as excluding the
original joint-processing path, not event bindings. In particular, a statement
that callbacks were absent solely because this field was zero is withdrawn.
The isolated harness explicitly restricts the executed routines and never
claimed a complete gameplay update. The viewer composes independently decoded
glTF joint clips with this numeric model-motion profile; it does not reproduce
the original joint update through `0x95df38`/`0x95e6ec`.
