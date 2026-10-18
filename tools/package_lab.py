#!/usr/bin/env python3
"""Make local review packages. Includes only the selected local resource sample."""
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import time
from lab_catalog import RESOURCES, RESOURCE_ORDER, write_catalog
from network_manifest import generate as generate_manifest
from world_recipe import WALL_SOURCE, WALL_PACKAGE, RECIPE_NAME, PROVENANCE_NAME, write_world_recipe

ROOT = Path(__file__).resolve().parents[1]
LLVM = ROOT/'.tools/llvm-mingw-20261006-ucrt-macos-universal'


def scene_resources(source_directory, scene_name, destination):
    """Copy only the generated scene and its explicit, local dependencies."""
    scene = json.loads((source_directory/scene_name).read_text())
    names = {scene_name}
    for resource in scene.get('buffers', []) + scene.get('images', []):
        uri = resource.get('uri')
        if (not isinstance(uri, str) or not uri or Path(uri).name != uri
                or ':' in uri or '\\' in uri or uri in ('.', '..')):
            raise ValueError('Package accepts only flat local glTF resources')
        names.add(uri)
    destination.mkdir(parents=True)
    for name in sorted(names):
        source = source_directory/name
        if source.is_symlink() or not source.is_file():
            raise ValueError(f'Expected generated local resource: {name}')
        shutil.copy2(source, destination/name)


def resources(destination):
    destination.mkdir(parents=True)
    copied = set()
    for resource in RESOURCES:
        source = ROOT/'assets/local'/resource.source_model
        target = destination/resource.packaged_model
        if source.suffix == '.mhnative':
            native_actor_resources(source,target)
        elif source.suffix == '.gltf':
            if source.name != target.name:
                raise ValueError('Scene filename must be preserved by packaging')
            scene_resources(source.parent, source.name, target.parent)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        if resource.source_texture and resource.packaged_texture not in copied:
            shutil.copy2(ROOT/'assets/local'/resource.source_texture, destination/resource.packaged_texture)
            copied.add(resource.packaged_texture)
    write_catalog(destination/'catalog.tsv', packaged=True)
    generate_manifest(destination/'actor/animated.gltf', destination/'actor/session.manifest')
    native_translation(destination/'texts')
    wall_source=ROOT/'assets/local'/WALL_SOURCE
    scene_resources(wall_source.parent,wall_source.name,(destination/WALL_PACKAGE).parent)
    write_world_recipe(destination,packaged=True)
    shutil.copy2(ROOT/'assets/local'/PROVENANCE_NAME,destination/'world-source-provenance.json')
    paired_motion(destination/'paired-motion')
    motion_events(destination/'motion-events')
    player_preview_constants(destination/'player-preview')


def player_preview_constants(destination):
    source=ROOT/'research/private/player-transition-runtime-v1/generated/player-transition-constants.bin'
    data=source.read_bytes()
    if len(data)!=88 or hashlib.sha256(data).hexdigest()!='1cf3b6e6873f77008eb072e18e27579e6bec375622c7d4780413f06f1cee3d5f':
        raise ValueError('Player transition constants differ from the verified source')
    destination.mkdir()
    (destination/'constants.bin').write_bytes(data)


def native_actor_resources(source,target):
    """Copy the recipe's explicit SHA-bound local inputs, without converted meshes."""
    raw=source.read_bytes()
    if len(raw)>65536 or not raw.startswith(b'MH4U-NATIVE-ACTOR 1\n'):
        raise ValueError('Unsupported native actor recipe')
    selected={};total=0
    for line in raw.decode('utf-8').splitlines()[1:]:
        row=line.split('\t')
        if row[0] not in ('model','motion','texture','commands'):continue
        if len(row)!=3 or not row[1] or Path(row[1]).name!=row[1] or ':' in row[1] or '\\' in row[1]:
            raise ValueError('Package requires flat native actor dependencies')
        path=source.parent/row[1]
        if path.is_symlink() or not path.is_file():raise ValueError('Invalid native actor dependency')
        total+=path.stat().st_size
        if total>128*1024*1024:raise ValueError('Native actor package exceeds policy')
        data=path.read_bytes()
        if hashlib.sha256(data).hexdigest()!=row[2]:raise ValueError('Native actor input digest mismatch')
        if row[1] in selected and selected[row[1]]!=data:raise ValueError('Conflicting native dependency')
        selected[row[1]]=data
    target.parent.mkdir(parents=True)
    target.write_bytes(raw)
    for name,data in selected.items():(target.parent/name).write_bytes(data)


