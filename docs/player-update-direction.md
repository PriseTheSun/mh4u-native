# Direção no estado persistente do Player

`Player::prepare_and_update_direction` continua a atualização original até
antes de `b14610`, depois do retorno de `b6d2a4`. Usa o estado já produzido
pela preparação, pelo despacho, pelos indicadores e pelas notificações.
O delta continua sendo o único valor de `Player+14` fornecido à operação.

A direção atual e seu destino permanecem nos campos canônicos `PBW24` e
`PWb0`. A rotina seleciona a taxa conforme o estado e a ação, faz as conversões
e operações binary32 na ordem original e conserva os resultados de 32 bits.
Em um ramo o destino completo é copiado; no outro, o resultado é reduzido a
16 bits. Valores negativos, NaN e infinito seguem as operações recuperadas,
sem uma regra adicional de interpolação ou normalização.

As identidades dos dois métodos virtuais de uPlayer são explícitas. O método
`virtual144` só é necessário quando o byte `PBW6f9` vale 1; `virtual84` é sempre
consumido. O valor carregado de `Player244` pelo segundo método é descartado
pelo original. A implementação reproduz seus efeitos e a ordem das chamadas,
sem inventar um segundo campo de estado ou alegar identidade de cada leitura
de instrução. A regra compartilhada `b43dcc` agora tem um único dono, usado
pela admissão e pela direção com seus argumentos originais distintos.

Antes da direção, o chamador executa `b29f7c` para o tipo 13. Os retornos em
que o ordinal permanece igual ou o banco já está selecionado usam os
predicados originais. O seletor `PW9e8` é opcional e só é lido quando alcançado.
Os caminhos que alteram recursos e materiais ainda retornam um erro explícito;
suas dependências de clone, referências e destruição continuam em recuperação.
Nenhuma dessas chamadas foi substituída por sucesso sem efeitos.

A mesma operação prepara a renderização uma única vez. Jogador, coleção,
grade e fila do mundo só mudam depois do sucesso de todas as etapas e da
construção da saída. Falhas tardias preservam os quatro estados. Os métodos
anteriores conservam seus próprios pontos de parada e não passam a executar
a nova etapa automaticamente.

A janela nativa da vila permanece aberta durante os testes, conforme o pedido
atual do usuário. Ela usa o perfil publicado de inspeção do Player; este novo
trecho de atualização ainda não constitui o bootstrap ou o ciclo completo de
uma partida. Combate, missões, saves, tradução integral, controle físico,
execução no Windows e coop pela Internet continuam pendentes.

A comparação contínua cobre 190 entradas originais: 188 alcançam esse ponto
e duas param antes da emissão de eventos ainda não recuperada. A rotina de
direção isolada coincide com outras 3.828 chamadas preservadas do original.
Os perfis normal e com ASan/UBSan/float-cast-overflow passaram. No comparador
contínuo, 94 processos rejeitaram entradas inválidas; na rotina isolada foram
40. Os testes do Player preservaram os quatro estados em 53 pontos de falha
de alocação, incluindo o primeiro dentro da preparação real da renderização.

Os 155 CTests passaram no Mac. A compilação Windows x64 terminou sem avisos;
execução no Windows continua pendente. O perfil instrumentado do Player cobre
81 unidades C++; cgltf, SDL e bibliotecas do sistema ficam fora da instrumentação.
A pesquisa seguinte executou 100 chamadas originais de materiais e cores,
mas isso ainda não fecha os caminhos de clonagem e destruição de recursos.

As evidências deste marco ficam em `research/private/player-update-direction-v1/`.
