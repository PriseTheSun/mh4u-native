# Paired action inspection in the native viewer

`--motion-package`, `--paired-root` and `--motion-action` select an explicit diagnostic mode that renders the paired joint clip and the model state produced by `action_motion::Player`. This is a consumer of the validated numeric application, not an action controller or a playable combat implementation.

```sh
build/mh4u-resource-lab \
  --motion-package assets/local/model-motion-native-v2/model-motion.bin \
  --paired-root assets/local/paired-player-actions-v1 \
  --motion-action 0 --motion-repeat
```

Space pauses/resumes; period advances one tick and pauses; Home constructs a fresh Player from the explicit initial policy and pauses. Reset does not seek from the current pose or infer continuity between actions. The selected action is fixed until a new invocation. The existing camera, texture selection, primitive visibility and controller-to-camera settings remain available. No primitive is automatically hidden.

For a deterministic inspection or capture:

```sh
build/mh4u-resource-lab \
  --motion-package assets/local/model-motion-native-v2/model-motion.bin \
  --paired-root assets/local/paired-player-actions-v1 \
  --motion-action 0 --motion-ticks 16 --inspect-only

build/mh4u-resource-lab \
  --motion-package assets/local/model-motion-native-v2/model-motion.bin \
  --paired-root assets/local/paired-player-actions-v1 \
  --motion-action 0 --motion-ticks 16 \
  --hide-primitive 3 --frames 2 --screenshot action.ppm
```

`--motion-ticks N` advances exactly N ticks from the initial state before presenting the first image, then pauses. It accepts 0–100,000 ticks. `--frames` bounds rendered frames, independently of action ticks. A bounded motion render requires `--motion-ticks` or `--paused`; elapsed rendering time does not silently advance that capture. The explicit `--hide-primitive 3` in the example removes the already documented torso inspection silhouette; this is not an inferred original visibility rule.

## Ownership, identity and time

`ActionScene` owns a nonmovable `Package` and its `Player`; the viewer's `LoadedResource` remains the sole owner of the parsed Scene and GL resources. The package loader verifies the compiled expected identity and all 13 content bindings, including joint geometry, animation binary and images. It repeats that owning verification after the scene loader has read them, then checks clip count and source-frame endpoint durations. No alternate model, similarly named clip or caller-supplied digest substitutes for the bound scene.

Each composed pose uses `Player.current().joint_source_frame` for `evaluate_scene(..., loop=false)`. The Player's already advanced `clock.frame` is logged separately and is never used to select that pose. The paired export declares one source frame per glTF second. The viewer therefore passes the source-frame number directly as glTF time; this is a coordinate mapping, not a one-second game frame. No second glTF loop, clock, missing-component fallback or root-motion application is added downstream.

The authored preview policy is initial frame 0, one source frame per tick, model position zero, rotation identity, scale and null-scale one. `--motion-repeat` explicitly chooses repeat; otherwise the validated stop behavior applies. Interactive pacing assumes 60 ticks/second, caps elapsed frame time at 50 ms and does not perform unbounded catch-up after suspension. Original event timing, action transitions, controller rates and combat remain unknown.

`MOTION_STATE` has the same numeric column order as the `TICK` record from `mh4u-action-motion`: tick, joint frame, advanced clock frame, internal/previous flags, loop count, model position/rotation, previous translation/rotation and prediction translation/rotation. `MOTION_MODEL_MATRIX` records the presentation matrix; `MOTION_RENDER_BOUNDS` measures the actual transformed vertices. These diagnostic records allow a capture to be tied to its consumed state.

`--events-package assets/local/motion-events-native-v1/motion-events.bin` additionally binds the externally pinned event container. `ActionScene` owns it for the Player's lifetime. Entry, slot, source LMT hash and frame count must match the selected model motion. Queries use the source clock's signed intervals; masks do not trigger gameplay callbacks. `MOTION_EVENTS` records tick, four current masks and four previous masks. The panel shows the current values in hexadecimal. The seven packaged action shortcuts and `tools/launch_lab.py --action` enable this option; the direct executable can still run the original no-table profile by omitting it. Events require the standalone motion mode and are not network-synchronized.

