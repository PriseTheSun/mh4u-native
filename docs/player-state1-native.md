# Ações do estado1 no motor nativo

`player_state1::apply` reimplementa as entradas (`pbw.byte5 == 0`) dos IDs1,2,4,9
e as continuações observadas dos IDs1,2,4,9, selecionados por `b66a20 → b6a424`.
A operação compõe os donos existentes de
controlador, política e comando de animação; não executa código ARM no runtime.
`Player::dispatch_state1` liga essa operação ao personagem persistente de sete
peças. Política, despacho e avanço da pose continuam sendo chamadas explícitas.
Não há scheduler, bootstrap ou associação desses IDs a botões físicos.

## Contrato e efeitos

O controlador é o único dono dos campos compartilhados PBW/PW. O novo
`player_state1::Fields` contém apenas os bits opcionais de `pw+50`, distintos
dos timers da política. As tabelas imutáveis são fornecidas em `Tables`;
Unknown e Absent continuam diferentes. O instalador não precisa ler a tabela
para atribuir essa referência, portanto não inventa conteúdo quando ela é
desconhecida. A origem e o hash dos arquivos pertencem ao chamador.

| ID | Pedido original na entrada | Instalação e efeitos posteriores |
| --- | --- | --- |
| 1 | `if_changed`, selector3/interpolação6; selector4/interpolação0 quando o atual é4 | Tabela f2efb8 antes do pedido; timer50 recebe20 depois |
| 2 | `if_changed`, selector3 ou4, interpolação4 | Tabela f2efb8 depois; timer50=20, timer144=30 e cópia da direção |
| 4 | `restart`, selector42, interpolação0 | Copia a direção antes; instala f2f2d0 depois; não consome timer50 |
| 9 | `restart`, selector49, interpolação6 | Timer50=20 antes; instala f2efb8 depois |

As flags e demais escritas preservam a largura original. Os tails também
executam quando o setter recusa validamente o pedido ou `if_changed` não chama
o setter. A saída registra comando, pedidos, aceitação, ordem da instalação
e contagens de consulta/amostragem. IDs não reconstruídos e continuações
consumidas ainda sem implementação retornam erro explícito. Os subestados
que o original ignora preservam tudo, exceto o byte376 zerado pelo dispatcher.

`motion_command::validate_storage` centraliza o limite das oito posições e
dos bindings antes de copiar. O driver une temporariamente duas camadas ativas
e seis inativas e devolve cada uma ao mesmo dono. Não há segunda pose persistente.
O Player só publica controlador, camadas, timer, revisão e contadores depois
da renderização bem-sucedida. A chamada não avança `completed_steps`.

## Comparação independente

O [oráculo original](player-state1-runtime.md) executa 384 chamadas ARM completas,
sem substituir helpers ou resultados. A fixture MH4US102 tem SHA256
`5a48fa8dcf24f45cfe45456115e122eded7a7abdde7726608af32a198ce0ce0c`.
O consumidor C++ compara todas as sete arenas de saída e a preservação da
entrada, não apenas os campos esperados como alterados. São 61.341.696 bytes de
saída, 576 setters, 120 aceitações, 96 chamadas sem setter, 288 consultas e 960
amostragens. A comparação passa 66.007 checks.

O adaptador de teste preserva ponteiros antigos dos slots inativos como tokens
opacos reversíveis. Não os converte em recursos utilizáveis; decode ativo e
resolver os recusam. Isso conserva bytes não consumidos sem fabricar tracks.
O mesmo adaptador atende a regressão anterior de recursos de animação.
Um probe isolado passou 51 verificações de aliases, roundtrip e rejeição de
tokens opacos ativos; ele testa o adaptador, sem executar ARM novamente.

O consumidor passou nas compilações normal e ASan/UBSan/float-cast-overflow
estrita, com 21 negativos por perfil e 43 inputs/headers fixados antes/depois.
São controles de identidade, formato, truncamento e corrupção detectável
da saída. Os positivos produziram stdout exato e stderr vazio.

A primeira integração das entradas passou 75.887 checks com 7 peças, 19 juntas, 4.507
vértices fonte e 5.043 triângulos. Encadeia decisão da política → despacho
explícito → step da pose, cobre recusas, erro tardio e rollback quando a
renderização rejeita o ambiente numérico. As regressões de política e Player
também passaram nas compilações normal e ASan/UBSan estrita. Os relatórios e
snapshots ficam em `research/private/player-state1-actor-v1/`.

