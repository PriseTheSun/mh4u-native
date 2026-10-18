# Consulta espacial nativa sobre SBC/GRCO

O motor possui uma composição C++ da consulta vertical `1774e8/180c5c` e da
função de altura `2bb178`. Ela usa os objetos ordenados do contexto, os filtros
originais, a grade GRCO, a normal armazenada na face e a seleção do contato.
É um componente de desenvolvimento. A [aquisição sem cache do Player](player-spatial-acquisition.md)
foi integrada na etapa seguinte; o carregamento completo do cenário e o gameplay
ainda estão pendentes.

A comparação cobre 312 chamadas completas do código ARM original: 223 retidas,
46 variações de filtros/classificação, 9 consultas de contadores persistentes
e 34 casos de repetição, descarte e altura. O resultado é 176 contatos e 136
ausências de contato, com 2.562 candidatos ordenados, 468 chamadas de face e
191 atualizações de grade. Separadamente, a grade nativa foi comparada em
565 consultas e 340 palavras de preparação. O ARM roda somente no ensaio de
referência; esse componente C++ não contém emulador de 3DS.

O comparador passou 237.463 verificações por perfil normal e com
ASan/UBSan/float-cast-overflow, além de 88 processos negativos. Os 20 casos
de erro tipado incluem falhas depois de encontrar um contato e a reutilização
indevida de estado entre recursos; quatro controles verificam campos que
permanecem desconhecidos em caminhos descartados. A grade passou 12.154
verificações por perfil e 36 processos negativos.

A integração passou nos 110 testes CTest do Mac em 56,33 s. O build completo
incremental, incluindo os testes, levou 112,55 s; a compilação cruzada para
Windows levou 45,90 s. Os builds rodaram em paralelo e não emitiram avisos do
compilador. Não houve execução no Windows. CPU de processos filhos foi
registrada nos relatórios; o pico de memória da família de processos não foi
medido. O launcher publicado permaneceu aberto e não foi substituído.

`src/collision_resource.cpp` continua sendo o único leitor dos fatos SBC.
`src/spatial_grid.cpp` prepara a grade e enumera seus pares na ordem original.
`src/spatial_query.cpp` compõe os filtros e chama o proprietário numérico
`src/spatial_face_query.cpp`. A divisão binária32 passou para
`motion_transform_math`, compartilhada pela grade e pela interseção.

A consulta preserva etapas diferentes: o nó do gerenciador testa somente
minX e maxZ; o grupo testa sua caixa completa segundo a direção vertical;
a grade escolhe uma célula por XZ; depois cada par pode testar sua própria
caixa. A conversão das coordenadas usa truncamento e extensão de sinal de
16 bits, sem arredondar para baixo nem prender a posição à borda da grade.
Contatos com a mesma distância substituem o anterior na ordem de visita.

O estado de visita pertence a `GridState`, separado dos bytes imutáveis SBC.
Cada entrada na grade incrementa o contador de 32 bits, inclusive quando a
consulta não encontra uma célula ou os atributos rejeitam todos os pares.
O contador pode voltar a zero sem limpar as marcas. A sequência é comparar
marca, testar atributo, gravar marca, testar caixa e visitar face. Marcar um
par não prova que houve contato nem que a função de face foi executada.

Uma consulta devolve suas alterações de estado somente depois de completar
todos os estágios admitidos. O chamador mantém o contexto original em caso
de erro. Instâncias que compartilham uma grade referenciam o mesmo slot de
estado; toda reutilização confere novamente recurso, grupo e quantidade de
marcas. O contato mantém a propriedade do recurso e usa identidades locais,
sem expor endereços ARM como identificadores nativos.

Valores ausentes continuam desconhecidos. Os testes fornecem estados,
máscaras, seletores e coleções explicitamente; nenhum deles estabelece o
bootstrap original. O carrier ARM contém 22 bytes copiados sem inicialização,
além de áreas não escritas. A API retorna os campos matemáticos e as
identidades conhecidas; não transforma esses bytes em saída nativa fictícia.

O perfil implementado admite a família direta de instâncias, um grupo SBC,
GRCO comprimido kind1, pares de uma face, seletores de status zero, byte20
zero, ponteiros de status nulos e callbacks opcionais ausentes. A semântica
geral de byte20 não foi atribuída. Famílias alternativas não vazias, árvores,
grupos múltiplos, pares duplos, outros seletores, estados aninhados e callbacks
desconhecidos falham explicitamente. Coleções alternativas desabilitadas ou
vazias não exigem recursos dos ramos que não serão executados.

A função de altura constrói seu filtro e raio descendente. Com posição
anterior ausente, a elevação é zero. Com posição anterior presente, usa o
maior valor entre a diferença Y e o literal selecionado pelas flags. A saída
opcional de atributos no caminho de contato ainda não foi recuperada.

As fontes e os ensaios originais estão em
`research/private/spatial-query-native-v1/original-contract.md`. Os manifestos
retêm hashes, snapshots, fixtures derivados dos resultados ARM e os limites
de cada comparação. Eles não demonstram inicialização do jogo, execução no
Windows, controle físico ou coop pela Internet.

Quatro sequências do ensaio de contadores retiveram listas de eventos
compartilhadas entre chamadas. O fixture canônico delimita os eventos pelos
stores originais do contador e pelos snapshots antes/depois. A derivação
anterior permanece preservada e marcada como histórica inválida; não é usada
no teste de produção. Os resultados esperados continuam vindo do ARM.

A etapa seguinte integrou `b40b88` e `2ba230`, suas máscaras e cache ao
`player_state1`; veja [aquisição do Player](player-spatial-acquisition.md).
Os números acima pertencem à validação original deste componente. O launcher
publicado continua no laboratório0.17.0.
