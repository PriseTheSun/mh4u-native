# Continuação do gerenciador no Player

`Player::prepare_and_update_manager` prolonga a atualização iniciada em
`b1407c` até antes de `b146d8`. As chamadas `b11f3c` e `b24c44` usam o estado
produzido pela preparação, pelos despachos, pelos ajustes, pelas notificações,
pela direção e pelo temporizador de posição.

O seletor consultado em `Player+244` pertence à primeira camada de animação.
A composição consulta essa camada depois do despacho. A coleção de objetos
e a posição também vêm do candidato anterior; não são cópias de entrada
reaplicadas depois da atualização.

O gerenciador tem um estado explícito e pode apontar para o próprio Player ou
para outro Player. O contexto global continua sendo o do chamador. Quando o
código original alcança outro personagem, apenas os dois valores verticais
`PWdc` e `PWe0` desse alvo são alterados. O ponteiro emprestado vale somente
durante a operação e não fica armazenado no estado persistente.

A publicação reúne o chamador, o alvo alcançado, o gerenciador, a coleção,
a grade espacial e a fila de notificações. Ambos os quadros visuais são
preparados antes da primeira publicação. Erros preservam todos os donos do
estado; apontar para o próprio Player usa um único candidato e uma única
preparação visual.

A implementação distingue valores conhecidos, ausentes e ainda desconhecidos.
Os caminhos que retornam cedo preservam os campos não consultados. Os valores
iniciais dos ensaios são dados explícitos de diagnóstico, sem afirmar que o
bootstrap ou uma partida original foram recuperados.

O comparador isolado usa 408 observações originais. O ensaio persistente usa
as sete partes e os 19 ossos do caçador e percorre as 243 alocações de uma
operação bem-sucedida com alvo distinto, provocando uma falha por vez. Cada
falha é seguida de nova tentativa sobre os mesmos donos. A cobertura inclui
as duas preparações visuais e erros próprios da geração de paletas.

A comparação contínua preserva 239 entradas originais: 237 chegam a esse
ponto e duas param na emissão de eventos ainda não recuperada. Os 212 prefixos
do marco anterior são conferidos integralmente, com as novas observações
separadas de qualquer estado inicial inferido. Os perfis normal e instrumentado
passaram; o teste persistente instrumenta 81 unidades do projeto e conserva
421 pares de entradas antes/depois. Bibliotecas externas e do sistema não
fazem parte dessa instrumentação.

Os 163 CTests passaram no Mac. A compilação Windows x64 terminou sem avisos;
execução no Windows continua pendente. O próximo trecho tem 262 observações
originais preservadas, mas ainda não foi integrado ao Player.

As evidências e limitações deste marco ficam em
`research/private/player-update-manager-v1/`. O aplicativo publicado continua
sendo o laboratório nativo 0.17: este trecho ainda não forma um ciclo completo
de jogo. Combate, missões, salvamento, tradução integral, validação de controles
físicos, execução no Windows e cooperação pela Internet continuam pendentes.
