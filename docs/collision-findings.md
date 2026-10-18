# Colisão SBC: núcleo geométrico observado

O primeiro resultado reproduzível é uma malha real de colisão de
`stage\v00\v00a01\collision\v00a01-a-g00`: **48 vértices, 86 triângulos**.
O executável C++ nativo carregou o glTF e renderizou dois frames no Mac M4
(OpenGL 4.1 Metal). A exportação conserva coordenadas, ordem dos vértices e
atributos brutos. O material cinza do glTF é apenas uma escolha de inspeção.
Não representa o material visual original do cenário.

Isso confirma geometria legível, não colisão de gameplay reproduzida. Máscaras,
propriedades físicas, regras de piso/parede/câmera, colocação de atores e unidades
do mundo ainda não estão estabelecidas. Nenhum raycast, movimento ou resposta
de colisão foi implementado neste módulo.

## Evidência e cobertura

O índice completo `research/private/resources.sqlite` contém **626 entradas
`rCollision`**, tipo `0x51fc779f`, entre 1.378 e 337.652 bytes descomprimidos.
`assets/local/collision-probe/inventory.json` enumera todas as entradas com ARC,
nome, tamanho e hash do payload comprimido. Esse inventário não implica que
todos os 626 recursos tenham sido decodificados. Não foram encontradas entradas
com os IDs calculados de `rGeometry`, `rGeometry2`, `rCollisionObj` ou
`rCollisionHeightField` neste índice.

O REA identificou estaticamente o loader de `rCollision` em **0x16014c**:

- Evidence `ev_01511f30730443caddfd86a170c3c2bd77261ffc9c5b2a974172ffbb5c47e2a6`;
- resultado completo local em
  `research/private/rea-session-1792271307255/034-procedure_pseudo_code.json`;
