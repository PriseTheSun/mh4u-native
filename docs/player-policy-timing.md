# Consulta de tempo da política de ação

O dono `player_policy_timing::query` reconstrói a cadeia completa
`b09da8 → b064fc` com a saída opcional de fração ausente, exatamente a forma
chamada pelo intérprete `b0d0e4`. Aceita os modos 0–12 e preserva o retorno
padrão para os demais valores. Ainda não executa o intérprete, a prioridade
dos registros, os latches, os gates de contexto ou uma ação do jogador.

O contrato toma emprestados a primeira `motion_request::Layer` e os
`motion_command::Fields`. Os campos da animação em `player+240` e os words
`d48..d60` continuam nesses donos; não há uma segunda cópia persistente do
relógio. O delta em `model+14` é opcional e exigido apenas pelo ramo que o lê.

A consulta preserva a preparação dos dois limites, o período de interpolação,
a volta da animação, o ajuste de 0,0005 antes da conversão para inteiro e as
inclusões/exclusões de borda de cada modo. A conversão ARM para int32 trunca
com saturação; não foi substituída por um cast indefinido fora do intervalo.
A multiplicação e subtração de VMLS arredondam separadamente em binary32.

O perfil nativo exige arredondamento ao mais próximo, underflow gradual e
operandos/intermediários consumidos finitos. Campos ignorados podem conservar
bits não finitos; por exemplo, um retorno durante interpolação pode ignorar
os limites `d58/d5c`. Falhas conhecidas retornam erro e a consulta não escreve
estado. Saída de fração presente, FPSCR diferente de zero, exceções VFP e
comportamento aritmético com NaN/infinito não são reivindicados.

## Evidência e comparação

Fonte ELF SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
A sessão REA compartilhada `1792303785054` reteve os pseudocódigos, assembly e
readbacks. `research/private/player-policy-timing-v1/source-observed/` verifica
os 980 bytes dos dois readbacks diretamente contra o ELF. O corpo Ghidra de
`b064fc` é descontínuo; o span inteiro e a desassemblagem LLVM conservam os
dois jump tables. Seus bytes não são permitidos como instruções no oracle.

`arm-complete/validation.json` registra **28.056 retornos originais**,
1.584.911 instruções e 15 controles negativos/de leitura preguiçosa. A execução
usa Unicorn 2.1.4/ARM11MPCore/FPSCR0 exclusivamente na pesquisa privada, com
campos sintéticos explícitos, guardas de memória/leitura/escrita/código e
restauração dos registradores preservados. Nenhum resultado de helper é
substituído; não há stub, alteração de PC, patch do executável ou runtime 3DS
no aplicativo nativo.

O arquivo `arm-complete/oracle.bin`, SHA-256
`9b7b9bbd91e2ba797c3ec89b0ec7cccc4940e621f0621513dd70bd5940c48a56`, contém
entradas e retornos observados. Os testes C++ verificaram **28.068 checks por
execução**, nas compilações normal e ASan/UBSan estrito, sem divergências nem
saída de erro. Incluem todos os modos, os retornos padrão, saturação de inteiro,
limites do ajuste epsilon, zeros, subnormais, bits de máscara e campos ignorados.
A regressão normal do CMake usa esse mesmo oracle fixado independentemente.

O relatório nativo conserva fontes e todos os headers transitivos usados pelo
compilador. Os tempos são medições locais de cada subprocesso, sem comparação
de desempenho anterior. Pico do relatório é o máximo cumulativo de filhos,
não uma amostra de toda a família de processos; tokens não foram medidos.

## Continuação

A instalação e os registros da tabela estão em
[player-policy-table-runtime.md](player-policy-table-runtime.md). O predicado
de entrada já tem dono próprio. Antes de conectar os três ao Player, faltam a
ordem completa do intérprete, os efeitos dos contextos e a migração do bloco
`pw+100..148`, hoje também representado em `player_transition::PwFields`.
Os helpers de contexto consultados na mesma sessão ficaram retidos para essa
continuação. A tabela real de 35 registros deve ser percorrida inteira; seus
últimos três registros não autorizam um atalho de “primeiro predicado verdadeiro”.
