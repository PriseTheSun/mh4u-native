# Posição e temporizador no estado persistente do Player

`Player::prepare_and_update_position` prolonga a mesma operação até antes de
`b146c4`. Após a direção, o trecho `b14610` usa os campos canônicos produzidos
pelas etapas anteriores e o único delta fornecido para a atualização.

O temporizador `PWb8` escolhe entre três caminhos. Um valor ordenado não
positivo mantém a posição e não consome os campos de velocidade ou posição
salva. Um temporizador que continua ativo integra os três componentes com as
multiplicações e somas binary32 na ordem original. Quando ele termina, o
trecho restaura os três valores salvos e escreve zero no componente W.

A comparação inicial e a comparação após o decremento têm comportamentos
distintos para NaNs. A integração preserva o resultado das operações; a
restauração copia os valores salvos sem normalização. Posição anterior,
posição atual do PBW e transformação do modelo continuam sendo conceitos
separados. A ponte de posição do modelo e o restante da atualização não são
executados implicitamente por este ponto de parada.

O Player, a coleção de objetos, a grade espacial e a fila de notificações
continuam sendo publicados juntos após preparar a renderização. Um campo
desconhecido consumido no último componente ou uma falha de alocação descarta
todo o candidato. As APIs anteriores conservam seus próprios pontos de
parada e não passam a exigir os novos campos.

O protótipo nativo permanece aberto durante os testes. Esta integração não
constitui uma partida completa: inicialização, agendamento, demais chamadas
obrigatórias, combate, missões, saves, PT-BR integral e coop online continuam
em desenvolvimento. A compilação para Windows e o teste com controle físico
também precisam de suas validações específicas.

A comparação contínua cobre 212 entradas originais: 210 alcançam esse ponto
e duas param na emissão de eventos ainda não recuperada. Foram observados
18 temporizadores ativos, 12 integrações e seis restaurações; os demais
192 checkpoints completos preservam a posição nessa etapa. Os perfis normal
e instrumentado passaram. A projeção compartilhada dos testes foi corrigida
para incluir os sete campos de posição e temporizador já existentes, sem
alterar as observações originais.

Os testes persistentes preservaram os quatro estados em 53 pontos de falha
de alocação, incluindo o primeiro dentro da preparação real da renderização.
Os 159 CTests passaram no Mac. A compilação Windows x64 terminou sem avisos;
execução no Windows continua pendente. O perfil instrumentado do Player cobre
80 unidades C++; cgltf, SDL e bibliotecas do sistema ficam fora da instrumentação.

O módulo independente de materiais coincide com 100 chamadas originais e
preserva identidades compartilhadas dentro dos seus conjuntos tipados. Ainda
não participa das trocas de materiais do Player. Clonagem, destruição,
aposentadoria e aliases entre conjuntos de tipos diferentes exigem continuação.
O contrato privado documenta a cobertura e as validações antecipadas de limites.

A pesquisa seguinte preservou 408 execuções originais de `b11f3c`, `b24c44`
e do trecho que chama ambas. Essas observações incluem o gerenciador apontando
para outro Player. Elas não representam integração nativa já concluída.

As evidências deste marco ficam em `research/private/player-update-position-v1/`.
