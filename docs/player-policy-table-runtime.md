# Tabela de política e instalador nativo

`player_policy_table` fecha a representação dos registros de 22 bytes e os efeitos do instalador original `0xb6a97c`. O módulo é isolado: não executa predicados, não escolhe ações e não foi ligado ao jogador persistente. O interpretador `0xb0d0e4` permanece um consumidor separado.

## Fonte e ordem observada

ELF ARMv6 little-endian SHA256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. O instalador executa:

1. Converte o terceiro argumento **int32 com sinal** para binary32.
2. Escreve a referência de tabela em `pw+100`, o resultado em `pw+140` e `+0` em `pw+144`.
3. Zera os u16 em `+104`, `+108`, `+106`, nessa ordem, e o byte `+148`.
4. Executa o fill original `10b778 → 2fef54`, limpando exatamente 48 bytes de `pw+10a` até `pw+139`.

A referência `pw=u32(player+a29c)` é relida no original entre as escritas. O perfil nativo representa objetos separados, sem alias entre o slot dessa referência, o bloco de destino, a fonte da tabela e a pilha. Essa separação está explícita no oracle. Nenhum conteúdo antigo do bloco é lido; nenhuma tabela é desreferenciada pelo instalador. Por isso `install(TableReference,int32)` devolve o estado novo e não exige um estado anterior artificial.

Os readbacks autoritativos, conferidos byte a byte contra o ELF, são:

- `rea-session-1792300233949/053-read_bytes.json`: 112 bytes em b6a97c, Evidence `ev_ceb4074219aea6d0526cbb0f7b81b224e80aec72363cb20a7ea3746892ebc59f`.
- `rea-session-1792300233949/068-read_bytes.json`: 792 bytes de f2efb8, Evidence `ev_04ef22a5914b2ca3e699b3872677569ae05feb43ea18634273689a106604db62`.
- `rea-session-1792283675798/125-read_bytes.json`: 64 bytes do fill10b778, Evidence `ev_3ab05721f16bb01485ed09f5749e4f3de037688288dd06e4ac309e5e04184370`.
- `rea-session-1792285120229/038-read_bytes.json`: 96 bytes em2fef54, Evidence `ev_d58377004abb45cd9223f3bd28b06aa78102d126df4753fc1a15679cadd8366f`.

Os bytes consumidos pelo helper acabam em2fefa4; a retenção inclui o restante, sem atribuí-lo à execução. Não foi aberta outra sessão REA neste recorte.

## Dono da tabela

`Table::decode(span<const u8>,expected_sha256)` valida a identidade e a cobertura estrutural antes de copiar o carrier. Uma `Table` possui bytes imutáveis, registros, SHA256 e identidade monotônica de snapshot. Cópias compartilham lifetime e identidade; dois parses dos mesmos bytes produzem snapshots distintos. As views exigem manter uma cópia do dono viva. O parser não retém a memória mutável do chamador.

Cada `Record` conserva `raw[22]`; getters com nomes de offsets expõem bytes00/01, u16 little-endian02/04/06/08/0a/0c/0e e int16 little-endian10/12/14. Não há máscara, saturação, interpretação de nomes de movimentos ou conversão de timing durante o parse. Os campos ainda incertos permanecem bytes/words fonte. A tabela real contém35 registros e sentinel em offset770; são771 bytes examinados dentro do carrier de792 bytes. O restante do sentinel permanece retido, não interpretado.

A ordem de parada exige atenção: `b100e8` faz `LDRB [r4,#22]!`, incrementa o índice e testa `0xff` **antes** de alcançar o teste de cap em `b0d1ac`. Assim, depois de48 registros inteiros, o primeiro byte da posição49 ainda é lido. O parser rejeita um slice de exatamente1056 bytes sem esse byte. Se o byte seguinte éFF, `Coverage` informa sentinel; caso contrário, informa cap. Em ambos, conserva o valor observado e `examined_bytes=1057`. Um sentinel inicial precisa somente de seu primeiro byte, não de um registro22B completo.

O domínio de carrier desta API permite até1078 bytes:48 registros e um slot22B adicional. Esse teto é um limite explícito da representação, não tamanho máximo inferido de todo arquivo/executável. Bytes após a parada são retidos até esse teto e não recebem interpretação. O chamador escolhe o slice e sua identidade; não deve passar o ELF inteiro. Truncamento de um registro que o parser precisa representar é erro, mesmo que um futuro contexto de execução pudesse retornar antes dele. A biblioteca não afirma que o parse estrutural equivale à execução completa do interpretador.

## Estado e instalação

