# Primeiro cenário estático real: v00a01

`tools/resources/stage_scene.py` exporta o modelo `stage\v00\v00a01\v00a01`
do ARC `/loc/data/v00a01.arc`, usando os parsers existentes sem modificá-los.
O resultado em `assets/local/stage-probe/v00a01/scene.gltf` contém **23
primitivas, seis materiais, cinco texturas reais, 6.778 vértices e 5.564
triângulos**. Todos os componentes estão em coordenadas originais, sem ajuste
para encaixar a colisão. Não foi executado código do jogo.

Este é o modelo principal selecionado, não uma reconstrução completa de toda a
área. Iluminação, visibilidade de primitivas, shader, colocação de objetos,
câmera, spawn e comportamento de gameplay continuam desconhecidos.

## Identidade e seleção

A extração usa `research/private/resources.sqlite`, verifica novamente o hash
do membro comprimido e seus limites na ROM, e herda a identidade integral já
validada pelo índice. ROM EU `CTR-P-BFGP`, título `0004000000126100`, SHA-256
`c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`.

| Recurso | ID do índice | Resultado |
| --- | ---: | --- |
| `v00a01` | 93727 | Modelo principal exportado, 23 primitivas / seis materiais |
| `v00a01-a00` | 93729 | Exportado separadamente, uma primitiva / um material / 166 vértices / 98 triângulos |
| `v00a01-hikari` | 93736 | Geometria inspecionada: quatro primitivas / três materiais / 263 vértices / 338 triângulos. Export rejeitado: textura de formato 16 não suportada |

O recurso `-a00` tem coordenadas próximas à origem, enquanto o principal está
na região Z≈10.471–12.313. Sua transformação de colocação não foi recuperada;
portanto ele não foi anexado ao cenário principal. O nome `hikari` não foi
usado para inventar uma fonte de luz. Sua textura `v00a01_Fclo00_GSM` é TEX
formato 16, fora do decoder ETC1/ETC1A4 disponível. Nenhum glTF parcial foi
emitido para esse recurso.

O MOD principal tem SHA-256
`3c719423a666d66b4a6b3ab5301b01d6911737369c06dfff3a1b5160ac00f52d`;
o glTF produzido tem SHA-256
`10e28b4d0cdcdbcc383b842bcbc0879213f30cf1d5fb3ac4e9342a7c4eecbdee`.
O glTF de `-a00` tem SHA-256
`ee1c73235bdb9bfdc60a78272cb674c1ba14995205e2a1f9980771e35b05bbc2`.

Cada diretório exportado contém BIN, PNG, alpha PGM, os membros originais e
`provenance.json`. Offsets absolutos/relativos, hashes, registros de primitivas,
materiais, comandos e campos desconhecidos são preservados. O catálogo dos três
modelos está em `assets/local/stage-probe/selection.json`. Dados proprietários
e derivados permanecem em diretórios locais ignorados.

## Geometria e materiais

Todos os modelos examinados são MOD230 sem juntas, vértices tipo4/stride28.
O parser existente lê posição float32×3 em `+0`, normal i8×3 em `+12`, UV
float32×2 em `+16`, e expande os triangle strips com reinícios. O exportador
conserva as posições XYZ; renormaliza normais; converte UV para `(u,1-v)`.
Não aplica a transformação Q dos modelos esqueléticos nem troca eixos.
Registros de cor/tangente e outros campos não ganham semântica presumida.

A ligação MRL32 foi generalizada neste módulo próprio. A tabela tem recursos
de 76 bytes e materiais de 60 bytes. O material MOD é associado pelo JAMCRC32
de seu nome ao campo `+4` do registro MRL. Isso importa porque as tabelas não
têm a mesma ordem. Cada ligação exige exatamente um comando de tipo 3
(`controle & 15`) com hash de shader `0xcd06f` (`shaderID >> 12`, tAlbedoMap).
O segundo word contém o índice de recurso começando em 1; zero é reservado e
rejeitado nesta prévia. Os índices de shader em ambos os words devem concordar.
Bits de preenchimento não são presumidos zero: os bytes observados variam.

| Material MOD | Registro MRL | Textura associada | Dimensão / formato |
| --- | ---: | --- | --- |
| `XfBAW_1__m00_135` | 5 | `v00a01_komono02_NUKI` | 256×256 / ETC1A4 |
| `XfB_W__m00_236` | 4 | `v00a01_komono01_BM` | 256×256 / ETC1 |
| `XfB_W_0__m00_64` | 1 | `v00a01_ita00_BM` | 128×64 / ETC1 |
| `XfB_W_0__m00_61` | 0 | `v00a01_komono02_NUKI` | 256×256 / ETC1A4 |
| `XfB_W_0__m00_62` | 2 | `v00a01_box_BM` | 64×64 / ETC1 |
| `XfB_W_0__m00_` | 3 | `v00a01_npc01_BM` | 128×128 / ETC1 |

PNG contém RGB do primeiro mipmap real. O alpha original é preservado à parte
em PGM; a prévia usa material opaco, metallic 0, roughness 1 e repetição de UV.
Essas são escolhas explícitas de visualização. Especialmente em `NUKI`, que
possui valores alpha 0,17,255, a aparência opaca não reproduz uma provável
regra de recorte ou mistura do original; tal regra não foi recuperada.

