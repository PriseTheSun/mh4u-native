# Recurso SBC e consulta vertical por normal original

`collision_resource` é o dono imutável dos registros SBC: grupos, pares, faces,
vértices, atributos e grades GRCO conhecidas. Preserva os bytes, offsets e SHA256
da fonte. A resolução de pares usa IDs locais do grupo, sem confundir índice de
par com índice de face. A interpretação da aceleração é separadamente falível:
um formato desconhecido não elimina os fatos válidos do núcleo do recurso.

Sete recursos reais passaram na leitura e recomposição, cobrindo 19 grupos,
5.711 pares, 5.834 faces, 15.232 células e 57.629 índices. São 1.504.663 verificações
por perfil normal e ASan/UBSan. Essa cobertura de bytes não prova execução
original das sete grades; a investigação ARM da grade completa usou v00a01-core2.

`spatial_face_query` implementa o trecho 17fec8 para o perfil vertical type15
recuperado. Usa a normal gravada no SBC; não a recalcula pelos vértices. Conserva
a ordem das operações binary32, o teste das arestas no plano XZ e os limites de t.
Um hit com t igual ao limite é aceito, permitindo substituir a seleção anterior.
A decisão de qual identidade guardar permanece com o chamador. A folha não é
equivalente ao trecho cc94f0 documentado em [spatial-triangle](spatial-triangle.md).

O comparador passou em 1.444 chamadas preservadas do ARM: 1.290 sobre o SBC real
e 154 sintéticas, com 910 hits e 534 misses. Comparou 18.772 palavras de saída e
quatro sequências contínuas. A seleção e a flag são projeções do chamador; os
22 bytes opacos do carrier não são interpretados pela matemática. Perfis normal
e ASan/UBSan passaram em 50.396 verificações cada, com mais 58 processos negativos.
O dono numérico possui 93 verificações próprias de limites por perfil.

A leitura SBC e a folha estão no build de desenvolvimento. Ainda falta compor
a travessia completa da grade, filtros, objetos e escolha do hit no runtime do
Player. Máscaras, instâncias e transformações iniciais do gameplay não foram
inferidas dos estados de diagnóstico.

Evidências preservadas:

- `research/private/collision-resource-native-v1/manifest.json`;
- `research/private/spatial-face-query-v1/manifest.json` (original);
- `research/private/spatial-face-query-native-v1/geometry-owner/manifest.json`;
- `research/private/spatial-face-query-native-v1/oracle-adapter/manifest.json`.
