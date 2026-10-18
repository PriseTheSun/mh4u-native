"""One explicit diagnostic scene recipe shared by launcher and packager."""
from pathlib import Path
from lab_catalog import RESOURCES

WALL_SOURCE = 'stage-composition-probe/v00a01-walls/scene.gltf'
WALL_PACKAGE = 'walls/scene.gltf'
RECIPE_NAME = 'village-native-actor-v2.world.tsv'
PROVENANCE_NAME = 'village-native-actor-v2.provenance.json'


def write_world_recipe(directory: Path, packaged=False):
    by_key = {item.key: item for item in RESOURCES}
    values = {}
    for role, key in [('stage', 'composition'), ('actor', 'native-hunter'), ('floor', 'collision')]:
        resource = by_key[key]
        values[role] = resource.packaged_model if packaged else resource.source_model
    values['walls'] = WALL_PACKAGE if packaged else WALL_SOURCE
    # Authored diagnostic values, deliberately not named as original game data.
    values.update({'spawn-x': '-61.14208710193634', 'spawn-y': '-17.212244669596355',
                   'spawn-z': '11963.744791666666', 'heading': '0', 'radius': '22',
                   'height': '175', 'speed': '150', 'min-normal-y': '0.8',
                   'actor-ground-offset': '2.09622'})
    for role in ('stage', 'actor', 'floor', 'walls'):
        if not (directory/values[role]).is_file():
            raise ValueError(f'Missing diagnostic world resource: {role}')
    data = ('MH4U-DIAGNOSTIC-WORLD 1\n'+''.join(k+'\t'+v+'\n' for k,v in values.items())).encode('utf-8')
    path = directory/RECIPE_NAME
    try:
        with path.open('xb') as stream:
            stream.write(data)
    except FileExistsError:
        if path.read_bytes() != data:
            raise ValueError(f'Existing world recipe differs; retain it and use a new version: {path}')
    return path
