# Preparação de entrada do Player

A implementação nativa de `b33638` já inclui os corpos reais dos helpers de
controle local, bloqueio externo, contexto e supressão. O código recebe os
campos originais de cada objeto, preservando a distinção entre desconhecido,
objeto ausente e valor zero. Não recebe decisões prontas para substituir esses
helpers.

A rotina pode conservar o bloco de 196 bytes de entrada, zerá-lo, restaurar
sete campos do pad após a limpeza, ou aplicar as limpezas condicionais de
`b27928`. A restauração usa os valores brutos dos halfwords. As comparações
com zero preservam o comportamento de NaNs, infinitos, zeros com sinal e
subnormais no perfil não interrompível com precisão gradual. Não há modelo
das flags de exceção ou traps de FPSCR.

Algumas gravações alcançam somente um byte de palavras usadas pela política
de ações. Por isso, essas palavras agora têm armazenamento por byte: uma
limpeza parcial não torna conhecidos os bytes vizinhos. A política monta uma
palavra somente quando todos os seus bytes estão conhecidos. Os 99 bytes
antes sem representação ganharam armazenamento próprio; campos existentes,
como ângulos e magnitudes, continuam em seus donos canônicos. O bloco completo
é uma projeção temporária, sem uma segunda cópia persistente do estado.

O núcleo `apply_block` aceita desconhecidos por byte. O wrapper canônico usa
o mesmo núcleo e publica nos campos do Player. Esse wrapper conserva o
contrato preexistente de ângulos, magnitudes, word3d4 e byte451 conhecidos,
e de quatro máscaras opcionais de 16 bits. Uma projeção que não cabe nesse
contrato falha explicitamente, em vez de inventar dados ou perder informação.

A rotina adjacente `b09ccc` também está implementada. Soma dois valores de
32 bits e compara os bits do resultado como inteiro com sinal contra
`0x4b189680`, limpando o acumulador quando a condição original se verifica.
O perfil nativo admite aritmética finita com arredondamento ao mais próximo;
operandos não finitos e overflow são rejeitados transacionalmente. Alguns
casos que o ARM limpa após produzir infinito continuam fora desse perfil.
A análise não atribui unidade nem frequência ao acumulador.

`Player::filter_input(context)` e `Player::update_accumulator(delta)` são
operações explícitas. A sequência local recuperada é filtro → acumulador →
`begin_update()`. Cada operação só publica seu próprio resultado após uma
renderização bem-sucedida. A sequência de três chamadas não é uma transação
única: uma falha posterior conserva o resultado das operações anteriores que
já retornaram sucesso. Nenhuma das três avança o relógio nem o contador de
passos, e não há chamada automática por frame.

Uma operação adicional, [Player::prepare_update](player-world-lookup.md),
agora compõe o prefixo original até a publicação de PW5a8, incluindo a busca
na coleção do mundo. Essa API faz uma única transação; as três chamadas
individuais acima conservam seus contratos separados.

O estado de pad e dos objetos externos ainda precisa ser fornecido pelo
chamador. O bootstrap, a aquisição dos dispositivos, o chamador `b1407c`
completo e o scheduler permanecem em reconstrução. Esta etapa não transforma
o laboratório em uma partida jogável.

O REA forneceu os contratos estáticos e leituras dos bytes do ELF; a sessão
1792317674134 foi exportada e fechada. A comparação independente executa o
ARM original completo com helpers reais e arenas disjuntas. A evidência fica
em `research/private/player-input-filter-v1/`; o acumulador tem manifesto
próprio em `research/private/player-update-accumulator-v1/manifest.json`.
O launcher publicado continua 0.17.0 e permanece aberto durante os testes.

A comparação passou em 309 chamadas ARM completas, com 34 controles de
leitura/código/limite. O núcleo foi comparado em todos os casos; 304 também
passaram pelo wrapper canônico. Os cinco restantes incluem aliases
obrigatórios desconhecidos, que não foram artificialmente preenchidos para
ampliar essa cobertura. Foram 279.064 verificações por perfil normal e
ASan/UBSan/float-cast-overflow, além de 70 negativos de processo. As 60.172
posições conhecidas da saída foram comparadas, com 392 posições permanecendo
desconhecidas; a referência conserva as arenas completas separadamente.

O acumulador tem 3.634 chamadas ARM: 3.325 saídas finitas exatas e 309 casos
explicitamente rejeitados pelo perfil numérico nativo. Passou em 147.787
verificações por perfil normal e sanitizado, com 54 negativos de processo.
A sequência no Player persistente passou em 75.814 verificações por perfil,
com oito negativos. Inclui preservação da pose/relógio, bytes parcialmente
conhecidos, falhas tardias sem publicação e falha do render seguida de retry.
Essas verificações de composição não são uma execução completa do chamador ARM.
Os sanitizadores instrumentaram 55 unidades do projeto; SDL e bibliotecas do
sistema ficaram fora da instrumentação. O teste específico de armazenamento
passou em 607 verificações por perfil.

A suíte completa passou em **117/117 CTests no Mac**, em 46,71 s. O build
incremental com testes levou 92,21 s; a compilação cruzada Windows levou
36,87 s. Ambos concluíram sem avisos após corrigir a inicialização explícita
do novo campo opcional no dono canônico. Os primeiros builds com avisos foram
retidos como histórico. Não houve execução no Windows. Os builds rodaram
em paralelo; CPU dos filhos está nos relatórios, mas pico de memória da
família de processos e custo de tokens não foram medidos. Esses tempos não
medem o desempenho do jogo.

O manifesto consolidado é
`research/private/player-input-filter-v1/manifest.json`. Ele conserva a
validação histórica desta etapa. A consulta `5b8f14 → 5ba61c`, responsável
por fornecer PW5a8, e sua integração posterior estão documentadas na
[etapa seguinte](player-world-lookup.md).
