"""Explicit model/joint split at the production exporter and sidecar boundary."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import animation_probe as motion
import paired_actions as paired
import root_motion_curves as curves
from test_root_motion_curves import track, parsed


def fixture():
    model = track(4, 1, 7, [0, 0, 9, 1])
    joint = track(1, 1, 0, [1, 2, 3, 1]);joint['ordinal'] = 1
    return parsed([model, joint])


def binding(source, **changes):
    values = dict(source_lmt_sha256=source['sha256'], source_slot=0, entry_id=1,
                  curve_index=0, uri='../model-curves.json', sha256='c'*64, track_ordinals=(0,))
    values.update(changes)
    return motion.ModelCurveBinding(**values)


class PairedActionTests(unittest.TestCase):
    def fail(self, code, callback):
        with self.assertRaises(motion.ProbeError) as caught:
            callback()
        self.assertEqual(caught.exception.code, code)

    def export(self, root, source, bindings=None):
        model=root/'model.gltf'
        model.write_text(json.dumps({'asset':{'version':'2.0'},'scene':0,'scenes':[{'nodes':[0]}],
            'nodes':[{'extras':{'sourceFunctionIdCandidate':0}}]}))
        return motion.export_gltf(root/'source.lmt',source,0,model,1,root/'out',
                                 model_curve_bindings=bindings)

    def test_public_export_separates_by_type_and_keeps_joint_component(self):
        source=fixture()
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);report=self.export(root,source,[binding(source)])
            doc=json.loads((root/'out/animated.gltf').read_text())
            animation=doc['animations'][0]
            self.assertEqual(len(animation['channels']),1)
            self.assertEqual(animation['channels'][0]['target'],{'node':0,'path':'translation'})
            self.assertEqual(animation['extras']['omitted_identity_tracks'],[])
            external=animation['extras']['model_channels_external']
            self.assertEqual(external['tracks'][0]['bone_id_raw'],7)
            self.assertEqual(external['tracks'][0]['track'],0)
            self.assertFalse(external['applied_to_gltf'])
            self.assertEqual(report['channels'][0]['track'],1)
            self.assertEqual(report['export_profile'],'paired-joints-with-external-model-curves-v1')

    def test_legacy_rejection_remains_unchanged(self):
        source=fixture();source['actions'][0]['tracks'][0]['bone_id']=255
        self.fail('unsupported_scene_motion',lambda:motion.prepare_action(source,0,{0:0},1))
        source['actions'][0]['tracks'][0]['reference_xyzw_or_xyz']=[0,0,0,1]
        action,prepared,omitted=motion.prepare_action(source,0,{0:0},1)
        self.assertEqual([p[0]['ordinal'] for p in prepared],[1])
        self.assertEqual(omitted,[{'track':0,'reason':'scene-root translation is identically zero'}])

    def test_joint_type_with_bone255_is_not_classified_as_model(self):
        source=fixture();source['actions'][0]['tracks'][1]['bone_id']=255
        action,joints,omitted,external=motion._prepare_action(source,0,{255:0},1,binding(source))
        self.assertEqual([j[0]['ordinal'] for j in joints],[1])
        self.assertEqual([e['track'] for e in external],[0])

    def test_identity_and_exact_partition_rejected_before_output(self):
        for changes,code in [({'source_slot':1},'model_curve_identity_mismatch'),
                             ({'source_lmt_sha256':'d'*64},'model_curve_identity_mismatch'),
                             ({'track_ordinals':()},'model_curve_track_mismatch'),
                             ({'track_ordinals':(0,1)},'model_curve_track_mismatch')]:
            with self.subTest(changes=changes),tempfile.TemporaryDirectory() as temporary:
                source=fixture();root=Path(temporary)
                self.fail(code,lambda:self.export(root,source,[binding(source,**changes)]))
                self.assertFalse((root/'out').exists())

    def test_bindings_cannot_be_missing_or_fall_back_silently(self):
        for bindings,code in [([], 'model_curve_count_mismatch'),([None],'invalid_model_curve_binding')]:
            with tempfile.TemporaryDirectory() as temporary:
                self.fail(code,lambda:self.export(Path(temporary),fixture(),bindings))

    def test_duplicate_model_and_unresolved_joint_fail(self):
        source=fixture();duplicate=copy.deepcopy(source['actions'][0]['tracks'][0]);duplicate['ordinal']=2
        source['actions'][0]['tracks'].append(duplicate)
        self.fail('duplicate_model_channel',lambda:motion._prepare_action(source,0,{0:0},1,binding(source,track_ordinals=(0,2))))
        source=fixture();source['actions'][0]['tracks'][1]['bone_id']=123
        self.fail('missing_bone',lambda:motion._prepare_action(source,0,{0:0},1,binding(source)))

    def test_sidecar_owner_comparison_rejects_lost_null_or_changed_sample(self):
        source=fixture()
        package=json.loads(curves.encoded({'entry_id':1,**curves.action_curve(source,0)}))
        paired.verify_curve_action(package,source)
        for mutation in ('default','sample','slot'):
            changed=copy.deepcopy(package)
            if mutation=='default':changed['channels']['rotation']['default']=[0,0,0,1]
            elif mutation=='sample':changed['channels']['translation']['samples'][0][2]=42
            else:changed['source_lmt_sha256']='d'*64
            self.fail('model_curve_content_mismatch',lambda:paired.verify_curve_action(changed,source))

    def test_caller_pinned_sidecar_hash_fails_before_index_or_outputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);path=root/'curves.json';path.write_text('{}')
            self.fail('model_curve_hash_mismatch',lambda:paired.build(root/'missing-index',path,'0'*64,
                root/'missing-model',root/'out',root/'research'))
            self.assertFalse((root/'out').exists())
            self.assertFalse((root/'research').exists())


if __name__=='__main__':unittest.main()
