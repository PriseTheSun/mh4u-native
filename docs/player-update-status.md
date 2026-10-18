# Indicadores no estado persistente do jogador

`Player::prepare_and_update_status` estende a composição anterior até antes
de `b145e4`, após o retorno de `b22164`. A preparação, o despacho, os ajustes
de velocidade e os indicadores pertencem a uma única operação. A renderização
e a publicação do Player, da coleção e das alterações da grade acontecem
somente depois de todas essas etapas terem sucesso.

O scanner dos indicadores consulta a coleção candidata produzida pela
admissão, quando ela existe. A mutação de `D349` feita por `686fc0` é preservada
nessa coleção. O scanner atual não lê `D349`: esse caso comprova continuidade
e propriedade do estado, mas não distingue numericamente o uso da coleção
anterior. A escolha da coleção candidata também foi conferida na implementação.
O incremento do temporizador usa o mesmo snapshot de `Player+0x14` das etapas
anteriores, sem aceitar um segundo relógio independente.

`player_update_status::apply` reutiliza os donos de pós-despacho e dos
indicadores. A camada de animação compartilha a projeção limitada dos oito
slots entre os três pontos de parada, e o Player compartilha o plano de
publicação. Os campos de posição, pose e relógio de animação não ganham um
segundo dono.

Falhas anteriores preservam sua causa original. Campos desconhecidos ou
emissão de evento ainda não recuperada em `b22164` também retornam uma causa
estruturada. Nenhuma dessas falhas publica o prefixo já calculado.

Esta API ainda é uma operação explícita de diagnóstico. Não executa
`b41804`, não retorna do chamador completo `b1407c` e não fecha o scheduler
nem a inicialização do jogo. O launcher publicado continua na versão 0.17.0.
As evidências da integração ficam em
`research/private/player-update-status-v1/`; a investigação das notificações
seguintes fica em seu diretório `research/`.

O corpus original contém 127 entradas contínuas desde `b1407c`: 125 chegaram
ao novo ponto de parada e duas pararam antes de `c08238`, após executar o
construtor real do evento. Não foram usados retornos de helpers simulados nem
alterações de PC durante a execução. Os estados, bancos e recursos de
diagnóstico permanecem explícitos; esses casos não demonstram a inicialização
original do jogo.

A comparação da composição nativa passou em 2.876.515 verificações, mais
659.704 verificações de projeção de animação por perfil normal e com
sanitizadores. Foram conferidos os estados e os efeitos retornados pela
composição contra as observações originais, incluindo 13 varreduras, nove
incrementos e duas limpezas de temporizador. Os 84 processos com entradas
inválidas terminaram exatamente com exit 1, sem saída de sucesso nem
diagnóstico dos sanitizadores. As regressões de pós-despacho e da folha de
indicadores mantiveram 3.285.761 e 1.671.978 verificações, respectivamente.
O build e os testes privados levaram 295,04 s; CPU e pico de memória da
família de processos nativos não foram medidos.

A integração persistente passou em 1.728.284 verificações por perfil normal
e ASan/UBSan com `float-cast-overflow`. Isso inclui 53 falhas de alocação até
a primeira falha de capacidade do renderer, sempre seguidas de uma tentativa
no mesmo Player. Os testes dos dois pontos de parada anteriores preservaram
suas contagens: 1.682.334 para ajuste e 1.712.659 para despacho. Foram rejeitados
16 processos negativos, com exit 1 e sem diagnóstico dos sanitizadores.

A suíte pública passou em 147/147 CTests no Mac, em 58,03 s. O build
incremental com esses testes levou 93,74 s; a compilação Windows x64 levou
38,48 s. Ambos terminaram sem avisos, com 320 entradas estáveis antes/depois.
Os builds rodaram em paralelo e não constituem uma comparação de desempenho
com versões anteriores. A execução em Windows, o pico de memória da família
de processos e o custo em tokens desses builds não foram medidos.

A investigação separada de `b41804` recuperou a fila de 16 registros usada
pelas notificações. Foram executadas 252 chamadas originais e 11 controles
de guarda. Isso prepara a próxima implementação; ainda não há fila nativa
integrada, consumo visual das notificações nem avanço do Player por essa etapa.

Combate, missões, saves, tradução integral, validação de controles físicos,
execução no Windows e coop jogável pela Internet continuam necessários para
concluir o port solicitado.