A interpretação do MRL combina bytes observados e fontes primárias do Albam,
commit fixado `d1fda41beeae129fbb26128157511afdee4002ea`:

- [mrl.ksy](https://github.com/Brachi/albam/blob/d1fda41beeae129fbb26128157511afdee4002ea/albam/engines/mtfw/structs/mrl.ksy): estruturas, nibble de comando, hash de tAlbedoMap.
- [material.py](https://github.com/Brachi/albam/blob/d1fda41beeae129fbb26128157511afdee4002ea/albam/engines/mtfw/material.py): ligação por JAMCRC e índice de textura com base 1.
- [Notas primárias MH4U rMaterial](https://github.com/svanheulen/mhff/wiki/MH4U-rMaterial-Format): corroboram os offsets das tabelas; não bastam isoladamente para identificar o shader.

Os dois arquivos fixados estão em `assets/local/stage-probe/references/`, com
SHA-256 em `manifest.json`, e não foram importados nem executados. Albam cobre
outros jogos MT Framework; sua aplicação aqui é corroborada pela correspondência
única dos seis nomes, comandos e recursos reais, não presumida pelo nome do motor.

## Comparação espacial com a colisão

`collision-comparison.json` compara os 48 vértices do SBC já decodificado
`v00a01-a-g00` com todos os 5.564 triângulos visuais. Usa distância de ponto à
superfície triangular, em coordenadas idênticas às fontes. Nenhum deslocamento,
escala, troca de eixo ou ajuste otimizado foi aplicado.

| Eixo | Modelo principal mínimo / máximo | Colisão mínimo / máximo |
| --- | --- | --- |
| X | −532,259827 / 514,127625 | −765,880554 / 817,959167 |
| Y | −34,587463 / 572,109924 | −21,331917 / −13,055095 |
| Z | 10.471,360352 / 12.313,034180 | 10.530,652344 / 12.457,205078 |

Distância mínima 0, mediana 1,019440 e máxima 499,369302 unidades da fonte.
**16/48** pontos ficam a até 0,001; **23/48** a até 1; **39/48** a até 10 unidades.
Há correspondência espacial concreta em parte da geometria, mas a colisão se
estende além do modelo principal e não corresponde à mesma superfície completa.
Isso não estabelece walkability, uma transformação global de toda a área ou
unidades em metros. Nenhum ator foi posicionado a partir dessa comparação.

## Validação executada

- Onze testes sintéticos em `tools/resources/tests/test_stage_scene.py`: identidade
  de coordenadas, UV, vínculo por hash com ordens diferentes, índice com base 1,
  bits de preenchimento, ponteiros/contagens inválidos, shaders desconhecidos,
  recursos incorretos, skeleton/layout incompatíveis, normal zero e distâncias
  analíticas a interior, aresta, vértice e triângulo degenerado.
- `assets/local/stage-probe/verify_stage.py` relê accessors float32 e compara
  diretamente com os bytes MOD. Posições e UV convertidas têm erro 0 em ambos
  os exports. O maior erro de norma unitária é 4,022399e−8. Contagens, índices,
  materiais e ausência de transformação de colocação são conferidos.
- O mesmo readback encontrou seis faces opostas à média das normais de seus
  vértices e uma face geometricamente degenerada no principal. Foram preservadas
  sem inverter ou reparar a fonte; sua causa e visibilidade original são
  desconhecidas. O recurso `-a00` não apresenta esses casos.
- Khronos glTF Validator **2.0.0-dev.3.10**: **0 erros, 0 avisos, 0 infos,
  0 hints**, para os dois glTFs e suas dependências locais. Relatórios nos
  diretórios respectivos; versão e origem do validador em `docs/skeletal-models.md`.

As verificações estabelecem consistência do formato exportado, não fidelidade
de shader nem equivalência visual com frames originais. Não houve instalação,
execução da ROM, conexão com serviços ou reabertura da análise REA nesta etapa.

## Reproduzir

```sh
python3 -B tools/resources/stage_scene.py --index research/private/resources.sqlite \
  --entry 93727 --output assets/local/stage-probe/v00a01 \
  --collision assets/local/collision-probe/v00a01/geometry.json
python3 -B tools/resources/stage_scene.py --index research/private/resources.sqlite \
  --entry 93729 --output assets/local/stage-probe/v00a01-a00
python3 -B -m unittest discover -s tools/resources/tests -p test_stage_scene.py -v
python3 -B assets/local/stage-probe/verify_stage.py
```

O export é idempotente para arquivos iguais e recusa substituir arquivos
diferentes. Cada membro é limitado a 64 MiB; o módulo estático limita a 4.096
primitivas, 1.000.000 vértices, 6.000.000 índices, 65.536 comandos de material e 5.000.000 comparações ponto-triângulo. São
limites explícitos desta ferramenta, não limites atribuídos ao formato do jogo.

As verificações de contagens acumuladas ocorrem antes da expansão dos buffers. A proveniência anterior ao reforço desses limites está preservada como `provenance.before-bounds-hardening.json`; os arquivos glTF/BIN/PNG permaneceram idênticos.