Os negativos do consumidor, os gates de compilação e a revisão do adaptador
ficam em `research/private/player-state1-native-v1/`. A prova ARM é separada
da prova de composição C++; testes com contextos preparados não estabelecem
o estado inicial real do jogo.

## Continuações nativas

| ID/subestado | Comportamento recuperado |
| --- | --- |
| 1/1 e 1/2 | Consome controle local, magnitude, timer e referência somente nos ramos originais. Pode transitar para1/3 ou1/26. No subestado1, se não houver transição e a animação sinalizar o gate, reinicia selector4 e avança ao subestado2. |
| 4/1 | O gate da camada0 chama a transição1/5, sem pedir animação. |
| 9/1 | Magnitude não zero atualiza direção. Com magnitude zero e timer esgotado, chama o wrapper de seleção do estado0; o dono existente decide a animação e a transição. |

`ContinuationContext` recebe fatos emprestados: constantes, projeção dos campos
de referência e contexto externo/área. No Player, `pw+14c/150` agora pertencem
ao único dono `player_fields::Fields`, compartilhado por política e despacho;
`pw+50` continua em `state1_fields`. A orientação usa
`player_reference::resolve_state/orient`; a transição direta usa
`player_selection::direct_transition`. O intérprete anterior e `resume` usam
esses mesmos donos. Não há duplicação de campos, tabelas ou interpretação do
estado compartilhado. `Error::cause` preserva a origem tipada do erro.

O timer finito é comparado com zero pelos bits binary32, incluindo zeros com
sinal e subnormais. NaN/Inf consumidos geram erro; dados não consumidos podem
continuar desconhecidos ou conter bits não finitos. O runtime preserva as
consultas repetidas de magnitude; sua saída conta as consultas, sem afirmar
que expõe um trace completo de todos os helpers.

O consumidor independente MH4US1D1 deriva 235 casos das
[375 continuações ARM](player-state1-continuation.md), sem alterar os bytes
antes/depois: ID1=83, ID4=76, ID9=76. SHA256 da fixture:
`b304c4760b13938787921e427ad8f7b78dc8ab853f2139fade52e0ecb26dbcf3`.
Compara sete arenas e identidades lógicas de recursos, bancos e canais. São
17 transições, 82 consultas de magnitude, 20 setters, 10 aceitações e 10
wrappers. Os 101.754 checks incluem 13 erros nativos e quatro sucessos com
contexto desconhecido não consumido; estes controles não são novos casos ARM.
Esses 235 casos têm zero consulta viva de pose, porque seus flags mantêm a
troca pendente. A cobertura anterior de entrada permanece separada.

A composição ampliada do personagem passou 151.682 checks: política → entrada
→ avanço até o gate real → continuação → novo avanço explícito. Também cobre
a seleção do LMT co2 real, a ligação/amostragem no passo seguinte, orientação
com campos canônicos, recusa do setter, banco desconhecido e rollback após
erro de renderização. Sua recipe privada preserva as sete peças e acrescenta
o LMT por hash; não modifica a montagem publicada. Os relatórios atuais estão
em `research/private/player-state1-continuation-native-v1/`.

As comparações e a integração passaram em modo normal e com
ASan/UBSan/float-cast-overflow. O consumidor das continuações passou 18
controles negativos de processo por perfil; a integração, 12. As 384 entradas
foram revalidadas contra o código atual, com 21 controles por perfil. Os
98 CTests passaram no Mac e a compilação Windows x64 concluiu; execução no
Windows continua sem validação.

## Próxima fronteira

O [ID2 nativo](player-state1-id2-native.md) compõe custo, posição e o perfil de
contexto recuperado, comparado em252 chamadas originais. A aquisição espacial
sem cache permanece pendente no runtime. O decremento do timer pelo
scheduler, o bootstrap e os estados de destino também
continuam pendentes. Este avanço não acrescenta combate, missões, saves ou
coop pela internet. O launcher 0.17 permanece como inspeção da cena nativa;
as novas continuações pertencem à API em desenvolvimento.
