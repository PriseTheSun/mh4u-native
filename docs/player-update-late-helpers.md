# Indicadores, temporizadores e posição do jogador

Esta etapa recupera componentes posteriores ao ponto `b145dc` da atualização
do jogador. O Player persistente continua parando antes desse ponto: as novas
rotinas são operações nativas isoladas, ainda sem agendamento no launcher.

`player_status_update::apply` reproduz os caminhos de `b22164` que retornam
sem emitir um evento. Calcula as palavras `PW1f4/PW1f8` e o byte `PW299`, usando
os campos canônicos do jogador. Preserva comparações de palavras com sinal,
arredondamento binary32, NaNs e a ordem das consultas condicionais.

O novo `player_object_query::scan_status` percorre a coleção original de até
32 posições. Reutiliza a resolução de referências e a regra de disponibilidade
existentes; distingue um objeto ausente de dados desconhecidos. O predicado
`d05ac0` tem um único dono, compartilhado com o intérprete da política de ações.

Quando o temporizador cruza o limiar que aciona `c08238`, a operação retorna
`unsupported_event` e descarta todo o candidato. A criação, a chamada virtual e
o registro desse evento ainda precisam ser reconstruídos. Isso é diferente dos
eventos de animação já implementados. Não há retorno simulado para esse caminho.
A disponibilidade remota dos objetos também mantém seu erro explícito.

`player_position_timer` implementa as folhas `b04c24` (incremento) e `b069fc`
(decremento), além do trecho isolado `b14610..b146c4`. Esse trecho restaura a
posição salva quando o temporizador termina ou integra os três componentes
com as operações originais, sem fundir a multiplicação e a soma.

Há uma diferença relevante entre os testes de ponto flutuante do original:
um NaN no temporizador inicial entra no trecho e é convertido em zero pelo
decremento; um NaN produzido pela subtração segue para a integração. Não é
correto substituir todas essas condições por um único teste C++ de `> 0`.

Os dados ausentes só causam erro quando consumidos. Entradas e coleções são
emprestadas; sucesso retorna um candidato e falha não publica alterações.
As APIs preservam os resultados semânticos, sem prometer a mesma sequência de
leituras, stores intermediários ou flags FPSCR do processador original.

As evidências desta etapa ficam em
`research/private/player-update-late-helpers-v1/`. O contrato estático de
`b22164`, os bytes confirmados pelo REA, as execuções originais e as comparações
nativas são mantidos separadamente. Os casos usam estados de diagnóstico;
não demonstram o bootstrap nem um frame completo do jogo.

Foram comparados 562 retornos completos de `b22164` e quatro execuções
interrompidas antes de `c08238`. Estas quatro verificam o erro explícito de
evento pendente; não contam como retornos completos. A comparação nativa
passou em 1.671.978 verificações por perfil, acompanhada de 65 verificações
de campos desconhecidos, consultas condicionais e preservação das entradas.

Os temporizadores e o trecho de posição passaram em 4.100 casos originais:
1.024 incrementos, 1.024 decrementos e 2.052 execuções do trecho isolado.
Foram 40.877 verificações por perfil e 68 verificações de fronteiras sem
fixture. Ambos os componentes passaram nos perfis normal e ASan/UBSan com
`float-cast-overflow` sem recuperação. Os validadores também rejeitaram os
74 processos negativos do conjunto de indicadores e os 28 de temporizadores.

A projeção compartilhada dos testes agora inclui os novos campos. Por isso,
a regressão anterior de pós-despacho passou de 3.243.369 para 3.285.761
verificações, mantendo seus 107 casos, oito fases isoladas e 446 chamadas de
ajuste originais. As fixtures históricas foram preservadas. A política de
ações passou novamente nos 341 casos e 158.517 verificações por perfil.

A suíte pública passou em 145/145 CTests no macOS. Os builds incrementais
levaram 132,59 s no Mac (incluindo 62,63 s de CTest) e 59,65 s para Windows
x64, ambos sem avisos e com 313 entradas estáveis antes/depois. São medições
de builds concorrentes, sem comparação de desempenho com versões anteriores.
O Windows foi compilado, mas não executado. Pico de memória da família de
processos e custo em tokens desses builds não foram medidos.

O launcher 0.17.0 permanece aberto e não recebeu estas operações isoladas.
O próximo ponto de integração é a mesma composição transacional do Player;
o mapa de dependências retido inclui `b41804`, `b0af48` e o sistema de eventos.
Uma relação entre as pilhas de `b22164` e `b41804` foi derivada dos bytes
originais, mas ainda não foi validada por uma execução contínua do chamador.

Ainda é necessário ligar esses componentes ao mesmo candidato do Player,
fechar as demais chamadas obrigatórias e recuperar o sistema de eventos. O
objetivo continua incluindo combate, missões, saves, PT-BR integral, controles
físicos, execução no Windows e coop pela Internet.