- Ghidra 12.1.4, ARMv6, banco efêmero e fonte imutável;
- ELF de análise SHA-256
  `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.

O pseudocódigo confirma a assinatura de três bytes `SBC`, versão
`0x77cf8df8` (2010091000), header de 0x48 bytes, contagens e ordem das leituras
abaixo. O parser exige também o quarto byte `ff`, observado nas amostras; esse
requisito é mais restrito que a comparação de magic no loader.
Não houve execução do código ARM. A ligação da factory ARM 0xccf330 à vtable
0xe5b568 usou LLVM porque Ghidra não reconheceu a factory como função; a função
de carregamento foi recuperada pelo REA. Os limites dessa identificação devem
acompanhar referências futuras ao loader.

Uma segunda função, **0x16c380**, confirma o endereçamento das faces:
Evidence `ev_f9126f947889e85fb0e7110e96cdfe6b20860f269c228d343d0d166f319ee71e`,
arquivo local `043-procedure_pseudo_code.json` na mesma sessão. Ela calcula
`grupo = *(object+0xbc) + groupID*72`, usa o início de faces em grupo+0x34 e o
início de vértices em grupo+0x3c. O u16 face+0x12 indexa a tabela global de
registros de 32 bytes em object+0xc0. Isso confirma índices de vértices locais
ao grupo e permite validar aquela referência de atributo, mantendo sua
semântica desconhecida.

O projeto público [Albam, schema SBC 211 no commit fixado](https://github.com/Brachi/albam/blob/d1fda41beeae129fbb26128157511afdee4002ea/albam/engines/mtfw/structs/sbc-211.ksy)
corrobora pares, faces e vértices. Ele descreve outros jogos de MT Framework:
**não é documentação específica de MH4U**. Seu header e seus grupos têm oito
bytes adicionais; transplantar aquele parser diretamente deslocaria os dados.
O loader real, os limites do arquivo e as relações geométricas fundamentam os
offsets usados aqui.

## Layout aceito

Valores multibyte são little-endian. As seções são contíguas e **não devem ser
realinhadas**: um número ímpar de pares de dez bytes deixa as faces desalinhadas.

| Header | Tipo | Observação |
| --- | --- | --- |
| 0x00 | bytes[4] | `SBC ff` nas amostras |
| 0x04 | u32 | 2010091000 |
| 0x08, 0x0c | u32 cada | Seletores das árvores global/por grupo; observados 0/2 |
| 0x10, 0x12 | u16 cada | Número de grupos e registros de atributos |
| 0x14, 0x18, 0x1c | i32 no loader | Número de pares, faces e vértices; parser exige spans válidos |
| 0x20, 0x24 | u32 cada | Desconhecidos, preservados |
| 0x28, 0x38 | float4 cada | AABB mínimo/máximo, incluindo componentes w brutos |

Após o header de 72 bytes:

| Seção | Stride | Conteúdo |
| --- | --- | --- |
| Grupos | 72 | AABB float4×2; três u32 zerados pelo loader; início/quantidade de pares, faces, vértices; u32 final |
| Pares | 10 | Duas referências u16 a faces; quatro bytes de ordem candidatos; u16 de atributo bruto |
| Faces | 32 | Normal float3; três índices u16 de vértice; u16 de atributo; u32 desconhecido; três bytes candidatos de adjacência; u8 e u32 desconhecidos |
| Vértices | 16 | float4; xyz exportado sem transformação, w preservado |
| Registros de atributos | 32 | Mantidos integralmente em hexadecimal, sem semântica atribuída |
| Restante | Variável | Árvores espaciais e dados auxiliares preservados em `unparsed-tail.bin` |

No grupo, os inícios/quantidades estão em 0x2c/0x30 (pares), 0x34/0x38
(faces), 0x3c/0x40 (vértices); o valor final em 0x44 permanece bruto. O loader
zera 0x20, 0x24 e 0x28 antes de substituir esses campos por dados de runtime.

O parser exige que os grupos particionem os arrays sem sobreposição ou lacunas,
preserva os índices locais das faces e deriva índices globais somando o início
de vértices do grupo. O u16 de atributo de cada face deve caber na tabela global,
conforme o consumidor REA. Os atributos de pares continuam brutos: não foi
confirmado um consumidor equivalente para eles.

A segunda face de um par pode conter `0xffff`; esse valor é preservado e aceito
como ausência de segunda face, corroborado pelos pares e pela contagem
geométrica nas amostras. A interpretação das referências de pares como locais
ao grupo é derivada do schema público e corroborada por uma amostra real de 13
grupos; não recebeu a mesma confirmação de consumidor REA que as faces.

O loader continua com as árvores, selecionando helpers 0x2e7a38 ou 0xdb1454.
O caso por-grupo 2 também lê dados associados a pares. O parser atual não tenta
reconstruí-los: valida apenas os spans do núcleo, conserva o arquivo original
completo e registra offset/tamanho/SHA-256 do restante. Não se afirma validação
semântica integral do SBC.

## Amostras verificadas

IDs abaixo pertencem ao índice da ROM cujo SHA-256 é
`c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`.
Todos os nomes devem ser resolvidos novamente caso a identidade da ROM mude.

| Entry | Sufixo/nome | Vértices | Faces | Uso da evidência |
| --- | --- | ---: | ---: | --- |
| 93738 | v00a01-a-g00 | 48 | 86 | Exportação principal |
| 93739 | v00a01-a-w00 | 90 | 90 | Pares com duas faces |
| 82813 | m06a00d-a-g00 | 46 | 72 | Quatro IDs de atributo, pares variados |
| 81177 | m04a07d-k00-a-g00 | 37 | 40 | Patch menor; função `k00` desconhecida |
| 83362 | m08a00d-a-cam | 4 | 2 | Retângulo simples; nome sugere câmera, não piso |
| 14530 | m13a12d-a-w00-shell | 1.578 | 2.384 | 13 grupos, endereçamento local confirmado |
| 86059 | m15a08d-a-g00 | 1.554 | 3.160 | Maior SBC do índice, um grupo |

As sete amostras passaram limites dos arrays, referências de faces e pares,
finitude, AABBs e análise de degeneração. Nenhuma face degenerada foi encontrada.
Nas cinco amostras iniciais, o produto escalar entre a normal fonte e a normal
derivada do winding foi ≥ 0,9999999999999993; erro máximo de comprimento da
normal fonte ≤ 1,35e-7. Na amostra de 13 grupos, o produto mínimo foi
0,9999999999999578, sem faces degeneradas nem referências fora dos grupos.
O relatório completo está em `assets/local/collision-probe/sample-validation-core2.json`.
Essas métricas sustentam o layout da geometria; não provam as propriedades
físicas dos atributos.

O recurso principal ocupa 7.960 bytes. O núcleo termina em 4.588; os 3.372
bytes finais são preservados sem interpretação. AABB de xyz:
`[-765.880554, -21.331917, 10530.652344]` a
`[817.959167, -13.055095, 12457.205078]`.
Os valores elevados de z foram mantidos; não se adicionou origem local ou spawn.

O mesmo ARC `/loc/data/v00a01.arc` contém os modelos `stage\v00\v00a01\v00a01`
(entry 93727), `v00a01-a00` (93729) e `v00a01-hikari` (93736). São candidatos
para correlacionar cenário visível e colisão, ainda não decodificados por este
trabalho. A associação resulta do nome/contêiner, não de uma ligação de runtime
verificada.

## Reprodução local

Recrie a inspeção e o índice conforme [resource-formats.md](resource-formats.md)
e as instruções de `index_resources.py`. Este módulo usa somente a biblioteca
padrão de Python e os parsers do projeto; não necessita baixar Albam ou acessar
REA para reproduzir uma exportação.

```sh
python3 -B tools/resources/collision_probe.py inventory --index research/private/resources.sqlite --output assets/local/collision-probe/inventory.json
python3 -B tools/resources/collision_probe.py export --index research/private/resources.sqlite --entry 93738 --output assets/local/collision-probe/v00a01-core2
python3 -B tools/resources/collision_probe.py export --index research/private/resources.sqlite --entry 14530 --output assets/local/collision-probe/m13a12d
python3 -B -m unittest tools/resources/tests/test_collision_probe.py
./build/mh4u-resource-lab --model assets/local/collision-probe/v00a01-core2/scene.gltf --inspect-only
./build/mh4u-resource-lab --model assets/local/collision-probe/v00a01-core2/scene.gltf --frames 2 --screenshot assets/local/collision-probe/v00a01-core2/capture.ppm
```

A extração usa `index_resources.extract`: verifica tamanho da ROM, faixa dentro
do ARC, hash dos bytes comprimidos, estabilidade do arquivo durante a leitura
e descompressão limitada. Não recalcula o hash de toda a ROM a cada recurso;
essa verificação pertence à criação do índice. Arquivos de saída diferentes
nunca são sobrescritos. Após mudar o produtor, exporte para outro diretório
para conservar a proveniência anterior.

`geometry.json` mantém os valores originais; OBJ mantém uma normal por face;
glTF expande os triângulos para preservar essas normais e as renormaliza apenas
para visualização. Não há mudança de eixo, centro, escala ou winding. As
limitações seguem no manifest. Antes de alocar coleções, o parser verifica os
spans e um orçamento conservador de 128 MiB para expansão Python/JSON/OBJ/glTF;
esse orçamento é da ferramenta, não uma propriedade do formato. Há também o
limite compartilhado de 64 MiB por membro descomprimido.

Onze testes sintéticos cobrem desalinhamento, campos desconhecidos, truncamento,
contagens exageradas, orçamento de representação, versões não suportadas,
offsets/overlap de grupos, índices locais e referências globais de atributo,
NaN/AABB, orientação e triângulos degenerados. Os dois glTF reais exportados
passaram `gltf-validator` 2.0.0-dev.3.10 com zero erros, avisos, infos ou hints;
relatórios em `assets/local/collision-probe/{v00a01-core2,m13a12d}/khronos-validation.json`.
A chamada e o pacote de validação estão descritos em
[skeletal-models.md](skeletal-models.md). Captura local verificada visualmente:
`assets/local/collision-probe/v00a01-core2/capture.png`.
O diretório anterior `v00a01/` e `collision_probe-v1.py` permanecem locais como
evidência da primeira revisão; o schema atual é `mh4u-sbc-core-v2`.

## Referência pública e licença

Referência fixada: Albam commit
`d1fda41beeae129fbb26128157511afdee4002ea`; `sbc-211.ksy` SHA-256
`22638e689d1fdd1ab508d13704a6f2ef0a698cfb9314bac11e77ce53dea91bea`.
URLs, hashes e cópias consultadas ficam em
`research/private/resources/collision-references/manifest.json`.
O parser Python é uma implementação local do subconjunto corroborado; Albam
não é uma dependência de runtime. Como atribuição à referência de formato,
segue o aviso MIT do [repositório fixado](https://github.com/Brachi/albam/blob/d1fda41beeae129fbb26128157511afdee4002ea/LICENSE):

```text
MIT License

Copyright (c) 2025 Sebastian C. Aguirre Brachi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Esse aviso cobre a referência pública, não concede direitos sobre os recursos
do jogo. Todos os SBC, derivados e capturas permanecem em `assets/local/`, e a
ROM não foi alterada nem transmitida.
