# Consulta geométrica nativa

`mh4u_collision` fornece `raycast` e `segmentcast` sobre uma cena estática.
O código C++ usa os triângulos glTF exportados do SBC, aplica os transforms
de nós uma vez pelo avaliador existente e constrói uma BVH própria. Ele não
executa código 3DS nem usa as árvores SBC ainda desconhecidas.

O primeiro recurso real validado é
`assets/local/collision-probe/v00a01-core2/scene.gltf`: 86 triângulos, sem
degenerados, BVH com 31 nós. Consultar o centro de cada face a partir de dez
unidades acima recuperou todos os 86 IDs originais. Erro máximo observado na
distância: **1,1724e-13 unidades da fonte**. Essa validação estabelece consultas
sobre a geometria; não estabelece piso caminhável, inclinação permitida,
máscaras, altura de caçador ou resposta física original.

## API e identidade

[`src/collision.hpp`](../src/collision.hpp) expõe:

```cpp
auto world = mh4u::collision::load_static_gltf(path);
auto result = world.raycast(origin, direction, max_distance);
auto segment = world.segmentcast(start, end, mh4u::collision::Backfaces::cull);
```

`World` possui a geometria imutável e sua aceleração; chamadas de consulta não
recarregam arquivos, não avaliam transforms novamente nem alocam coleções.
`load_static_gltf` reutiliza `load_gltf` e `evaluate_scene`. Cenas contendo
animações ou skins são recusadas; não são congeladas silenciosamente.

O resultado contém `optional<Hit>` e contadores de AABBs/triângulos examinados.
`Hit` preserva um `triangle_id` correspondente ao ordinal antes da ordenação
da BVH, além de instância, nó, primitiva e triângulo dentro da primitiva.
Esses IDs pertencem à representação do proprietário `Scene`; a ordem de nós
no arquivo não deve ser confundida com a ordem de instâncias do avaliador.
Instâncias diferentes da mesma primitiva recebem IDs globais distintos.
No glTF de colisão exportado por `collision_probe.py`, a única primitiva
mantém a ordem do array de faces SBC: o `triangle_id` coincide com o índice
da face em `geometry.json`.

A normal retornada é geométrica, calculada pelo winding dos vértices depois
dos transforms. Não é uma normal interpolada de iluminação. Escala negativa
altera o winding e, consequentemente, o lado considerado frontal. Atributos
SBC desconhecidos não entram na consulta, nem recebem valores físicos.

## Políticas de consulta

| Caso | Política explícita |
| --- | --- |
| Direção de raio | Finita e não nula; normalizada internamente, incluindo magnitudes muito pequenas/grandes |
| Distância máxima | Finita e não negativa, nas unidades da cena; o intervalo calculado é fechado `[0,max]` |
| Segmento | Dois pontos finitos e distintos; distância máxima é o comprimento do segmento |
| Ponto inicial sobre a face | Pode retornar distância zero se a direção não for paralela |
| Raio coplanar ou paralelo | Sem interseção isolada, portanto `miss` |
| Faces traseiras | Incluídas por padrão; `cull` as exclui; a normal nunca é invertida para acompanhar o raio |
| Empates | Distâncias calculadas exatamente iguais escolhem o menor ID original; valores próximos não são agrupados |
| Triângulo degenerado | Área calculada exatamente zero: contado e excluído da BVH, sem renumerar os demais |
| Mundo vazio ou somente degenerados | Consulta válida retorna `miss` |
| NaN, infinito, direção zero, segmento zero ou orçamento excedido | Erro explícito, não `miss` |

Interseções usam aritmética `double` sobre posições produzidas em `float` pelo
avaliador. O teste de paralelismo usa
`64*epsilon(double)*length(edge1)*length(edge2)`; barycentrics aceitam erro
de borda de `64*epsilon(double)`. Barycentrics calculadas são retornadas sem
clamp e podem conter negativos dessa ordem junto a uma borda. Os limites de
distância usam comparação direta, sem aumentar o alcance solicitado. Divisões
do teste de AABB são arredondadas um ULP para fora. Isso não constitui um
predicado geométrico exato ou garantia de interseção para todo caso mal
condicionado; valores intermediários não finitos produzem erro.

## Aceleração e limites

