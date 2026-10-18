# Primeiro recurso com esqueleto real

`tools/resources/model_scene.py` exporta um subconjunto observado do MOD230 para glTF2. O marco usa `/data/em062.arc`, recurso `enemy\em062\mod\em062`: **12 juntas**, uma paleta de 11 juntas usadas, um material, textura ETC1A4 de **128×128** e a primitiva selecionada 1 com **1.516 vértices / 1.564 triângulos**. O nome do monstro exibido pelo jogo não foi estabelecido nesta investigação.

Foram inspecionados os 12 menores `/data/em*.arc`, com todos os modelos desses arquivos registrados em `assets/local/actor-probe/candidates.json`. Isso é uma amostra limitada. Sete dos modelos encontrados não tinham esqueleto; outros quatro tinham 23 juntas. `em062` foi escolhido por ter 12 juntas, vértices de layout uniforme e uma textura diretamente nomeada pelo MRL. O índice completo produzido por `tools/resources/index_resources.py` pode apoiar próximas seleções.

O modelo também contém a primitiva 0: 360 vértices / 452 triângulos, com a mesma caixa que a primitiva 1. Sua finalidade permanece desconhecida; não afirmamos que seja LOD. A escolha de primitiva é um argumento obrigatório para evitar sobrepor silenciosamente alternativas de geometria.

## Reproduzir

Crie uma inspeção com `inspect_rom.py`, conforme `docs/resource-formats.md`. A partir da raiz do projeto:

```sh
python3 -B tools/resources/model_scene.py inventory --inspection research/private/rom-inspection --limit 12 --output assets/local/actor-probe/candidates.json
python3 -B tools/resources/model_scene.py export --inspection research/private/rom-inspection --archive /data/em062.arc --model 'enemy\em062\mod\em062' --primitive 1 --output assets/local/actor-probe
python3 -B -m unittest discover -s tools/resources/tests -v
```

Os artefatos atuais usaram a inspeção anterior fornecida explicitamente em `--inspection ../rea-main/.cache/mh4u-native-2026-10-17/extracted`; esse caminho não está embutido no código. A leitura verifica tamanho/estabilidade da ROM e os limites do intervalo selecionado. O hash integral da ROM é herdado da inspeção, sem releitura de 2,7 GB. Os hashes de ARC, membros e saídas são recalculados. Não há download ou execução do jogo durante o export.

`scene.gltf`, `scene.bin` e `em062_BM.png` formam a cena. MOD/MRL/TEX originais, alpha PGM, `provenance.json` e `validation.json` permanecem junto dela na pasta privada ignorada. Cada saída existente só é aceita se for byte a byte igual. Para alterar a seleção ou o formato, use um novo diretório de saída.

O export usa os parsers ARC, zlib, ETC1/ETC1A4 e strips de `extract_model.py`, sem modificar esse módulo. O parser esquelético recusa MOD diferente de versão230, vértice diferente de tipo4/stride36, material múltiplo, hierarquia cíclica, matrizes inconsistentes, pesos inválidos e índices fora de limites. Todos os campos não interpretados permanecem no MOD retido; os registros de juntas/primitivas e a tabela de remap também ficam em hexadecimal no manifest.

A investigação posterior do player acrescentou a opção explícita
`allow_multiple_materials` ao produtor de geometria, com um pacote completo de
materiais exigido pelo gerador glTF. O comportamento padrão acima continua
restrito a um material e reproduz os mesmos bytes do em062. A associação MRL e
o novo chamador estão descritos em `docs/player-findings.md`.

## Formato confirmado e conversão

Os offsets abaixo são relativos à seção ou registro indicado; inteiros e floats são little-endian.

