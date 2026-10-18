# Semântica dos atributos de colisão

Os índices u16 em `face+0x12` e `pair+8` apontam para a tabela global de registros
SBC de 32 bytes. O filtro da classe derivada consome os u32 em atributo+0 e +8,
com uma regra exata abaixo. Ainda não estão demonstrados nomes físicos para
os bits nem um campo de material. O kernel geométrico não foi alterado.

## Evidência preservada

O ponto de partida foi o bundle local
`research/private/rea-motion-collision-evidence.json`, 48 registros, e
[rea-findings.md](rea-findings.md). A análise é estática sobre o ELF BSS de
SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`;
não houve execução do jogo. Extratos selecionados, hashes e limitações foram
preservados em `research/private/collision-semantics/bundle-selection.json`.

A investigação seguinte gerou
`research/private/rea-collision-semantics-evidence.json`, **50 registros**, e
`research/private/rea-collision-semantics.snapshot.json`. A sessão foi fechada.
O manifesto `research/private/collision-semantics/followup-selection.json`
registra IDs, parâmetros, hashes, limitações e arquivos completos por operação.
O SHA-256 do novo bundle é
`53eaacb05ef04508664a98aa5d53ef4c0cd6ae562c66c1aef5c00ec570acddad`.
Foi usado Ghidra 12.1.4, ARM little endian v6, heap 3 GiB e o mesmo perfil
focado, com `Decompiler Switch Analysis` desativado. Nenhum código do jogo
foi executado.

| Evidência | Fato sustentado |
| --- | --- |
| `ev_01511f30730443caddfd86a170c3c2bd77261ffc9c5b2a974172ffbb5c47e2a6` — loader 0x16014c | Lê a tabela de atributos com stride 32 em objeto+0xc0, após os vértices |
| `ev_f9126f947889e85fb0e7110e96cdfe6b20860f269c228d343d0d166f319ee71e` — 0x16c380 | Resolve grupo, face, base de vértices e ponteiro ao atributo |
| `ev_3cb8f29050addb53b6df299a354ec6d4c928af582b4e3a87ad0fb4d0c1bbb6e4` — assembly 0x16c380 | Confirma `LDRH face+0x12`, multiplicação por 32 e armazenamento do ponteiro em contato+0x40 |
| `ev_2f7f2f01ec960941f0fb071b2717b674e2b20279e20031a6d0ef7b0a5771ce56` — 0x1872a8 | Consome contato, calcula AABBs de faces/pares, filtra objetos e encaminha consultas/callbacks |

O endereço solicitado deve ser resolvido com `resolve_containing_procedure`
antes de consultar pseudocódigo por `FUN_nome`. A sessão anterior documentou
endereços não reconhecidos interpretados como offsets de arquivo; esses
resultados não sustentam os comportamentos das funções originalmente pedidas.

## O que o bundle inicial acrescenta

`FUN_0016c380` escreve uma estrutura de contato resolvida:

| Offset no contato | Valor |
| --- | --- |
| +0x34 | Ponteiro ao grupo de 72 bytes |
| +0x38 | Ponteiro à face, com início do intervalo de faces do grupo aplicado |
| +0x3c | Base do intervalo de vértices do grupo |
| +0x40 | Base global de atributos + `face.u16(+0x12)*32` |

A função apenas resolve o ponteiro de atributo nesse caminho; não interpreta
os bits da linha. Valores adicionais em contato+0x48/+0x49 e
+0x4c/+0x50/+0x54/+0x58 vêm de estado do wrapper de runtime ou de uma estrutura
por grupo de 64 bytes, com fallback. Não vêm automaticamente do registro SBC.
Seu significado permanece desconhecido; os callbacks selecionados por esse
estado não podem receber nomes de piso/parede apenas por proximidade no código.

`FUN_001872a8` recebe esse contato por `param_3+0xe8`. O caminho com ponteiro
de grupo não nulo acessa pares em `resource+0xc4`, usando início do grupo em
grupo+0x2c, stride 10 e índice local do par. A primeira referência u16 é uma
face local; a segunda também é local ou `0xffff`. A função usa somente a
primeira face quando a segunda é `0xffff`; caso contrário, calcula limites
com vértices de ambas. Essa observação confirma estaticamente a interpretação
de pares que antes tinha apenas corroboração de bytes e do schema público.

Nesse mesmo caminho, o código testa campos do **wrapper de runtime**, incluindo
byte +5, campos +0x10/+0x14 contra duas máscaras vindas da consulta e comparações
de identidades/exclusões. Essas máscaras não são leituras da tabela SBC de
32 bytes. Outros ramos testam valores como `&7`, `&0x400`, `&4` e `&8` em objetos
ou descritores distintos; o bundle não demonstra que sejam flags de piso ou
parede da linha de atributo. Confundir a base do ponteiro nesses testes
produziria uma classificação sem evidência.

## Consumidor do atributo: classe base e override

`FUN_0018a9f4` resolve `pair.u16(+8)*32 + resource.attr_base`, armazena o
ponteiro em contato+0x40 e chama o slot virtual +0x2c do gerenciador. Os
argumentos efetivos são gerenciador em r0, `queryFilter.u32(+0x0c)` em r1 e
ponteiro ao atributo em r2. O filtro vem de `queryContext+0xd8`.
O pseudocódigo omite r2 nessa chamada; a assembly confirma sua continuidade
em 0x18aad8–0x18ab1c e no ramo de árvore tipo 2 em 0x18ab58–0x18ab64.

É necessário seguir a classe concreta. `FUN_0019c944` instala vtable
0xe5b82c e publica o gerenciador em `DAT_010574f4`. Seu caller
`FUN_002bbd08` **substitui** essa vtable por 0xe5e784. O slot +0x2c da base
aponta para 0x175318; o mesmo slot na derivada aponta para **0x2b9e18**.
Isso é uma relação de construção observada no código, não uma captura do
tipo dinâmico em execução. A regra da base não deve ser aplicada ao override.

| Evidência nova | Fato sustentado |
| --- | --- |
| `ev_02df16dc583f030edf0a6a2c11735a6e0c7f9cf803d9684192c83f5a971eb8eb` e `ev_61112ed9d1bc5bc9b7654ee0764795ec7ffe7ba4a65076d57cb7c85480370529` | Pseudocódigo e assembly de 0x18a9f4; índice do par e argumentos do filtro |
| `ev_eb9a9ee6b9344386be8125a617958e2988f0f37fbd7cc1b753690fa39375a434` | Construção base 0x19c944 e publicação do singleton |
| `ev_12279df00f34ff99a447b4c93f5f843621aa51633cc7b7390ad4a6ace63bc933` | Construção derivada 0x2bbd08 e substituição da vtable |
| `ev_f1c0d7d1aabae88567c7e834229cbcd50483fa3d50b17bd13d9703fa199eca20` e `ev_6162b02a3268e22e4bf17bd0d73f18ca38319d4d70163e06c542ac5b53e882ac` | Bytes das duas vtables e destinos do slot +0x2c |
| `ev_4377520a39e07050f7e685b1ee55ab0c95688e4b743a3f92e497ce6f826e7c16` e `ev_7b82fc35deecee35ca9f6898860469d8effb8faa40b0a3fce7973b9ebb03fc2e` | Bytes dos filtros 0x175318 e 0x2b9e18 |

Ghidra respondeu `not_in_procedure` para os dois filtros. Portanto, não há
pseudocódigo Ghidra válido atribuído a eles. LLVM, configurado explicitamente
com `--triple=armv6-none-eabi`, decodificou os intervalos 0x175318–0x17537b e
0x2b9e18–0x2b9e77 do mesmo ELF. No segundo, o u32 em 0x2b9e74 é o literal
`0x1004` acessado por PC; não é uma instrução alcançada pelo fluxo mostrado.
As saídas completas ficam privadas em `collision-semantics/*-llvm-assembly.txt`.

### Regra da classe derivada

Definições: `Q = queryFilter.u32(+0x0c)`, `A0 = attribute.u32(+0)` e
`A8 = attribute.u32(+8)`. Todos os valores são u32 little endian.

1. Se `(Q & A0 & 0xffff) != 0`, aceita exatamente quando
   `(Q & A0 & 0x00ff0000) != 0`. Esse ramo retorna antes de testar A8.
2. Caso contrário, rejeita se Q não contém `0x01000000` e A8 contém algum
   dos bits `0x1004`.
3. Depois rejeita se Q não contém `0x02000000` e A8 contém `0x10`.
4. Se nenhum desses casos rejeitar, aceita.

| Campo SBC | Uso confirmado nesse filtro | O que não está estabelecido |
| --- | --- | --- |
| atributo+0, bits 0–15 | Interseção com Q escolhe o primeiro ramo | Nomes como piso, parede, escalável ou água |
| atributo+0, bits 16–23 | Interseção com Q permite o contato no primeiro ramo | Nome físico das categorias ou permissões |
| atributo+8, bits 2 e 12 | Rejeição no segundo ramo, dispensada por Q bit 24 | O que distingue esses dois bits |
| atributo+8, bit 4 | Rejeição no segundo ramo, dispensada por Q bit 25 | Nome físico da propriedade |
| atributo+4 | Não é lido por esse filtro | ID de material, som, efeito ou atrito |

Os bits 24–31 de A0 e 26–31 de Q não participam deste override. Isso não
significa que sejam ignorados por todos os consumidores. Em particular, o
`0x02000000` recorrente em A0 nas amostras não é, por si só, critério de
aceitação aqui. Não confundir Q bit 25 com A0 bit 25: são campos diferentes.

A base 0x175318 faz outra coisa: `overlap = (Q & A0 & 0x3fffffff) != 0` e
aceita `overlap XOR Q.bit30 XOR A0.bit30`; ignora bit 31 e não lê A8. Ela é
documentada para preservar a distinção de classes, sem adotá-la como regra do
controlador.

O script privado `derive_filters.py` verificou quatro intervalos de bytes REA
contra os segmentos do ELF, enumerou as oito combinações booleanas da base e
as 128 combinações independentes do override, incluindo invariância dos bits
não lidos. Isso verifica a consistência da derivação das ramificações; não é
um teste de comportamento do jogo nem execução do ARM. Relatório:
`research/private/collision-semantics/filter-derivation.json`.

### A aceitação ainda passa por callbacks

`FUN_00194388` seleciona uma ou duas faces do par e chama o filtro opcional
do solicitante em `queryFilter+0x30/+0x34/+0x38`, com dados em +0x3c, antes
dos callbacks geométricos do contexto. O helper `FUN_00cd2f04` retorna 1
quando esse filtro não existe; esse retorno só ficou explícito na assembly,
pois o pseudocódigo o declarou `void`.

Assim, passar pela máscara não prova que o contato é usado como suporte.
Existem decisões adicionais do solicitante e do cálculo geométrico. Evidências:
`ev_b8002f49adf0ee757fc53fa6c0a54cc3610f10042ea7b9858f16ed56b1af106b`
e `ev_7d59ab8425d91a9afadd9b2877fc679a0bc84153eab90aeda1e2f56b6b4d3644`.

## Confronto com os bytes

`research/private/collision-semantics/inspect_attributes.py` reutiliza o parser
congelado, verifica o SHA-256 de cada SBC contra sua proveniência e produz
`sbc-attribute-observations.json`. Foram examinados **sete arquivos, 41 registros
de atributo e 5.834 faces**. Esses dados são derivações locais, não novos
Evidence emitidos pelo REA.

Observações úteis para evitar hipóteses incorretas:

- Em `v00a01-a-g00`, IDs 0 e 1 apontam para registros diferentes e ambos
  cobrem faces voltadas para cima. Em `v00a01-a-w00`, ID 0 cobre 90 faces
  verticais. Portanto, o número do índice não identifica um tipo físico global.
- O primeiro u32 é `0x02000000` em registros de `v00a01-a-g00` e do retângulo
  `m08a00d-a-cam`; esse valor sozinho não distingue os dois usos sugeridos pelos
  nomes dos recursos.
- A hipótese “bit 0 do primeiro u32 significa parede” não cobre as amostras:
  `m13a12d-a-w00-shell` tem faces verticais com `0x02000004`, enquanto
  `m15a08d-a-g00` contém faces com componente y positiva usando `0x0200007b`.
  Sem consumidor, isso não autoriza uma hipótese substituta.
- O recurso com sufixo `m15a08d-a-g00` contém normais y negativas; o sufixo
  tampouco garante que todos os triângulos sejam superfícies caminháveis.
- Nas 41 linhas, +0x0c e +0x10 são zero. +0x14 é constante dentro de cada
  arquivo; +0x18/+0x1c repetem `0x0496ff68`/`0x0496fec0`. São apenas padrões de
  bytes: não se afirma que sejam strings, ponteiros utilizáveis, coeficientes
  ou propriedades de material.
- Todos os pares examinados têm o mesmo índice de atributo das faces a que
  se referem. A correlação não prova igualdade em todo o jogo, mas o uso do
  índice do par como referência global agora está confirmado por 0x18a9f4.

O schema SBC de outros jogos usado como referência de formato não resolve
essas lacunas. Nomes como `floor`, `wall`, `airwall` ou `jp_path` nessa referência
não devem ser transferidos para MH4U sem consumidor ou outra evidência própria.

## Prioridade para o controlador

Antes de usar atributos no controlador do caçador, é necessário estabelecer:
quais campos aceitam/rejeitam um contato, como o solicitante distingue contatos
de suporte e laterais, e de onde vêm máscaras/limites que selecionam esses
contatos. Depois disso, mapear IDs de material a efeitos exige consumidores
específicos. A consulta geométrica nativa pode retornar posição/normal/face,
mas não deve apresentar uma regra genérica de inclinação ou gravidade como
comportamento original.

Foi encontrado um caminho vertical em `FUN_001a91cc`: constrói dois pontos
com x/z iguais e y diferentes, passa o filtro em `P+0x88` à consulta, calcula
uma diferença vertical do resultado e pode ajustar a coordenada y fornecida.
A máscara Q desse caminho fica em **P+0x94**. O caminho de modo 3 usa
`FUN_00184000 → FUN_0018d42c → FUN_001872a8`. Os callbacks de face são
selecionados por `queryFilter.byte(+0x4c)`; no ramo com quarto argumento zero,
os destinos lidos em 0xe3b21c/0xe3b224 são 0x18f43c e 0x18149c.

Essa é uma pista de consulta de suporte, mas ainda não identifica o controlador
de locomoção do caçador. O caller imediato `FUN_001aa228` resolve IDs de juntas,
usa dois comprimentos e trigonometria de uma cadeia de dois segmentos. A
interpretação de ajuste de esqueleto/IK é uma **inferência**, e essa cadeia não
autoriza transpor a regra para o movimento do jogador. Evidências principais:
`ev_71082a800448475461438b1585d4fb58adefc47407cf4ff7e41d9b3d06145f07`
e `ev_16d9b662417362254083ea1b51bf57e5f918406ca2368142480d436140575a1d`.

O próximo passo mínimo é identificar **quem inicializa P+0x88/P+0x94 do objeto
usado em FUN_001a91cc**, quais máscaras/callbacks são escolhidos e se esse
objeto pertence ao suporte de esqueleto ou à locomoção. Para a decisão
geométrica imediata, seguir os callbacks 0x18149c e
`FUN_0018f43c → 0xd8cf0c`, primeiro resolvendo os endereços. Material requer
uma pergunta separada: um consumidor comprovado de `contact.attribute+4`
que faça lookup de efeito/som/propriedade. Os bundles atuais não o mostram.
