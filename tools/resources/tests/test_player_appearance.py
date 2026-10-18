"""Explicit selection and material dependency boundaries for appearance previews."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import player_appearance as appearance


def fixture():
    return {'actor':{'material_names':['a','b','c'],'meshes':[
                {'primitive_index':0,'material_index':2,'positions':[[1,2,3]]},
                {'primitive_index':1,'material_index':0,'positions':[[4,5,6]]},
                {'primitive_index':2,'material_index':2,'positions':[[7,8,9]]}]},
            'material_bundle':{
                'materials':[{'name':n,'pbrMetallicRoughness':{'baseColorTexture':{'index':i}}} for n,i in [('a',1),('b',0),('c',1)]],
                'textures':[{'source':1,'sampler':0},{'source':0,'sampler':0}],
                'images':[{'uri':'used.png'},{'uri':'unused.png'}]},
            'files':[('used.png',b'rgb'),('used-alpha.pgm',b'alpha'),('unused.png',b'other')]}


class AppearanceTests(unittest.TestCase):
    def test_selection_preserves_order_identity_and_prunes_unused_tables(self):
        piece=fixture();original=copy.deepcopy(piece)
        selected,bundle,files=appearance.select_piece(piece,[2,1])
        self.assertEqual(piece,original)
        self.assertEqual([m['primitive_index'] for m in selected['meshes']],[2,1])
        self.assertEqual([m['positions'] for m in selected['meshes']],[[[7,8,9]],[[4,5,6]]])
        self.assertEqual(selected['material_names'],['a','c'])
        self.assertEqual([m['material_index'] for m in selected['meshes']],[1,0])
        self.assertEqual(bundle['images'],[{'uri':'used.png'}])
        self.assertEqual(bundle['textures'],[{'source':0,'sampler':0}])
        self.assertEqual([m['pbrMetallicRoughness']['baseColorTexture']['index'] for m in bundle['materials']],[0,0])
        self.assertEqual(files,[('used.png',b'rgb'),('used-alpha.pgm',b'alpha')])

    def test_empty_duplicate_out_of_range_and_noninteger_selections_rejected(self):
        for indices in [[],[0,0],[-1],[3],[True],[0.0]]:
            with self.subTest(indices=indices),self.assertRaisesRegex(ValueError,'distinct existing'):
                appearance.select_piece(fixture(),indices)


if __name__=='__main__':unittest.main()