A BVH divide pela mediana do eixo de maior extensão dos centroides e mantém
até oito triângulos por folha. O desempate de centroides usa ID original.
Construção tem profundidade logarítmica; travessia usa pilha fixa dimensionada
pela largura de `size_t`. A distância do melhor hit limita a travessia, mas
não exclui candidatos à mesma distância.

Antes de reservar vetores, `World` verifica um orçamento padrão de 256 MiB
para triângulos, geometria auxiliar, ordem e um limite superior de dois nós
por triângulo. `estimated_storage_bytes` é uma estimativa conservadora desse
armazenamento, não memória máxima medida do processo. O parser/avaliador glTF
mantém seus próprios limites de arquivo, vértices, texturas e expansão.
O construtor recebe um vetor já existente; o orçamento não abrange memória
que o chamador alocou antes de invocá-lo nem toda a fase de importação.

Nas 86 consultas reais, a BVH examinou **1.272 triângulos**, frente a 7.396
candidatos de uma varredura completa. Na fixture de 128 triângulos separados,
128 consultas examinaram 1.024 candidatos, frente a 16.384. Essas contagens
confirmam redução de trabalho; não são uma medição comparativa de velocidade.

Um processo local com cache aquecido, incluindo fixtures, importação, BVH e as
86 consultas reais, levou 0,00926 s de parede, 0,00346 s de CPU de usuário e
0,00296 s de sistema; pico RSS reportado pelo macOS de 9.306.112 bytes.
Medição única em Mac M4, preservada em
`assets/local/collision-probe/v00a01-core2/runtime-tests.json`.
Não foram medidos benchmark isolado do kernel, execução fria, desempenho
comparativo anterior nem paridade de ponto flutuante entre plataformas.

## CLI de diagnóstico

```sh
./build/mh4u-collision-query \
  --scene assets/local/collision-probe/v00a01-core2/scene.gltf \
  --origin 430.1338755289714 -8.464843114217121 12205.611328125 \
  --direction 0 -1 0 --max-distance 20

./build/mh4u-collision-query \
  --scene assets/local/collision-probe/v00a01-core2/scene.gltf \
  --origin 430.1338755289714 -8.464843114217121 12205.611328125 \
  --end 430.1338755289714 -28.46484311421712 12205.611328125 \
  --backfaces cull
```

A primeira consulta retorna face 0, distância 10 e posição y=-18,4648431142.
JSON completo vai para stdout. `hit:null` é um resultado válido com código 0;
entrada inválida produz JSON com `error` em stderr e código 1. Opções repetidas,
combinações de raio/segmento e políticas desconhecidas são recusadas.

Targets CMake: `mh4u_collision`, `mh4u-collision-query` e
`mh4u-collision-tests`. Use o CMake local já instalado pelo projeto ou
`tools/build.py` conforme as instruções existentes. Os targets compilaram no
Mac arm64 e em cross-build Windows x64 com LLVM-MinGW; **Windows não foi
executado** nesta validação.

## Verificação executada

```sh
./.tools/cmake-4.4.4-macos-universal/CMake.app/Contents/bin/ctest --test-dir build -R collision --output-on-failure
python3 -B tools/collision_smoke.py \
  --binary build/mh4u-collision-query \
  --scene assets/local/collision-probe/v00a01-core2/scene.gltf \
  --geometry assets/local/collision-probe/v00a01-core2/geometry.json
```

Testes analíticos cobrem distância/barycentrics, raio escalado, ponto inicial,
bordas/vértices, endpoint, paralelo/coplanar, faces traseiras, empates, NaN,
infinito, zeros, degenerados, orçamento, BVH e preservação de IDs. Uma fixture
glTF verifica transform hierárquico, escala não uniforme, reflexão e duas
instâncias da mesma primitiva. O teste real consulta todos os centroides do
recurso congelado. O smoke de processo verificou 12 casos, incluindo rejeição
de cena animada e uma consulta derivada independentemente do JSON SBC; relatório
local em `runtime-smoke.json` ao lado da cena.

O kernel permanece separado do render/controller e não implementa sweep de
cápsula, contatos, gravidade, navegação, masks, dano ou gameplay. A interpretação
dos atributos e a resposta de colisão exigem investigação própria.
