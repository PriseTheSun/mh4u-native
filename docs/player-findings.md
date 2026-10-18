# Recursos do caçador: associação, rig e primeira peça animada

Há evidência suficiente para associar uma rig de 19 juntas do corpo do player
às animações comuns, sem montar um conjunto de equipamento por suposição.
`assets/local/player-probe/body001-actions/animated.gltf` contém **uma peça real
do corpo**, quatro primitivas, três materiais, duas texturas, 1.280 vértices,
1.506 triângulos e 44 slots de animação completos. Esses slots representam
**34 conjuntos numéricos distintos de canais**, pois dez aparecem nos dois
arquivos de movimentos examinados.

Isso não é ainda um caçador completo ou controlável. Cabeça, rosto, braços,
pernas, cintura, equipamento e suas regras de montagem não foram adicionados.
As ações têm nomes técnicos; nenhum slot foi denominado idle, walk ou run.

## Associação demonstrada pelos arquivos

O inventário completo encontrou 24.220 membros sob `player\`: 5.455 modelos,
5.461 materiais, 12.291 texturas, 39 listas de movimentos e 974 outros recursos.
São entradas de arquivo, incluindo variantes e duplicatas; não significam
5.455 personagens. O catálogo integral está em
`assets/local/player-probe/inventory/player-resource-catalog.json`.

Foram extraídos e inspecionados 11 modelos, selecionando peças `pl001`, rosto 000
e cabelo 000. Dois corpos, masculino e feminino, têm exatamente o mesmo conjunto
de 19 IDs encontrado nos canais de movimentos comuns, excluindo o ID de cena 255:

`[0,17,18,19,14,15,16,1,2,3,4,9,10,11,12,5,6,7,8]`.

| Modelo | ID do índice | Juntas / primitivas / materiais | Layout |
| --- | ---: | --- | --- |
| `player\mod\m\pl001\m_body001` | 44780 | 19 / 4 / 3 | tipo 4, stride 36 |
| `player\mod\f\pl001\f_body001` | 22250 | 19 / 4 / 3 | tipo 4, stride 36 |
| `m_arm001` / `f_arm001` | 42456 / 19948 | 9 / 2 / 2 | tipo 4, stride 28 |
| `m_leg001` | 49875 | 7 / 2 / 2 | tipo 4, stride 36 |
| `f_leg001` | 27339 | 7 / 3 / 3 | tipo 4, stride 36 |
| `m_wst001` / `f_wst001` | 52640 / 30082 | 6 / 3 / 3 | tipo 4, stride 28 |
| `m_face000` / `f_face000` | 47197 / 24655 | 1 / 17 ou 18 / 14 ou 15 | tipo 4, stride 28 |
| `m_hair000` | 47245 | 3 / 7 / 1 | tipo 4, stride 28 |

Os arquivos realmente contêm peças e rigs parciais. Por exemplo, os braços
começam no ID 2, enquanto as pernas usam os IDs 0,17,18,19,14,15,16. As cinturas
inspecionadas têm duas raízes. Rosto de uma junta não passa no atual parser
esquelético, cujo contrato exige pelo menos duas. Um mesmo prefixo `pl001`
não determina sozinho visibilidade, substituição de pele, reparenting ou
equipamentos que o jogo montaria; nenhuma dessas regras foi inferida.

As duas listas escolhidas são explicitamente nomeadas no namespace do player:

| Movimento | ARC | ID | Slots populados / slots totais |
| --- | --- | ---: | --- |
| `player\mot\co\lco_0` | `/data/pl_mot_lco.arc` | 54913 | 19 / 68 |
| `player\mot\co\co_0` | `/loc/data/pl_base_0.arc` | 92608 | 103 / 136 |

Cada ação examinada usa os 19 IDs de rig e a cena 255; três ações do segundo
arquivo acrescentam um canal de rotação absoluta. A coincidência exata dos IDs,
suas ocorrências em caminhos `player\mod`/`player\mot` e os esqueletos coerentes
corroboram a associação. A abreviação `lco` foi tratada somente como nome fonte,
não como prova de que todos seus movimentos sejam de locomoção.

## Export da peça e material

O MOD escolhido, entry 44780, veio de `/data/pl_m_body001.arc`; SHA-256
`f63bc643e17f05156f77734e0797ee2e3f135f118fe29bed34e5112a339178a3`.
`body001/scene.gltf` tem SHA-256
`a4377f81188ed7f7bf8ec4e7adb6bdcd1c5847620a7a35cc0c84f026d13e2eb4`.
O glTF animado tem SHA-256
`f299e6fbee88ce5c3e5693b7378d67ec33cccb40c108bb85a56768984b271147`.

`tools/resources/player_probe.py` coordena a seleção, sem duplicar os codecs
ou o layout de pesos. O produtor `model_scene.parse_actor` recebeu uma opção
explícita `allow_multiple_materials`; o comportamento padrão permanece restrito
a um material. `make_gltf` aceita um pacote completo de materiais para essa
opção. Posições, hierarquia, TRS, paletas e inverse binds seguem a conversão
validada em `docs/skeletal-models.md`. Todos os quatro grupos de geometria do
MOD são conservados, sem atribuir semântica de LOD ou visibilidade.

A ligação por JAMCRC 32/comandos MRL de `stage_scene.py` identifica:

- `XfB__m00_m_body001_skin_` → `player\mod\common\m_skin_BM`, ETC 1,64× 64.
- `XfB__m00_m_body001_sym_` e `XfB__m00_m_body001_` →
  `player\mod\m\pl001\m_body001_BM`, ETC 1A 4,128× 128.

A textura comum é fornecida pelo ARC explicitamente selecionado
`/loc/data/pl_base_0.arc`, entry 92580. O nome é referido pelo MRL; não foi
escolhida por proximidade no índice. Existe outra cópia no `pl_base_1.arc`,
entry 92745; ambas foram verificadas com SHA-256
`d44a3044c57cfafb75191478716ad277aa8b63f65dd300cebc70094d23b3bc61`.
O exportador não escolhe arbitrariamente entre recursos globais duplicados.

RGB usa pixels reais; alpha fica em PGM. A prévia opaca não reproduz tintura,
personalização de pele, iluminação, sampler ou shader do jogo.

A validação visual posterior no renderer nativo abriu os 44 clips e mostrou
textura dourada no torso/cinto, com grandes regiões pretas na geometria de
cabeça, braços e pernas. Capturas locais:
`research/private/scene-validation/player-native.png` e
`research/private/input-diagnostic/catalog-player.png`. A ligação MRL acima
comprova a referência à textura `m_skin_BM`; não comprova qual regra de cor,
visibilidade, composição ou shader explica essas regiões. Essa aparência segue
como limitação observada da prévia, sem correção de cor por hipótese. A peça
continua rotulada como torso parcial, não como caçador visualmente completo.

## Codecs e ações

O produtor `animation_probe.py` passou a interpretar codecs 12/13 como quaternion
YW 14/ZW 14, com dois campos unsigned 14, delta 4 e scale/bias. A interpretação vem
da mesma referência primária já fixada, RevilLib commit
`e17f9718ea571f040a7e1f331c28a4d45a6066b5`:
[seleção dos codecs](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/bone_track.cpp),
[equações YW/ZW](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/codecs.cpp)
e [definições dos campos](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/codecs.hpp).
Os novos eixos usam a mesma equação dos XW 14, sem remendar tracks após o parsing.

| Codec | Tracks em lco_0 | Tracks em co_0 |
| --- | ---: | ---: |
| 1 | 10 | 46 |
| 2 | 2 | 51 |
| 4 | 10 | 103 |
| 5 | 18 | 57 |
| 6 | 76 | 307 |
| 7 | 202 | 1.193 |
| 11 | 40 | 204 |
| 12 | 40 | 203 |
| 13 | 1 | 2 |

Foram decodificados 2.565 tracks e 63.299 keys. Não restou codec desconhecido
nesses dois arquivos; não se extrapola essa cobertura aos demais movimentos.
Normas brutas dos quaternions variam de 0,9658294567 a 1,0013103491. A conversão
glTF normaliza os quaternions e usa interpolação esférica; não prova a precisão
da compressão em relação às poses originais sem compressão ou ao runtime.

O export foi conservador: dez slots de `lco_0` e 34 de `co_0` são completos e têm
raiz de cena exatamente zero. Cada clip exportado tem 20 canais: 19 rotações
locais e uma translação local no ID 0. Os 78 slots restantes são rejeitados
individualmente como `unsupported_scene_motion`, preservados na proveniência
e no JSON decodificado. A política de raiz, flags de eventos e montagem do
controller não foi presumida nem removida silenciosamente.

Os dez slots aceitos em `lco_0` são 1,21,51,54,60,61,63,65,66,67. Os seus canais
numéricos são idênticos aos slots correspondentes de `co_0`; continuam como
entradas separadas para preservar origem. O seletor tem 44 entradas e 34 conjuntos
numéricos distintos, não 44 comportamentos originais identificados.

60 FPS é apenas convenção explícita de export. Frames declarados variam de 7 a 275
no corpus; os 44 clips aceitos têm domínio de 0,1 a 4,4 segundos após a correção ARM do sampler.
O loop do viewer pela duração das keys não implementa os loop frames positivos
da fonte. O domínio termina em frame_count − 1; o export amostra a cauda bruta ou
mantém o valor terminal nesse endpoint, conforme o sampler observado. Frames,
keys brutas, deltas e loops permanecem na proveniência. A revisão
`mh4u-lmt67-arm-sampler-v1` também corrige a quantização de vetores e remove
a referência sintética dos codecs com buffer; detalhes em
[animation-findings.md](animation-findings.md#shipped-arm-sampler-correction).

## Verificação e próximos limites

Cinco testes sintéticos novos verificam valores analíticos YW/ZW 14, bias nos
componentes não armazenados, acúmulo de delta, rejeição de payload inválido e
contrato multimaterial explícito. Os 11 testes de skeleton anteriores passaram.
As 120 verificações do corpus `em062` também passaram; glTF e BIN antigos foram
regenerados em memória e comparados byte a byte, sem alteração naquele incremento.
A correção posterior baseada no consumidor ARM regenerou as animações em062
e player; hashes e métricas desta página correspondem à revisão corrigida.

O readback `assets/local/player-probe/verify_player.py` passou 53 verificações,
cobrindo canais, IDs, horários, normalização, limites, cobertura disjunta de
122 slots, preservação da rig/material e igualdade da geometria copiada.

| Medida | Resultado |
| --- | ---: |
| Maior deslocamento na pose bind, após float 32 | 1,335685e−5 unidades fonte |
| RMS do deslocamento bind | 3,278344e−6 unidades fonte |
| Maior erro de soma de pesos | 4,470348e−8 |
| Serialização dos canais: posição | 6,335880e−6 unidades fonte |
| Serialização dos canais: rotação | 4,829673e−6 graus |
| Serialização dos canais: tempo | 1,907349e−7 segundos |

Erros de serialização comparam keys decodificadas/normalizadas em double com
accessors float 32; não são erros medidos contra o jogo. Ambos glTFs passaram
no Khronos Validator 2.0.0-dev.3.10 com 0 erros, 0 avisos, 0 infos e 0 hints.

Para avançar ao personagem controlável completo, faltam regras de composição
das peças, layouts skin de 28 bytes, rosto de uma junta, regras de materiais e
personalização, nomes/estados das ações, transições, root motion e sua relação
com deslocamento/colisão. Os dados atuais não autorizam chamar `em062` de
player nem transformar esta peça parcial em um caçador completo por rótulo.

Não houve execução da ROM, instalação adicional ou conexão de jogo nesta etapa.

## Reproduzir

```sh
python3 -B tools/resources/player_probe.py inventory \
  --index research/private/resources.sqlite --model-entry 44780 --model-entry 22250 \
  --motion-entry 54913 --motion-entry 92608 --output assets/local/player-probe/selected-repeat
python3 -B tools/resources/player_probe.py export-body \
  --index research/private/resources.sqlite --entry 44780 \
  --shared-archive /loc/data/pl_base_0.arc --output assets/local/player-probe/body001
python3 -B tools/resources/animation_probe.py export-many \
  --lmt assets/local/player-probe/inventory/source/entry-54913.bin \
  --lmt assets/local/player-probe/inventory/source/entry-92608.bin \
  --model assets/local/player-probe/body001/scene.gltf --fps 60 \
  --output assets/local/player-probe/body001-actions
python3 -B -m unittest discover -s tools/resources/tests -p 'test_player*.py' -v
python3 -B assets/local/player-probe/verify_player.py
```

O inventário completo executado usa os 11 IDs de modelos da tabela acima; o
primeiro comando reproduz deliberadamente a seleção mínima dos dois corpos.
As saídas completas permanecem em diretórios ignorados, com hashes de extração,
limites de origem, arquivos e produtores retidos. Os parsers recusam semânticas
fora da cobertura declarada em vez de criar pose, equipamento ou ação faltante.