def paired_motion(destination):
    source=ROOT/'assets/local/model-motion-native-v2'
    paired=ROOT/'assets/local/paired-player-actions-v1'
    identity=json.loads((source/'identity.json').read_text())
    binary=(source/'model-motion.bin').read_bytes()
    if len(binary)>8*1024*1024 or hashlib.sha256(binary).hexdigest()!=identity['package_sha256']:
        raise ValueError('Native motion container differs from selected identity')
    destination.mkdir()
    (destination/'model-motion.bin').write_bytes(binary)
    shutil.copy2(source/'identity.json',destination/'identity.json')
    shutil.copy2(paired/'manifest.json',destination/'manifest.json')
    names=set();total=0
    for binding in [identity['gltf'],identity['curves'],*identity['dependencies']]:
        name=binding['relative_path'];relative=Path(name)
        if relative.is_absolute() or '..' in relative.parts or ':' in name or '\\' in name or name in names:
            raise ValueError('Invalid or duplicate paired dependency')
        names.add(name);path=paired/relative
        if path.is_symlink() or not path.resolve().is_relative_to(paired.resolve()):
            raise ValueError('Paired dependency escapes selected root')
        total+=path.stat().st_size
        if total>64*1024*1024:raise ValueError('Paired dependencies exceed selected package budget')
        data=path.read_bytes()
        if hashlib.sha256(data).hexdigest()!=binding['sha256']:raise ValueError('Paired dependency hash differs')
        target=destination/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)



def motion_events(destination):
    source=ROOT/'assets/local/motion-events-native-v1'
    identity=json.loads((source/'identity.json').read_text())
    binary=(source/'motion-events.bin').read_bytes()
    if len(binary)>8*1024*1024 or hashlib.sha256(binary).hexdigest()!=identity['package_sha256']:
        raise ValueError('Native events container differs from selected identity')
    destination.mkdir()
    (destination/'motion-events.bin').write_bytes(binary)
    shutil.copy2(source/'identity.json',destination/'identity.json')


def native_translation(destination):
    source = ROOT/'assets/local/localization-native-v4'
    manifest_bytes = (source/'manifest.json').read_bytes()
    manifest = json.loads(manifest_bytes)
    if (manifest.get('schema') != 'mh4u-native-text-manifest-v1'
            or manifest.get('package_file') != 'catalog.mhtxt'):
        raise ValueError('Unsupported native text manifest')
    data = (source/'catalog.mhtxt').read_bytes()
    if (len(data) != manifest['package_size']
            or hashlib.sha256(data).hexdigest() != manifest['package_sha256']):
        raise ValueError('Native text catalog differs from its manifest')
    destination.mkdir()
    (destination/'catalog.mhtxt').write_bytes(data)
    (destination/'manifest.json').write_bytes(manifest_bytes)
    shutil.copy2(source/'provenance.json', destination/'provenance.json')


def notices(destination):
    destination.mkdir(parents=True)
    for source, name in ((ROOT/'vendor/SDL3-3.4.18/LICENSE.txt', 'SDL.txt'),
                         (ROOT/'vendor/imgui/LICENSE.txt', 'ImGui.txt'),
                         (ROOT/'vendor/cgltf/LICENSE', 'cgltf.txt'),
                         (ROOT/'vendor/picosha2/LICENSE', 'PicoSHA2.txt'),
                         (LLVM/'LICENSE.TXT', 'LLVM-MinGW.txt')):
        shutil.copy2(source, destination/name)
    for source in (LLVM/'x86_64-w64-mingw32/share/mingw32').glob('COPYING*'):
        shutil.copy2(source, destination/source.name)


