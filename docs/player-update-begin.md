# Início de atualização do Player

`player_update_begin` implementa a rotina completa `b3b3a0` sobre os campos
canônicos do Player. Suas 16 gravações publicam a posição anterior, copiam a
área atual, limpam as flags originais e invalidam o cache de altura. A altura
armazenada permanece intacta; só o byte de validade é zerado. PBW6c0 e PW6c0
continuam campos distintos.

As coordenadas são copiadas como bits de 32 bits, incluindo NaNs, infinitos e
zero negativo. Esta rotina não calcula nem normaliza coordenadas. Seus cinco
campos opcionais de entrada precisam estar conhecidos: XYZ atual, área e
PW1f8. Os destinos sobrescritos podem estar desconhecidos. PWe8 já pertence
ao estado canônico conhecido da transição; a operação preserva seus outros
bits, assim como faz com PW1f8. Nenhum campo ganha um segundo dono.

`Player::begin_update()` expõe essa etapa de forma explícita. O Player valida
o resultado e a renderização antes de publicar alterações. Não avança o
relógio de animação nem o contador de passos, e não é chamado automaticamente
por `dispatch_state1`, `step` ou `tick`.

Essa escolha preserva a ordem recuperada: `b1407c` chama a rotina antes da
política e dos possíveis dois despachos de ação. Reinicializar o cache a cada
despacho faria a segunda ação consultar o cenário novamente, contrariando a
vida útil observada. O chamador completo tem outras etapas e gates ainda não
implementados; uma chamada explícita não representa um frame completo do jogo.

A prova da rotina original e os testes de composição são separados. A
referência ARM executa a função inteira com arenas disjuntas e registra suas
gravações e os bytes antes/depois. O teste persistente usa a consulta de
altura e o recurso SBC real já integrados. A origem dos campos do mundo e do
input permanece explícita, sem valores de bootstrap inventados.

Passaram 96 chamadas completas originais, com oito controles de leitura.
Cada chamada é comparada com os destinos previamente conhecidos e também
desconhecidos: 82.509 verificações por perfil normal e com
ASan/UBSan/float-cast-overflow, além de 52 negativos de processo. As arenas
originais totalizam 8.650.752 bytes; somente 3.264 bytes são destinos gravados
pela rotina. Os bytes restantes servem para verificar preservação, e não são
dados produzidos pelo motor nativo.

A integração persistente passou 272.990 verificações, em modo normal e com
sanitizadores. Confirma início → consulta → reutilização → novo início →
nova consulta, além de rejeições sem publicação por fonte desconhecida ou
falha de renderização. São testes de composição nativa, não uma execução
completa do chamador ARM. Na verificação com sanitizadores, 53 unidades do
projeto foram instrumentadas; SDL e bibliotecas do sistema não foram.

A suíte completa passou em 113/113 CTests no Mac, em 47,88 s. O build
incremental completo com testes levou 63,78 s; a compilação cruzada Windows,
18,54 s. Os dois builds rodaram em paralelo e não emitiram avisos do
compilador. O alvo de integração Mac já havia sido compilado durante o
desenvolvimento. Não houve execução no Windows. CPU dos filhos está nos
relatórios; pico de memória da família de processos e custos de tokens não
foram medidos. Esses tempos não são uma comparação de desempenho do jogo.

Evidências e validação deste marco ficam em
`research/private/player-update-begin-v1/manifest.json`. O contrato estático
anterior permanece congelado em
`research/private/player-spatial-acquisition-v1/next-cache-lifecycle.json`.

O launcher publicado continua na versão 0.17.0. Esta alteração pertence à API
do motor em desenvolvimento; não acrescenta interface nova. Combate, missões,
saves e cooperativo pela Internet ainda dependem da reconstrução dos sistemas
restantes. A [preparação local de entrada](player-input-preparation.md) foi acrescentada
posteriormente; o chamador completo e o scheduler continuam pendentes.

Na validação posterior da preparação de entrada, foi corrigido um erro no
adaptador deste teste: ângulos e magnitudes pertencem a PBW3c0/3c4, mas eram
lidos de 3b0/3b4. A rotina b3b3a0 não usa nem altera esses campos, por isso a
comparação anterior não detectava a atribuição errada. O adaptador corrigido
também conserva a nova representação de bytes parcialmente conhecidos.
A mesma referência de 96 chamadas passou novamente: 112.751 verificações
por perfil normal e sanitizado, com 52 negativos. Os relatórios históricos
permanecem preservados; a correção está em
`research/private/player-input-filter-v1/begin-mapping-amendment/`.