| Estrutura | Interpretação usada |
| --- | --- |
| Header MOD `0x06/0x24/0x28` | Número de juntas u16, número de paletas u32, offset da seção de juntas u32. |
| Registro de junta, 24 bytes | Byte0: candidato a ID de função; byte1: índice do pai, com255 indicando raiz. Outros bytes e cinco floats são preservados. |
| Após todos os registros | N matrizes A de64 bytes, seguidas de N matrizes B de64 bytes. Ambas são interpretadas column-major. |
| Após as matrizes | Tabela de remap de256 bytes retida bruta; depois paletas de28 bytes: contagem u32 e até24 índices u8. |
| Primitiva `0x25` | Seletor de paleta segundo o importador fixado; no recurso observado vale0. A semântica entre múltiplas paletas não recebeu validação em recursos diferentes. |
| Vértice `0x00/0x0C/0x10` | Posição float32×3, normal i8×3, UV float32×2. |
| Vértice `0x18,0x19,0x20,0x21` | Quatro slots na paleta de juntas. |
| Vértice `0x1A,0x1B,0x22,0x23` | Quatro pesos u8. Todos os1.876 vértices das duas primitivas somam255. |

As duas versões públicas do importador divergem no terceiro/quarto índice/peso. O layout acima, do importador Mobile fixado, soma255 em todos os vértices. A combinação alternativa presente no importador original falha em65 vértices deste recurso. A implementação exige soma255, em vez de normalizar bytes de um layout potencialmente incorreto. Influências repetidas da mesma junta são somadas antes da exportação; slots de peso zero tornam-se `(joint0, weight0)`. O MOD conserva os bytes originais.

As matrizes B **não são diretamente inverse bind matrices** neste arquivo. Para cada junta, calculamos `G[j]` acumulando A pela hierarquia. Observou-se a relação `G[j] × B[j] ≈ Q` para todas as12 juntas, com a mesma Q: escala uniforme **811,9807739257812** e translação **(−230,60704040527344; −142,6981964111328; −544,7486572265625)**. Esses valores também correspondem à maior extensão e ao mínimo da caixa registrados no header do MOD.

O glTF recebe posições `Q × posição_original` e inverse binds `B[j] × inverse(Q)`. As matrizes locais A são preservadas por decomposição TRS e recomposição verificada. Assim, as translações nativas do esqueleto continuam úteis para motion real, e a pose de bind deixa a geometria imóvel. Não copiamos a troca de eixos do importador Blender: a implementação registra e testa a convenção column-major diretamente. Unidades físicas e correspondência em metros não foram estabelecidas; mantemos unidades do modelo fonte.

No recurso escolhido, todas as rotações locais são identity e todas as escalas locais são1. `skin.joints[i]`, `nodes[i]` e índice de junta MOD coincidem para `i=0..11`; o nó12 contém a malha. IDs candidatos por índice: `[0,1,140,141,142,143,144,200,4,3,100,2]`. Pais: `[255,0,1,2,3,4,5,2,1,1,1,10]`. Os IDs ficam em `extras.sourceFunctionIdCandidate`; nomes de ossos e sua função anatômica não foram inventados. A investigação separada de LMT encontrou exatamente esse conjunto de IDs, corroborando a ligação para este recurso.

O material `XfBA0__m00_body` vem do MOD. O primeiro registro TEX do MRL nomeia `enemy\em062\mod\em062_BM`, que está no mesmo ARC. Exportamos seus pixels RGB para PNG e os vinculamos a `baseColorTexture`. Isso preserva a ligação real do recurso, mas **não reimplementa o shader original**: metallic0/roughness1 são parâmetros de preview, alpha é opaco e o canal original é guardado em PGM com significado desconhecido. Normais são renormalizadas; UVs viram `(u,1-v)` para a origem superior esquerda do glTF/PNG. Nenhuma animação é gerada por este módulo.

## Verificação executada

Os11 testes sintéticos não contêm bytes do jogo. Cobrem hierarquia com junta rotacionada, recomposição TRS, conversão Q, bind serializado, truncamento, ciclos, paletas/índices inválidos, peso incorreto, matriz B inconsistente, strip excessivo, formato desconhecido e corrupção de pesos no glTF.

O validador local de bind relê os accessors float32 do glTF produzido e usa a hierarquia/skin exportadas, em vez de validar somente arrays intermediários. Resultado no `em062`:

| Verificação | Resultado |
| --- | --- |
| Vértices / triângulos examinados | 1.516 / 1.564 |
| Maior erro na soma dos pesos | 4,470348358154297e−8 |
| Maior erro em `G × inverseBind ≈ I` | 1,52587890625e−5 |
| Maior deslocamento na pose bind | 1,644586333894187e−5 unidades do modelo |
| RMS do deslocamento | 5,473074807262579e−6 |
| Limite aceito por vértice | 1e−3 |

Uma checagem geométrica adicional encontrou os1.564 triângulos alinhados às normais dos vértices, sem faces opostas ou geometricamente degeneradas. Ainda não há comparação contra frames do jogo original.

A cena também passou pelo [Khronos glTF Validator](https://github.com/KhronosGroup/glTF-Validator), versão fixada **2.0.0-dev.3.10**, com **0 erros, 0 avisos, 0 infos, 0 hints**. A validação foi local e incluiu BIN/PNG; relatório em `assets/local/actor-probe/khronos-validation.json`. O pacote Apache-2.0 foi obtido de `https://registry.npmjs.org/gltf-validator/-/gltf-validator-2.0.0-dev.3.10.tgz`, SHA-256 `4e03dbdc3bc0d1342afd5d2d7ae341a7e3474502ccb872b02dbbaddaee0fdec6`, com integridade SHA-512 do registro conferida antes da extração. A proveniência do download está em `research/private/resources/gltf-validator-package/download-provenance.json`.

Para repetir usando a cópia local instalada nesta máquina e um novo relatório:

```sh
node tools/resources/validate_gltf.cjs --validator research/private/resources/gltf-validator-package/package --asset assets/local/actor-probe/scene.gltf --output assets/local/actor-probe/khronos-validation-repeat.json
```

O wrapper não baixa dependências, exige a versão fixada e lê somente arquivos locais sob a raiz explícita de recursos. Numa instalação nova, é possível obter a mesma versão com `npm install --prefix research/private/resources/gltf-validation --ignore-scripts --no-save gltf-validator@2.0.0-dev.3.10` e passar `--validator research/private/resources/gltf-validation/node_modules/gltf-validator`. O renderer não depende desse validador.

## Fontes e limites de autoridade

- [Importador Mobile, commit2bb9c35be1f0ccfb3899e8317da1b6e6cf011a8e](https://github.com/xZombieAlix/MT-Framework-Mobile---Blender-Model-Script/blob/2bb9c35be1f0ccfb3899e8317da1b6e6cf011a8e/BlenderMTFrameworkMobile.py): layout de pesos/paletas/registros de juntas; SHA-256 `e60b0f91c327c9540aee795a8cce0c5e582a999a7d6ec9489de16e07f81ad61f`. Cabeçalho GPL-3.0-or-later, Copyright2015 Seth VanHeulen. Cópia privada em `research/private/resources/BlenderMTFrameworkMobile.py`, nunca importada/executada pela ferramenta ou renderer.
- [Importador MH4U, commit6ef14c8ffa341ccbe44e0e6a93eb4bfe541265cf](https://github.com/svanheulen/mhff/blob/6ef14c8ffa341ccbe44e0e6a93eb4bfe541265cf/n3ds/io_import_scene_mod.py): base da documentação e comparação de layout. O layout divergente foi tratado como hipótese e confrontado com os bytes, não assumido correto.
- [MH4U rModel](https://github.com/svanheulen/mhff/wiki/MH4U-rModel-Format): estrutura das seções; a própria wiki mantém muitos campos de juntas desconhecidos.
- [Notas MOD3 de outro jogo MT Framework](https://github.com/Ezekial711/MonsterHunterWorldModding/wiki/Mod3-Structure-Notes): contexto da relação entre matrizes locais e absolutas inversas. A aplicabilidade a MH4U foi verificada pela relação algébrica nos bytes deste modelo; o formato MOD3 não foi presumido igual.
- [glTF2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html): contrato de accessors, TRS, skin, matrizes e imagens.

Os resultados estabelecem um recurso esquelético estático coerente e reproduzível. Não estabelecem fidelidade de animação, iluminação, colisão, física, IA ou comportamento do jogo. A proveniência separa os bytes observados, a relação matricial derivada, a interpretação corroborada por referências e os campos ainda desconhecidos.
