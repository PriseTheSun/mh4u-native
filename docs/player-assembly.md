# Montagem auditável de peças do caçador

O candidato em `assets/local/player-assembly-probe/male001-actions/animated.gltf`
combina cinco recursos reais: corpo, braços, pernas, cintura e acessório de
cabeça. São 3.918 vértices, 4.311 triângulos, 14 primitivas, 13 materiais,
6 texturas e uma rig compartilhada de 19 juntas. Os 44 clips conservam os
canais do export de torso descrito em [player-findings.md](player-findings.md).
Não representam 44 comportamentos identificados: são 34 conjuntos numéricos
distintos, com dez duplicatas de origem preservadas.

Esta é uma composição compatível na pose de referência. Ainda faltam seleção
de rosto/cabelo, regras de visibilidade e personalização, identidade do
equipamento no jogo e controlador de ações. A peça `m_helm001` é um acessório
pequeno e não completa rosto ou cabeça. Nenhum conjunto inicial do jogo foi
inferido a partir do sufixo `001`.

## Associação sustentada pelos bytes

`tools/resources/player_assembly.py` recebe IDs explícitos do índice local,
reutiliza a extração verificada e a resolução MOD → MRL → TEX de
`player_probe.load_piece`, e conserva os registros brutos na proveniência.
A associação exige IDs de junta únicos, presença no corpo base, pais internos
compatíveis, matrizes globais de referência e inverse binds coincidentes com
tolerância de `1e-4` por componente. Uma raiz de peça pode corresponder a uma
junta interna do corpo somente quando essas matrizes coincidem. Não há ajuste
de escala, rotação, translação ou retargeting.

| Recurso selecionado | Entry | Juntas / primitivas | Vértices / triângulos | Maior diferença global / inverse bind |
| --- | ---: | ---: | ---: | --- |
| `m_body001` | 44780 | 19 / 4 | 1.280 / 1.506 | 0 / 0 |
| `m_arm001` | 42456 | 9 / 2 | 578 / 742 | 1,50e−7 / 7,63e−6 |
| `m_leg001` | 49875 | 7 / 2 | 1.079 / 1.194 | 0 / 1,91e−6 |
| `m_wst001` | 52640 | 6 / 3 | 580 / 481 | 1,40e−7 / 7,44e−6 |
| `m_helm001` | 47355 | 3 / 3 | 401 / 388 | 1,50e−7 / 5,72e−6 |

Braços começam no ID 2; cintura tem duas raízes, IDs 0 e 1; acessório de cabeça
usa IDs 2, 3 e 4. A rig base preservada contém
`[0,17,18,19,14,15,16,1,2,3,4,9,10,11,12,5,6,7,8]`.
A maior mudança geométrica causada pelo remapeamento de qualquer peça é
`3,82e−6` unidades fonte na pose de referência. Unidades físicas não foram
estabelecidas.

Como controle negativo real, `f_arm001`, entry 19948, tem IDs coincidentes,
mas o bind diverge. A inspeção interrompe no primeiro desacordo, ID 9,
diferença global 1,0 unidade, e registra `bind_mismatch`. A peça não entra no
glTF. Coincidência de IDs ou nomes, portanto, não basta para aceitação.

`m_face000` (47197, uma junta ID 4, 17 primitivas, 14 materiais) e `m_hair000`
(47245, três juntas, sete primitivas) foram apenas inspecionados. Seus binds
são compatíveis, mas o rosto inclui nomes de materiais `make000` a `make011`,
rosto e olhos; a seleção de aparência e visibilidade não está recuperada.
Não foram sobrepostas todas as variantes. Nessa inspeção de geometria, as
dependências MRL/TEX dessas duas peças não foram resolvidas.

## Extensão restrita dos produtores

`model_scene.parse_actor` aceita tipo 4/stride 28 apenas com
`allow_two_influence_vertices=True`, mantendo stride 36 no contrato padrão.
O perfil de uma junta também exige `allow_single_joint=True`. As verificações
de hierarquia, matrizes, paletas, spans e pesos permanecem obrigatórias.

