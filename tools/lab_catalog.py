"""Explicit resource catalog shared by the local launcher and package builder."""
from pathlib import Path
from dataclasses import dataclass


@dataclass(frozen=True)
class Resource:
    key: str
    label: str
    source_model: str
    packaged_model: str
    source_texture: str = ''
    packaged_texture: str = ''


RESOURCES = (
    Resource('actor', 'Ator em062: 28 ações', 'animation-probe/em062-actions/animated.gltf', 'actor/animated.gltf'),
    Resource('stage', 'Cenário v00a01', 'stage-probe/v00a01/scene.gltf', 'stage/scene.gltf'),
    Resource('collision', 'Colisão v00a01', 'collision-probe/v00a01-core2/scene.gltf', 'collision/scene.gltf'),
    Resource('shield', 'Escudo one133', 'one133/sld133.obj', 'sld133.obj', 'one133/one133_BM.ppm', 'one133_BM.ppm'),
    Resource('sword', 'Espada one133', 'one133/one133.obj', 'one133.obj', 'one133/one133_BM.ppm', 'one133_BM.ppm'),
    Resource('player', 'Caçador: torso parcial, 44 ações', 'player-probe/body001-actions/animated.gltf', 'player/animated.gltf'),
    Resource('assembly', 'Caçador: cinco peças, 44 ações', 'player-assembly-probe/male001-actions/animated.gltf', 'assembly/animated.gltf'),
    Resource('appearance', 'Caçador com rosto/cabelo: 44 ações', 'player-assembly-probe/seven-piece-actions/animated.gltf', 'appearance/animated.gltf'),
    Resource('composition', 'Vila: composição das primeiras chaves', 'stage-composition-probe/v00a01-first-keys/scene.gltf', 'composition/scene.gltf'),
    Resource('native-actor', 'Ator em062: animação nativa MOD/LMT', 'native-actor-v1/em062/em062.mhnative', 'native-actor/em062.mhnative'),
    Resource('native-hunter', 'Caçador: animação nativa MOD/LMT', 'native-actor-v1/hunter-seven/hunter-seven.mhnative', 'native-hunter/hunter-seven.mhnative'),
)
RESOURCE_ORDER = tuple(resource.key for resource in RESOURCES)


def catalog(packaged=False):
    rows = []
    for resource in RESOURCES:
        model = resource.packaged_model if packaged else resource.source_model
        texture = resource.packaged_texture if packaged else resource.source_texture
        rows.append('\t'.join((resource.label, model, texture, '', '', ''))+'\n')
    return 'MH4U-LAB-CATALOG 1\n'+''.join(rows)


def write_catalog(path: Path, packaged=False):
    data = catalog(packaged).encode('utf-8')
    try:
        with path.open('xb') as stream:
            stream.write(data)
    except FileExistsError:
        if path.read_bytes() != data:
            raise ValueError(f'Existing catalog differs; preserve it and choose a new path: {path}')
    return path