def reference_translation(destination):
    """Copy only the frozen, hash-bound experimental reference overrides."""
    recipe_path = ROOT/'localization/pt-BR/reference-package.json'
    recipe_bytes = recipe_path.read_bytes()
    recipe = json.loads(recipe_bytes)
    if recipe.get('schema') != 'mh4u-reference-translation-package-v1':
        raise ValueError('Unsupported reference translation package recipe')
    prepared, names = [], set()
    for entry in recipe['files']:
        for key in ('source', 'destination'):
            relative = Path(entry[key])
            if (relative.is_absolute() or not relative.parts or '..' in relative.parts
                    or ':' in entry[key] or '\\' in entry[key]):
                raise ValueError('Reference package requires local relative paths')
        if entry['destination'] in names:
            raise ValueError('Duplicate reference package destination')
        names.add(entry['destination'])
        source = ROOT/entry['source']
        if source.is_symlink() or not source.resolve().is_relative_to(ROOT.resolve()):
            raise ValueError('Reference package source escapes project')
        if type(entry['size']) is not int or not 0 <= entry['size'] <= 8*1024*1024:
            raise ValueError('Reference package file exceeds the local 8 MiB policy')
        with source.open('rb') as stream:
            data = stream.read(entry['size']+1)
        if len(data) != entry['size'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError(f'Reference translation changed; review recipe before packaging: {source}')
        prepared.append((entry['destination'], data))
    destination.mkdir(parents=True)
    for name, data in prepared:
        target = destination/name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    (destination/'provenance.json').write_bytes(recipe_bytes)
    (destination/'LEIA-ME.txt').write_text(
        'PT-BR experimental — recursos locais de comparação\n\n'
        '160 fragmentos alterados em Title, CharMake e Common. Tradução parcial, em rascunho.\n'
        'Fonte: ROM EU CTR-P-BFGP, title ID 0004000000126100; SHA registrado na proveniência.\n'
        'A pasta romfs contém os dois overrides gerados, sem alterar a ROM original.\n'
        'Foram testados no perfil separado do Azahar 2126.2 usado como referência.\n'
        'Dois cabeçalhos, 14 fragmentos da criação e os botões Voltar/Confirmar foram observados.\n'
        'As capturas não verificam todas as telas nem constituem revisão humana completa.\n'
        'Os rótulos gráficos do menu principal ainda permanecem em inglês.\n\n'
        'O laboratório nativo usa um catálogo separado para inspecionar os fragmentos.\n'
        'O layout e os comandos dos menus originais ainda não foram implementados.\n'
        'Estes overrides não são necessários para abrir a prévia nativa.\n'
        'Catálogos e relatórios estão incluídos; cada arquivo tem hash na proveniência.\n'
        'Para reconstruir, use tools/localization/arc_patch.py no projeto, com os catálogos\n'
        'e ARC originais correspondentes. Não aplique os dados a outra edição por semelhança de nome.\n',
        encoding='utf-8')


def main():
    output = ROOT/'build/packages'/time.strftime('%Y%m%d-%H%M%S')
    output.mkdir(parents=True, exist_ok=False)
    app = output/'macOS/MH4U - Laboratorio.app'
    contents = app/'Contents'
    macos = contents/'MacOS'
    macos.mkdir(parents=True)
    shutil.copy2(ROOT/'build/mh4u-resource-lab', macos/'mh4u-resource-lab')
    for tool in ('mh4u-network-lab', 'mh4u-collision-query', 'mh4u-action-motion'):
        shutil.copy2(ROOT/'build'/tool, macos/tool)
    launch = macos/'launch'
    launch.write_text('''#!/bin/sh
base="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)" || exit 1
exec "$base/MacOS/mh4u-resource-lab" --default-catalog "$base/Resources/assets/catalog.tsv" --text-catalog "$base/Resources/assets/texts/catalog.mhtxt" "$@"
''')
    launch.chmod(0o755)
    with (contents/'Info.plist').open('wb') as f:
        plistlib.dump({'CFBundleExecutable': 'launch', 'CFBundleIdentifier': 'local.mh4u.resource-lab',
                      'CFBundleName': 'MH4U - Laboratório', 'CFBundlePackageType': 'APPL',
                      'CFBundleShortVersionString': '0.17.0', 'CFBundleVersion': '18',
                      'NSHighResolutionCapable': True}, f)
    resources(contents/'Resources/assets')
    notices(contents/'Resources/notices')
    shortcuts = (('cenario', 'stage'), ('colisao', 'collision'), ('cacador parcial', 'player'),
                 ('cacador cinco pecas', 'assembly'), ('cacador com rosto', 'appearance'),
                 ('vila composta', 'composition'), ('ator nativo', 'native-actor'),
                 ('cacador nativo', 'native-hunter'))
    for label, key in shortcuts:
        index = RESOURCE_ORDER.index(key)
        shortcut = output/'macOS'/f'Abrir {label}.command'
        shortcut.write_text(
            '#!/bin/sh\n'
            'base="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1\n'
            'app="$base/MH4U - Laboratorio.app"\n'
            f'open -n "$app" --args --resource {index} "$@"\n')
        shortcut.chmod(0o755)
    world_shortcut=output/'macOS/Abrir cena controlavel.command'
    world_shortcut.write_text(
        '#!/bin/sh\n'
        'base="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1\n'
        'app="$base/MH4U - Laboratorio.app"\n'
        f'open -n "$app" --args --world "$app/Contents/Resources/assets/{RECIPE_NAME}" --hide-primitive 3 "$@"\n')
    world_shortcut.chmod(0o755)
    player_shortcut=output/'macOS/Abrir controlador nativo.command'
    player_shortcut.write_text(
        '#!/bin/sh\n'
        'base="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1\n'
        'app="$base/MH4U - Laboratorio.app"\n'
        f'open -n "$app" --args --world "$app/Contents/Resources/assets/{RECIPE_NAME}" '
        '--player-preview "$app/Contents/Resources/assets/player-preview/constants.bin" --hide-primitive 3 "$@"\n')
    player_shortcut.chmod(0o755)
    for index in range(7):
        shortcut=output/'macOS'/f'Abrir acao pareada {index}.command'
        shortcut.write_text('#!/bin/sh\nbase="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1\napp="$base/MH4U - Laboratorio.app"\n'
            'motion="$app/Contents/Resources/assets/paired-motion"\n'
            f'open -n "$app" --args --motion-package "$motion/model-motion.bin" --paired-root "$motion" --motion-action {index} --events-package "$app/Contents/Resources/assets/motion-events/motion-events.bin" --hide-primitive 3 "$@"\n')
        shortcut.chmod(0o755)
    server_shortcut=output/'macOS/Iniciar servidor local.command'
    server_shortcut.write_text(
        '#!/bin/sh\n'
        'base="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1\n'
        'app="$base/MH4U - Laboratorio.app"\n'
        f'exec "$app/Contents/MacOS/mh4u-network-lab" server --world "$app/Contents/Resources/assets/{RECIPE_NAME}" --port 47124 --run-ms 86400000 "$@"\n')
    server_shortcut.chmod(0o755)
    for player in (1,2):
        shortcut=output/'macOS'/f'Abrir jogador {player}.command'
        shortcut.write_text('#!/bin/sh\nbase="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1\napp="$base/MH4U - Laboratorio.app"\n'
            f'open -n "$app" --args --world "$app/Contents/Resources/assets/{RECIPE_NAME}" --connect-port 47124 --player-name Cacador{player} --hide-primitive 3 "$@"\n')
        shortcut.chmod(0o755)
    windows = output/'Windows-x64'
    windows.mkdir()
    shutil.copy2(ROOT/'build/windows-x64/mh4u-resource-lab.exe', windows/'mh4u-resource-lab.exe')
    for tool in ('mh4u-network-lab', 'mh4u-collision-query', 'mh4u-action-motion'):
        shutil.copy2(ROOT/'build/windows-x64'/(tool+'.exe'), windows/(tool+'.exe'))
    resources(windows/'assets')
    notices(windows/'notices')
    (windows/'Abrir ator.bat').write_bytes((
        '@echo off\r\ncd /d "%~dp0"\r\n'
        '"%~dp0mh4u-resource-lab.exe" --catalog "%~dp0assets\\catalog.tsv" --text-catalog "%~dp0assets\\texts\\catalog.mhtxt" %*\r\n'
        'if errorlevel 1 pause\r\n').encode('ascii'))
    for label, key in shortcuts:
        index = RESOURCE_ORDER.index(key)
        (windows/f'Abrir {label}.bat').write_bytes((
            '@echo off\r\ncd /d "%~dp0"\r\n'
            f'"%~dp0mh4u-resource-lab.exe" --catalog "%~dp0assets\\catalog.tsv" --text-catalog "%~dp0assets\\texts\\catalog.mhtxt" --resource {index} %*\r\n'
            'if errorlevel 1 pause\r\n').encode('ascii'))
    for label, model in (('escudo', 'sld133.obj'), ('espada', 'one133.obj')):
        (windows/f'Abrir {label}.bat').write_bytes((
            '@echo off\r\ncd /d "%~dp0"\r\n'
            f'"%~dp0mh4u-resource-lab.exe" --model "%~dp0assets\\{model}" '
            '--texture "%~dp0assets\\one133_BM.ppm" --text-catalog "%~dp0assets\\texts\\catalog.mhtxt" %*\r\n'
            'if errorlevel 1 pause\r\n').encode('ascii'))
    (windows/'Abrir cena controlavel.bat').write_bytes((
        '@echo off\r\ncd /d "%~dp0"\r\n'
        f'"%~dp0mh4u-resource-lab.exe" --world "%~dp0assets\\{RECIPE_NAME}" '
        '--text-catalog "%~dp0assets\\texts\\catalog.mhtxt" --hide-primitive 3 %*\r\n'
        'if errorlevel 1 pause\r\n').encode('ascii'))
    (windows/'Abrir controlador nativo.bat').write_bytes((
        '@echo off\r\ncd /d "%~dp0"\r\n'
        f'"%~dp0mh4u-resource-lab.exe" --world "%~dp0assets\\{RECIPE_NAME}" '
        '--player-preview "%~dp0assets\\player-preview\\constants.bin" '
        '--text-catalog "%~dp0assets\\texts\\catalog.mhtxt" --hide-primitive 3 %*\r\n'
        'if errorlevel 1 pause\r\n').encode('ascii'))
    (windows/'Iniciar servidor local.bat').write_bytes((
        '@echo off\r\ncd /d "%~dp0"\r\n'
        f'"%~dp0mh4u-network-lab.exe" server --world "%~dp0assets\\{RECIPE_NAME}" --port 47124 --run-ms 86400000 %*\r\n'
        'if errorlevel 1 pause\r\n').encode('ascii'))
    for player in (1,2):
        (windows/f'Abrir jogador {player}.bat').write_bytes((
            '@echo off\r\ncd /d "%~dp0"\r\n'
            f'"%~dp0mh4u-resource-lab.exe" --world "%~dp0assets\\{RECIPE_NAME}" --connect-port 47124 --player-name Cacador{player} '
            '--text-catalog "%~dp0assets\\texts\\catalog.mhtxt" --hide-primitive 3 %*\r\nif errorlevel 1 pause\r\n').encode('ascii'))
    for index in range(7):
        (windows/f'Abrir acao pareada {index}.bat').write_bytes((
            '@echo off\r\ncd /d "%~dp0"\r\n'
            f'"%~dp0mh4u-resource-lab.exe" --motion-package "%~dp0assets\\paired-motion\\model-motion.bin" --paired-root "%~dp0assets\\paired-motion" --motion-action {index} --events-package "%~dp0assets\\motion-events\\motion-events.bin" '
            '--text-catalog "%~dp0assets\\texts\\catalog.mhtxt" --hide-primitive 3 %*\r\nif errorlevel 1 pause\r\n').encode('ascii'))
    reference_translation(output/'PT-BR-experimental')
    (output/'LEIA-ME.txt').write_text(
        'MH4U - Laboratório de recursos nativo, marco 0.17.0\n\n'
        'Novo atalho: Abrir controlador nativo. Exibe o Player persistente no caçador da vila.\n'
        'Duas camadas ativas, oito posições e hierarquia MOD; recursos LMT e desenho nativos.\n'
        'Espaço: pausa; ponto: um passo; 1/3: pedir estado; Home: reiniciar a inspeção.\n'
        'Os pedidos são manuais e o ritmo de 60 passos/s é diagnóstico. Não é o bootstrap do jogo.\n'
        'Esse perfil ainda não sincroniza ações em rede nem mapeia a política original de input.\n\n'
        'Novos atalhos: Abrir cacador nativo e Abrir ator nativo.\n'
        'Leem geometria, pesos e animações diretamente de MOD/LMT; sem conversão para glTF.\n'
        '72 ações selecionadas: 44 do caçador e 28 do em062. Relógio, movimento do modelo e eventos nativos.\n'
        'O seletor preserva a pose ao trocar ações; a linha do tempo busca uma ação isolada.\n'
        'Metadados dos ossos usam os registros MOD. A política do controlador original continua pendente.\n\n'
        'Mac: abra macOS/MH4U - Laboratorio.app.\n'
        'Windows: abra Windows-x64/Abrir ator.bat. Atalhos de espada/escudo também incluídos.\n\n'
        'A cena controlável e a sessão local agora carregam o caçador diretamente de MOD/LMT.\n'
        'As dependências nativas também entram na identidade verificada da sala.\n'
        'Abrir cena controlavel reúne caçador, cenário e colisão: WASD/analógico esquerdo para mover.\n'
        'Mouse/analógico direito controla a câmera. A primitiva 3 fica oculta nesse atalho por inspeção.\n'
        'É locomoção diagnóstica com disco apoiado no piso, paredes e bordas; sem saltos ou escadas.\n'
        'Velocidade, ponto inicial, alinhamento dos pés e animação são parâmetros de inspeção.\n'
        'Não reproduz ainda o controlador, física, ações ou spawn originais.\n\n'
        'Modelo em062, esqueleto de 12 juntas e 28 ações extraídos da ROM local.\n'
        'Atalhos de cenário v00a01 e malha de colisão extraída incluídos.\n'
        'Seletor de onze recursos; torso, cinco peças e candidato com rosto/cabelo, com 19 juntas e 44 ações.\n'
        'A seleção original de aparência, geometria e cores ainda está em investigação.\n'
        'Partes da geometria permite inspecionar primitivas; todas aparecem por padrão.\n'
        'Vila composta inclui acessório posicionado pela primeira chave do SDL; não valida o estado inicial original.\n'
        'Composição completa, tempo do cenário, spawn e física ainda estão em pesquisa.\n'
        'Render, skinning e reprodução de animação próprios em C++/OpenGL.\n'
        'Animações com codecs e curvas corrigidos a partir dos consumidores ARM.\n'
        '60 quadros/s é uma convenção de inspeção; ritmo original não foi validado.\n'
        'Interface PT-BR, órbita de câmera, seleção de texturas e configuração de controle.\n'
        'Este laboratório ainda não contém combate, missões ou save.\n'
        'Movimento compartilhado: abra Iniciar servidor local, depois Abrir jogador 1 e Abrir jogador 2.\n'
        'Os clientes compartilham posições e colisão calculadas pelo servidor; até quatro participantes.\n'
        'Corrigida a entrada quando a ordem das conexões difere da ordem de identificação dos jogadores.\n'
        'Este teste conecta somente processos no mesmo computador, pela porta local 47124.\n'
        'Para encerrar o servidor use Ctrl+C na janela dele; dura no máximo 24 horas.\n'
        'Não há colisão entre jogadores, combate cooperativo, sincronização de ações ou acesso pela internet.\n'
        'CLIs de diagnóstico: mh4u-network-lab, mh4u-collision-query e mh4u-action-motion.\n'
        'A CLI de movimento lê assets/paired-motion/model-motion.bin com --paired-root assets/paired-motion.\n'
        'Use --action 0..6 --ticks N [--repeat]; a saída são estados numéricos em frames da fonte.\n'
        'Opcional: --events-package assets/motion-events/motion-events.bin acrescenta eventos numéricos.\n'
        'Abrir acao pareada 0..6 exibe pose e deslocamento nativo juntos, fora da cena de rede.\n'
        'Espaço pausa, ponto avança um tick, Home reinicia e pausa; 60 ticks/s é política de inspeção.\n'
        'Eventos de animação são consultados nativamente e exibidos como máscaras; sem disparar efeitos.\n'
        'As sete ações ainda não controlam a cena compartilhada ou combate.\n'
        'O manifesto de rede fica em assets/actor/session.manifest.\n'
        'PT-BR-experimental contém 160 fragmentos alterados em overrides da referência separada.\n'
        'Catálogo nativo: 1.082 fragmentos, 925 traduções em rascunho e 157 fallbacks preservados.\n'
        'Textos do jogo permite selecionar documento/fragmento e alternar PT-BR/inglês.\n'
        'Layouts e comandos dos menus originais ainda não foram implementados.\n'
        'Packs PNG por hash: use --texture-pack /caminho/pack.tsv; imagens sem substituição mantêm a original.\n'
        'Não há pack HD incluído. --texture-mode alternative seleciona um pack fornecido.\n'
        'Não representa o jogo completo nem valida material, pose ou iluminação original.\n'
        'Windows foi compilado no Mac; execução em Windows ainda não foi validada.\n'
        'Pacotes locais de desenvolvimento, sem assinatura/notarização de distribuição.\n', encoding='utf-8')
    files = [{ 'path': str(p.relative_to(output)), 'size': p.stat().st_size,
               'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
             for p in sorted(output.rglob('*')) if p.is_file()]
    (output/'manifest.json').write_text(json.dumps({'milestone': 'resource-lab-0.17.0',
                                                    'files': files}, indent=2)+'\n')
    (ROOT/'research/private/latest-package.txt').write_text(str(output)+'\n')
    print(output)


if __name__ == '__main__':
    main()
