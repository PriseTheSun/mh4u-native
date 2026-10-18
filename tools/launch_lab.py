#!/usr/bin/env python3
"""Open one locally extracted model in the native resource laboratory."""
import argparse
from pathlib import Path
import subprocess
import sys
from lab_catalog import RESOURCES, RESOURCE_ORDER, write_catalog
from world_recipe import write_world_recipe

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', choices=RESOURCE_ORDER, default='actor')
    parser.add_argument('--world', action='store_true', help='Open the diagnostic village and controllable hunter')
    parser.add_argument('--action', type=int, choices=range(7), help='Inspect a paired native action (0..6)')
    args, extra = parser.parse_known_args()
    spec = next(resource for resource in RESOURCES if resource.key == args.model)
    model = ROOT/'assets/local'/spec.source_model
    binary = ROOT/'build/mh4u-resource-lab'
    if not binary.is_file():
        parser.error('Build first: python3 tools/build.py macos')
    if args.action is None and not args.world and not model.is_file():
        parser.error('Import the local assets first; see README.md')
    custom_texture = any(flag in extra for flag in ('--texture', '--alpha', '--texture-alternative', '--alpha-alternative'))
    if args.action is not None:
        if args.world or args.model != 'actor' or custom_texture:
            parser.error('--action selects its own resources')
        command = [str(binary), '--motion-package', str(ROOT/'assets/local/model-motion-native-v2/model-motion.bin'),
                   '--paired-root', str(ROOT/'assets/local/paired-player-actions-v1'), '--motion-action', str(args.action)]
        if '--events-package' not in extra:
            command += ['--events-package',str(ROOT/'assets/local/motion-events-native-v1/motion-events.bin')]
    elif args.world:
        if args.model != 'actor' or custom_texture:
            parser.error('--world selects its own resources')
        command = [str(binary), '--world', str(write_world_recipe(ROOT/'assets/local'))]
    elif custom_texture:
        command = [str(binary), '--model', str(model)]
        if spec.source_texture:
            command += ['--texture', str(ROOT/'assets/local'/spec.source_texture)]
    else:
        catalog = write_catalog(ROOT/'assets/local/laboratory-v11.catalog.tsv')
        command = [str(binary), '--catalog', str(catalog), '--resource', str(RESOURCE_ORDER.index(args.model))]
    text_catalog = ROOT/'assets/local/localization-native-v4/catalog.mhtxt'
    if '--text-catalog' not in extra and text_catalog.is_file():
        command += ['--text-catalog', str(text_catalog)]
    return subprocess.call([*command, *extra], cwd=ROOT)


if __name__ == '__main__':
    sys.exit(main())
