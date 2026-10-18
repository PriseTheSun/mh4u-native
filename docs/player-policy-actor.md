# Política original no dono persistente do caçador

`Player::apply_policy` integra o intérprete nativo de `0xb0d0e4` ao `Player`
que já possui as duas layers ativas, seis slots inativos, joints, snapshots e
saída de hierarquia. A tabela real de 35 registros continua no dono imutável
`player_policy_table::Table`; não há parser adicional, endereço ARM em estado
nativo ou cópia persistente de `player_selection::State`.

Esta é uma integração de contexto explicitamente escolhido. Não recupera o
bootstrap completo de uPlayer, o instalador da tabela no scheduler, controles
físicos ou combate. A ligação posterior da decisão a pedidos de animação
agora existe para quatro entradas e quatro continuações do estado1, em uma
[operação explícita separada](player-state1-native.md).
`apply_policy` não chama `request`, não avança o relógio e não reinstala a tabela
apagada por uma transição. O contexto de inspeção não deve ser apresentado
como estado inicial do jogo.

## Donos e ordem

`PlayerInitial::shared_fields` fornece somente os offsets adicionais já
interpretados pelo dono `player_policy_interpreter::Fields`. O controlador
continua sendo dono dos campos compartilhados de PBW/PW, inclusive
`pw.policy`; `layers[1].animation.layer.weight` continua sendo o único dono dos
quatro bytes também observados pelo controlador em `player+0x3b8`.

A operação realiza a projeção temporária existente `selection_view`, aplica os
patches explícitos de entrada e executa o intérprete com a primeira layer, os
campos auxiliares de motion e as constantes pertencentes ao próprio Player.
Somente depois do sucesso copia o estado limitado já possuído, reintegra o
resultado usando `player_controller_step::store_selection`, gera a mesma
paleta/skinning/Frame e publica a transação. Um erro de interpretação ou render
não publica estado, entrada, campos extras, revisão nem contadores parciais.
O estado privado só é produzido pelas fronteiras que verificam os limites de
joints, snapshots, bindings e order antes da cópia.

`store_selection` também é usado internamente por `player_controller_step`.
Ele verifica o extent de duas layers antes de qualquer store e preserva os
bits raw do peso, inclusive NaN. Não há nova interpretação desse alias no
consumidor.

`PlayerPolicyInput` contém patches opcionais de `Channels`, `field3d4`, os 19
words seguintes de flags e os quatro masks de 16 bits. Omissão preserva o
valor existente, inclusive desconhecido; zero explícito é um valor conhecido.
A API não associa esses campos a teclado, joystick ou nomes de ação.
`PlayerPolicyContext` empresta delta, referência espacial e fatos de contexto
somente durante a chamada. Campos desconhecidos são exigidos apenas quando
o caminho original os consome. O resultado conserva retorno, ordem de visitas,
eventual transição observada e Frame; não agenda a transição uma segunda vez.

## Preview explícito

A factory antiga `PlayerPreview::from_actor` não instala política. A nova
`from_actor_with_policy` exige `PolicyPreviewSeed{PlayerFields, Fields}` do
caller. Esta receita de reset contém campos do controlador e uma referência
imutável à tabela; não contém outra pose, modelo ou conjunto de layers.
`reset()` recompõe o seed explícito, enquanto chamadas comuns não o reaplicam.
Todos os demais bancos continuam desconhecidos no perfil de preview existente.
`apply_policy` é uma operação distinta de `request` e `step`.

## Evidência e validação

A prova independente do intérprete está em
`research/private/player-policy-interpreter-v1/arm-source-order/`: 341
execuções completas de ARM original, com sete arenas antes/depois, visitas de
todos os registros alcançados, helpers e `b1c050` reais, sem substituir PC ou
retornos. Fixture SHA256
`48b82ec4e7d9bc2a4a2e0450833349bbe772ec217eec18f33efcee66c3081798`;
ELF SHA256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
A integração descrita aqui não constitui outra execução ARM.

`src/player_policy_actor_tests.cpp` usa o caçador MOD/LMT de sete peças:
19 joints, 4.507 vértices fonte, 5.043 triângulos e 20 draws. Exercita as
visitas ordenadas aos 35 registros, decisões 32/33/34 e reorientação, latches,
timer e bloqueio148, conhecido/ausente/desconhecido, patches omitidos e zero,
peso compartilhado raw e erro tardio sem publicar escritas precedentes. Também
prepara explicitamente a ação LMT8, executa seis steps reais do dono nativo e
aplica política sobre a layer em reprodução, comparando toda a estrutura
canônica e os vértices renderizados. A escolha dessa ação é um contexto de
teste, não uma conclusão sobre bootstrap ou política de gameplay.

Os relatórios finais foram retidos em
`research/private/player-policy-actor-v1/native-normal/validation.json` e
`native-strict/validation.json`. Ambas as execuções passaram 121.306 checks dedicados e
a regressão anterior de 1.573.865 checks, com stderr vazio, saída exata e 167
inputs transitivos pinados antes/depois. ASan/UBSan usa
`-fno-sanitize-recover=all`; a biblioteca SDL estática tem identidade pinada,
mas não é instrumentada. Os negativos de processo verificam os tamanhos dos
inputs e identidades da tabela/constantes. O manifesto registra os resultados
realmente executados e distingue tentativas preliminares de relatórios finais.

O inventário `Player::source_files()` continua sendo o inventário do Data MOD/LMT/texturas. A tabela fornecida pelo caller é um owner separado, identificado pelo SHA256 completo e pelo token de snapshot; o consumidor deve preservar separadamente a origem desse input, pois a API não inventa um caminho para bytes recebidos.