O registro de 28 bytes contém posição float32×3 em +0, normal snorm8×3 em +12,
UV float32×2 em +16, índices de junta u8×2 em +24 e pesos unorm8×2 em +26.
Stride 36 acrescenta índices em +32 e pesos em +34. A interpretação dos
campos corresponde à referência primária fixada
[MT Framework Mobile Blender Model Script, commit 2bb9c35](https://github.com/xZombieAlix/MT-Framework-Mobile---Blender-Model-Script/blob/2bb9c35be1f0ccfb3899e8317da1b6e6cf011a8e/BlenderMTFrameworkMobile.py),
SHA-256 `e60b0f91c327c9540aee795a8cce0c5e582a999a7d6ec9489de16e07f81ad61f`.
Esse script foi referência de pesquisa, não código executado ou incorporado
ao runtime. Nos vértices aceitos, a soma dos pesos fonte é exatamente 255.
Influências de peso zero viram padding de junta zero no glTF, sem mudar
qualquer influência ativa.

O limite do protótipo é de 16 IDs distintos explícitos, 25 mil vértices por
peça e 100 mil no conjunto. A lista de peças aceitas, rejeitadas e somente
inspecionadas permanece disjunta e completa. A extração verifica o hash do
membro comprimido selecionado contra o índice; não recalcula o hash da ROM
inteira a cada membro. O hash da ROM registrado no índice é
`c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`.

Materiais conservam a associação JAMCRC/comandos MRL. As seis texturas base
são entries 92580, 44778, 42454, 49873, 52637 e 47352. A pele compartilhada
92580 vem do ARC explicitamente informado `/loc/data/pl_base_0.arc`.
RGB é real; alpha permanece separado. O shader é uma prévia neutra opaca,
sem implementação de personalização ou iluminação original.

## Regiões pretas e visibilidade ainda desconhecida

Todos os grupos das cinco peças estão presentes por padrão, inclusive a
primitiva 3 do torso. O diagnóstico nativo isolado em
`research/private/material-diagnostic/` mostrou que essa primitiva produz a
silhueta preta; ao ocultá-la explicitamente, a geometria de pele aparece.
Ela tem 187 vértices, 334 triângulos, material 2 e uma única UV glTF
`(0.038787841796875, 0.716552734375)`. A textura de pele carregada pelo produtor
C++ tem RGB entre 6 e 250 e nenhum pixel zero; ela não explica essa silhueta.
As capturas `only-primitive3.png` e `without-primitive3.png`,
`visibility-report.json` e `scene-image-statistics.txt` preservam o diagnóstico.

Há uma diferença adicional nos bytes do registro de 48 bytes:

| Campo bruto | Torso primitivas 0–2 | Torso primitiva 3 |
| --- | --- | --- |
| u16 em +0 | `0xedd7` | `0xe6c8` |
| bits baixos 0–11 do u32 em +4 | 0 | 100 (`0x064`) |
| material, bits 12–23 do u32 em +4 | 0, 1, 2 | 2 |

As outras dez primitivas aceitas também têm `0xedd7` e zero nesses bits baixos.
O comparativo integral está em
`assets/local/player-assembly-probe/primitive-record-comparison.json`.
São pistas para investigar a seleção original, não prova de que os campos
signifiquem ocultar, sombra ou proxy. A causa visual imediata é a geometria
específica com UV constante/material 2; a condição de uso no jogo continua
desconhecida. Nenhuma primitiva foi removida automaticamente do candidato.

## Validações executadas

- Sete testes específicos: stride 28 opt-in, peso inválido, índice fora da
  paleta, limites do buffer, rig de uma junta, raiz parcial rotacionada,
  IDs duplicados/ausentes, pais incompatíveis e bind incompatível.
- Onze testes anteriores do produtor esquelético passaram. Os seis produtos
  do export de torso anterior foram regenerados byte a byte sem alteração;
  hashes em `body-regression/byte-identical-regression.json`.
- O readback `verify_player_assembly.py` passou 231 verificações: geometria,
  índices, influências, material/TEX, cobertura e exclusões; os canais,
  tempos e valores TRS dos 44 clips são byte-idênticos aos do torso.
- Ambos os glTFs finais passaram no Khronos Validator 2.0.0-dev.3.10 com
  zero erros, avisos, infos ou hints. Saídas anteriores com padding não
  canônico foram preservadas nos diretórios `*-initial-padding`.
- `mh4u-scene-tests` carregou a cena final: erro bind máximo `8,20382e−6`,
  RMS `4,057e−6`, movimento não zero e 44 animações. A validação Python do
  float32 serializado mede erro bind máximo `1,33569e−5`; são percursos
  numéricos diferentes, não medidas contra o jogo.
- O renderer nativo abriu e capturou a cena final em clip 0, tempo 0,5 s,
  saindo com código zero. Captura: `male001-actions/candidate.png`.
  A imagem conserva as regiões pretas acima; não prova fidelidade visual.
- Na janela diagnóstica isolada do macOS, cliques reais no controle de
  primitivas ocultaram/restauraram a primitiva 3 do torso. Trocar para colisão
  e voltar ao torso restaurou todas as primitivas. Cinco capturas e o relatório
  `research/private/input-diagnostic/primitive-ui-validation.json` documentam
  esse teste. O processo próprio saiu com código zero; janelas persistentes
  foram preservadas. Isso não valida interação de UI no Windows.

SHA-256 do glTF estático:
`3d807d3c9d730136b61ec223c7b45e7683fdb08bae3638f046ee9d4a670c06bd`.
SHA-256 do glTF animado:
`eeb859c211be8297389c9099ca29fe59f7f5c1f707cb4c3304c44e783604618a`.
O BIN de geometria é parte indispensável dessa identidade:
`f82aa7a4ebdb90cdfbae388b73ef81a2cc0203eab88ab1e0a037bae6649357ce`.
A revisão `mh4u-lmt67-arm-sampler-v1` regenerou o JSON e o BIN de animação,
preservando esse BIN de geometria. O novo animation.bin tem SHA-256
`c904b4537812aa2e199ce83c74f661e9038147b762b757671166feac5825e5ea`.
Os manifestos conservam todos os hashes das dependências. A correção de
quantização/tempo está em [animation-findings.md](animation-findings.md#shipped-arm-sampler-correction);
44 clips e suas identidades foram mantidos, e os 231 readbacks foram repetidos
contra o torso também corrigido. Exports anteriores estão preservados em
`research/private/animation-corrections/20261017-arm-sampler-v1/before/`.

Reprodução em um diretório de saída novo, a partir da raiz do projeto:

```sh
python3 -B tools/resources/player_assembly.py \
  --index research/private/resources.sqlite --base-entry 44780 \
  --piece-entry 42456 --piece-entry 49875 --piece-entry 52640 \
  --piece-entry 47355 --piece-entry 19948 \
  --inspect-entry 47197 --inspect-entry 47245 \
  --shared-archive /loc/data/pl_base_0.arc --output /caminho/novo/conjunto
python3 -B tools/resources/animation_probe.py export-many \
  --lmt assets/local/player-probe/inventory/source/entry-54913.bin \
  --lmt assets/local/player-probe/inventory/source/entry-92608.bin \
  --model /caminho/novo/conjunto/scene.gltf --fps 60 \
  --output /caminho/novo/animado
build/mh4u-resource-lab --model /caminho/novo/animado/animated.gltf
```

60 FPS continua uma convenção de export; ritmo, loops, transições e estados
originais não foram validados. A análise é de arquivos extraídos, e a
execução de verificação é do motor nativo local, não do executável do 3DS.