## Coordinate scope

The seven-piece candidate and paired producer preserve decoded source-model XYZ without axis swaps or fitted placement transforms. The model curves also preserve source XYZ/XYZW. This mode begins at an identity inspection placement and applies the returned model position, raw quaternion and scale to the already skinned joint geometry.

The presentation matrix obtains its raw-quaternion basis from `root_motion::rotation_basis_raw`, the same owning polynomial used by motion application, then composes scale and column-major layout. It does not call the glTF TRS helper, which normalizes rotations. It does not modify or feed the presentation matrix back into Player. `null_scale` is already consumed by the motion application; it is not reapplied to rendered vertices. The current explicit policy sets both scales to one.

This establishes a documented diagnostic composition in the preserved model coordinate system. It does **not** establish original world placement, collision response to the action, the game's final render-matrix update, or a verified stage transform. The camera follows the transformed model for inspection; the numeric position/matrix records retain the displacement. `--world` and `--connect-port` are rejected in this mode. Network movement and the existing local world remain separate, unchanged modes.

## Validation

`tools/action_viewer_smoke.py` exercises public native processes. It compares seven actions at initial, intermediate, stopped and repeated states against the validated numeric action CLI, checks the raw-Q presentation matrix against the returned state, and compares the joint mesh bounds with the standalone glTF viewer evaluated at the returned joint frame. These checks target composition and frame alignment; the independent retained ARM oracle for the underlying application is documented in [action-motion-binding.md](action-motion-binding.md).

The script also rejects missing/mixed options, invalid action/tick values, duplicate arguments, changed package bytes, changed glTF, changed geometry buffer and changed texture bytes before opening a window. Optional `--render` creates bounded native windows, captures four fixed states and checks that the initial/final action records remain identical. It does not control or close unrelated persistent windows.

Executed on macOS: **28 state/geometry comparisons, 23 rejected inputs and 4 native render captures passed**, using 111 bounded processes. All complete state records matched the numeric CLI, including source frames, advanced clocks, flags and loops. Evidence: `research/private/action-viewer-validation-v1/validation.json` and its per-process logs/captures. The loader/math build completed without warnings in `research/private/action-viewer-build.log`.

An isolated macOS app bundle separately verified keyboard interaction: period produced tick1/frame0, a second period tick2/frame1; Space played, a second Space paused at tick310/frame179 and preserved it over another observation; Home returned to tick0/frame0 with the initial pose; Escape ended only the owned process with exit code 0. The post-close accessibility query timed out because the window had closed; process supervision independently confirmed successful cleanup. Evidence: `research/private/action-viewer-ui/validation.json`, captures and `viewer.log`. The pre-existing launcher and server remained alive.

Windows compilation is handled by the root integration workflow; Windows runtime rendering and keyboard interaction were not executed here. These results verify the paired inspection consumer, not original gameplay timing, appearance selection or world placement.

## Event integration validation (milestone 0.10)

The native process comparison checks 2,604 motion rows and 2,604 event rows
against the retained original-instruction oracle, including 20,832 exact mask
components. It also checks 68 viewer inspection states and rejects changed or
missing event packages before state publication. Vector maximum difference is
1.1920928955078125e-7; quaternion difference is zero for these fixtures.
Report: `research/private/motion-events-native-v1/action-process-validation.json`.

The final macOS package `build/packages/20261017-212913` was checked separately:
234 file hashes, identical Mac/Windows assets, nine resources and all seven
paired actions with event tables loaded. The panel was visually checked, its
four groups are fully visible, Home produced tick0/all-zero masks, and six
period key presses returned tick6/sourceframe5 with current G2=1. That agrees
with the pinned action54913:3 oracle. The final window remains open and paused.
Report/captures: `research/private/scene-validation/milestone010-event-ui.json`.

All 24 CTest suites passed before the final display-only line wrapping and
footer wording adjustment. Both platforms were rebuilt after that adjustment;
the final package loading and macOS UI checks above cover the delivered build.
Windows was cross-compiled only, with no Windows runtime or GUI validation.