`State` declara somente `table`, `field104/106/108`,48 latches, `timer140_bits`, `timer144_bits` e `field148`. `TableReference` diferencia `Unknown`, `Absent` e `Table`. Instalar uma referência desconhecida ou ausente é válido, pois o original somente copia o valor. Ausência não é inventada a partir de uma falha de lookup. Não existem endereços ARM no runtime nativo.

A conversão int32→binary32 usa aritmética inteira e arredondamento nearest-even. Isso preserva todos os int32, inclusive INT_MIN e os empates acima de2^24, sem depender de cast otimizado ou do modo de arredondamento do host. O perfil corresponde ao original executado com FPSCR0/RNE; não modela flags de exceção FP nem promete equivalência aos outros modos FPSCR. O timer é devolvido como bits; não é limitado a zero ou interpretado como segundos/FPS. `timer144_bits` recebe os bits literais de+0. Campos antigos arbitrários, inclusive NaNs, são substituídos apenas porque o original os sobrescreve.

**Fronteira de integração pendente:** `player_transition::PwFields` já representa `field100/104/106/108`, `fields10a`, `field140/144/148` como campos raw. Antes de ligar o interpretador ao Player, esses campos precisam de um único dono com projeção/reintegração explícita na ordem dos efeitos. Este módulo não foi anexado ao Player e não cria uma segunda cópia mutável. O adapter de teste projeta o resultado em memória privada para comparação, sem introduzir essa duplicação no runtime. Bytes opacos entre13a..13f e fora do bloco não pertencem ao novo State e permanecem intactos no oracle.

## Prova e limites

`research/private/player-policy-table-v1/verify_arm.py` executa o instalador inteiro e ambos os helpers reais, sem stubs, rewrites de PC ou substituição de resultados. Unicorn2.1.4/ARM11MPCORE é ferramenta privada; não é dependência do porte. Guardas verificam identidade ELF, código permitido, leituras inicializadas, escritas somente no bloco/pilha, retorno, registradores preservados e orçamento.

São **19.842 retornos originais**, cobrindo a faixa inteira-300..300, fronteiras e empates em torno de2^15..2^31,2.048 amostras int32 determinísticas adicionais, ponteiros de tabela zero/fonte/valores não mapeados, pois não são lidos, e três padrões de memória prévia. Cada execução conserva512 bytes antes/depois de pw. Os seis controles negativos removem ponteiro, literal, helper, permissão de escrita, último byte de latch ou orçamento. Todos falham no guard esperado.

Um probe separado executa somente os blocos originais de avanço/cap:12 casos e um negativo confirmam a leitura antecipada da posição49 e a ordem sentinel/cap. Esse probe não é anunciado como oracle de b0d0e4 inteiro.

O C++ compara todos os512 bytes projetados por caso, incluindo os bytes opacos preservados, a identidade tipada da tabela e os bits do timer. Os testes de parse cobrem sentinel nas49 posições, registro truncado, falta do byte após cap, tail retido, SHA, limite antes da alocação, cópias/lifetime, isolamento do input, campos signed/unsigned e timers sob os quatro modos de arredondamento do host. Não existem nomes de ação, seleção de hardware, predicado, execução de combate, bootstrap completo ou teste Windows nativo reivindicados nesta entrega.


A validação final em `research/private/player-policy-table-v1/final/validation.json` passou **10.218.872 checks por execução**, normal e ASan/UBSan estrito (`-fno-sanitize-recover=all`), com stdout exato e stderr vazio. Dez processos negativos adicionais verificam fixture truncada, tail, magic, count e identidade. Os15 inputs usados — fontes, headers transitivos, readbacks, ELF, scripts e biblioteca Unicorn — foram fixados antes/depois. A fixture original tem SHA256 `95e39d228d4f2fa0722732ed387389a5bbb7e2a2509269348d1957c4526a7ad3`; a tabela real tem SHA256 `688f58d2943eea064fbbd3b1063a9a49183128b30080523034f614c6f912a83a`.

O replay original final consumiu cerca de5,23 s de parede,5,35 s de CPU e260.816.896 bytes de pico RSS do processo Python, sem filhos; foram1.150.836 instruções,198.420 leituras e416.682 stores, no máximo58 instruções por instalação. Os testes nativos completos, incluindo leitura/hash da fixture e checks, levaram aproximadamente0,39 s normal e0,65 s sanitizado. Isso não compara desempenho ARM/3DS com PC: são ferramentas e tarefas diferentes. Caches cold e tokens da tarefa não foram medidos. O review do root aprovou a representação e a fronteira isolada sem achados materiais; o manifesto fixa essa revisão separadamente da evidência de execução.
