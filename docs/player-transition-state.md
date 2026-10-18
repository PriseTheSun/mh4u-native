# Fronteira da transição de ações

Continuação executável: [player-transition-runtime.md](player-transition-runtime.md)
documenta o perfil numérico nativo e sua comparação com a função ARM completa
em estados explicitamente delimitados. Esta página conserva a investigação
estática que identificou suas dependências; o controlador completo continua aberto.

`0xb1c050` faz mais que escrever `act_st` e `act_no`. A leitura estática
identificou alteração de estado global, limpeza de campos, uma chamada virtual
e uma chamada final a outra rotina. Por isso o motor ainda não substitui essa
função por uma atribuição de dois campos nem considera um pedido de animação
equivalente à transição completa.

Esta é análise estática do ELF local de SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
O inventário em `research/private/player-transition-dependencies/static-dependencies.json`
retém o comando e hash do LLVM, o intervalo de bytes e a tabela virtual. Não
houve execução da transição completa.

## Ordem do gate inicial e das escritas

Antes dessas escritas, `0xb1c06c` chama incondicionalmente `0xd04924`. Se ele
retorna não zero, lê o bit antigo `pw+0xf0 & 0x10000000` e conserva o resultado
em `r5`. Essa informação é usada em `0xb1c27c–0xb1c280` para alterar o flag 1.
Mover o gate para depois da limpeza perderia o estado anterior.

Com `pbw=*(player+0xe30)` e `pw=*(player+0xa29c)`, a função copia `pw+0xf0/0xf4`
para `pw+0xf8/0xfc` antes de zerar os dois primeiros campos. Copia os estados
anteriores de `pbw+0x371/0x372` para `+0x377/+0x378` e grava os novos valores
com larguras de 8 e 16 bits. Também zera `pbw+5/+6/+7` e o temporizador
`pw+0x4c`. Os argumentos inteiros completos continuam sendo usados em testes
posteriores; truncar os argumentos antes de executar esses testes seria uma
mudança de comportamento.

Em ambos os ramos do flag `0x20`, chama `0x2b555c(*(0xfb6b7c),1)`.
O assembly desse helper lê, transforma e grava o `u16` em `global+0x22`,
deixando o valor em `r0`. O pseudocódigo o apresenta como `void`; a leitura de
`r0` pelo caller e as instruções do helper estabelecem a dependência real.
Com flag `0x20`, conserva o byte baixo anterior de `pw+0xbc` e usa o byte alto
retornado; sem o flag, grava o resultado inteiro de 16 bits. A finalidade
desse estado global não recebeu um nome funcional.

## Helpers delimitados pelo REA

As respostas abaixo pertencem à sessão local `1792283675798`. Seus arquivos
originais estão em `research/private/rea-session-1792283675798/`.

| Helper | Observação estática | Respostas pseudo/assembly |
| --- | --- | --- |
| `0xd04924` | Se `global+0x2b==2`, exige `pbw+0x30!=3` e `pbw+0x33==0`; caso contrário compara o byte **com sinal** de `global+0x2f` com o byte **sem sinal** de `pbw+0x33`. Retorna 0 ou 1. | 034/035 |
| `0xd0473c` | Classifica o byte `pl_type` em `pbw+0x31`. Argumento 0 aceita `{4,5,6,15}`; 2 aceita `{10}`; qualquer outro argumento aceita `{4,5,6,10,15}`. | 036/037 |
| `0x2b555c` | Atualiza um `u16` global e retorna os mesmos bits em `r0`; o caller conserva ou substitui parte de `pw+0xbc`. | 038/039 |
| `0xb04e40` | Soma `0x370` ao argumento e, se o resultado não é zero, grava os bits de `s0` em `player+0x3b8`. No caller, `s0` recebe zero antes da chamada. | 040/041 |
| `0x10b778` | Limpa bytes da faixa fornecida; o caller passa `pw+0x10a` e comprimento `0x30`. O ramo de endereço não alinhado encaminha para `0x2fef54`, ainda necessário para uma execução completa. | 042/043 |

Não se atribuiu significado de arma, dispositivo ou jogador local aos valores
numéricos apenas com base nesses predicados.

## Dependências que impedem uma substituição parcial

Mesmo com gates opcionais desativados, a função chama `0xb151b4(player,255,255)`,
a virtual `+0x134` e termina com `0xb6cdf0(player,0x20000000)`. Na tabela concreta
de `uPlayer`, o slot em `0xe6bdd8` contém `0xb31c64`, cujo corpo é apenas
`BX LR`; a função vizinha em `0xb31c68` não pertence a esse corpo. Outras tabelas
podem ter outra implementação. O tailcall `0xb6cdf0` faz OR do argumento em
`pw+0xf0` quando o bit 31 está limpo, como neste caller. Com bit 31 definido,
remove esse bit e faz OR em `pw+0xf4`. As respostas REA 078/079, 083 e 085
retêm esses helpers e o slot. Os demais ramos alcançam helpers adicionais, incluindo
`0xb33778`, `0xca550c`, `0xc017f0`, `0xcbf7b0`, `0x33ecc8` e `0xb66f0c`.

Há uma prioridade de consulta observável no ramo angular: consulta o setor
`0x800` antes de `0x400`. Se o primeiro é verdadeiro, soma um deslocamento a
`pw+0xb0` e não consulta o segundo; caso contrário, o segundo pode subtrair o
mesmo deslocamento. Isso não estabelece uma prioridade universal de direção
para todos os callers.

O próximo ensaio dessa fronteira deve executar os helpers reais com memória
inicializada explicitamente, incluindo o estado global e a tabela virtual
concreta. Substituir essas chamadas por no-ops não validaria a transição. O
módulo [player-input-runtime.md](player-input-runtime.md) cobre apenas os
classificadores numéricos independentes.
