# Produção da entrada do jogador — recorte estático v1

Este recorte recupera a cadeia que produz `pbw+0x3c0..0x3c7` e parte da
composição de `pbw+0x3dc` no executável europeu fornecido. A associação dos dois
pares a dispositivos físicos e a ordem completa do agendador continuam abertas.
Não autoriza converter eixos SDL diretamente para esses campos, nem habilitar
um controlador original a partir de nomes presumidos de ações.

As afirmações abaixo são derivadas de código original, sem execução integral
do jogo ou ensaio ARM nesta investigação. A comparação de 360 ângulos inteiros
é uma avaliação independente de operações inteiras em Python, não execução ARM.

## Identidade e retenção

- ELF: `research/private/mh4u-analysis-bss.elf`, SHA-256
  `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
- Sessão REA/Ghidra focused 3 GiB: `1792283675798`, exportada e encerrada.
- Respostas completas: `research/private/rea-session-1792283675798/`.
- Bundle: `research/private/rea-player-input-production-evidence.json`.
- Derivações, script de retenção, índice de Evidence, bytes privados e readback:
  `research/private/player-input-production/`.
- `retain_evidence.py` compara cada `read_bytes` ao segmento PT_LOAD correto do
  ELF fixado: **46 intervalos, 14.912 bytes, todos iguais**. Não confunde offset
  de arquivo com endereço virtual. O manifesto fixa os arquivos do recorte.

REA não reconheceu as funções em `0xaf8ae8`, `0x94be10` e o retorno trivial em
`0xb31c64`. Nessas fronteiras foram usados bytes REA e disassembly LLVM
`armv6-none-eabi,+vfp2`, identificados como derivação. Uma lista vazia de callers
não prova ausência de chamadas virtuais. Endereços encontrados por varredura de
palavras/BL alinhados são candidatos até a verificação do contexto.

## Donos e cadeia dos pares numéricos

`pbw = *(player+0xe30)` e `pw = *(player+0xa29c)` seguem os nomes de trabalho de
[player-action-controller.md](player-action-controller.md). A global
`*(0x10572d8)` aponta para o objeto de entrada, chamado `S` abaixo.

O construtor básico `0x5b86a0` publica `S` nessa global. A reflexão identifica
`sPad` e sua derivação `sMhPad`: inicializador `0xd1ee74`, nome em `0xd1eec8`,
descriptor `0x10b5278`, pai `0x10e8728`, tamanho `0x448`. A factory `0xcfff48`
e o construtor `0x694bd8` chamam o construtor básico e instalam a vtable
`0xe658cc`; seu slot `+0x1c` aponta para `0x694644`.

`0x694644` primeiro chama `0x5b814c`, a atualização do componente básico. Esta
chama `0x5b6fa0` para obter amostra, seguida de `0x5b8328` para transformar os
valores. Quando a aquisição falha, limpa os inteiros/eixos e palavras de botões
em `S+0x8c..0xb8` e desativa bytes de estado. O caminho depende dos bytes
`S+0x30` e `S+0x33`; não se presume conexão ativa.

No ramo normal de `0x4855d0`, usado por `0x5b6fa0`, `0x47cc9c` devolve o
buffer `0x11177a0`. `0x571fcc` lê um anel de oito registros de `0x18` bytes com
índice/tempos explícitos; o caller pede uma amostra e considera sucesso somente
quando a contagem devolvida é positiva. Há outro ramo (`0x485cf8`) quando
`0x485b28` é falso. Não foi identificado fisicamente esse ramo alternativo.

| Par | Amostra após filtros | Inteiros de `sPad` | `sMhPad` ângulo/magnitude | Destino `pbw` |
|---|---|---|---|---|
| 0 | `s16+0`, `s16+2` | `S+0xac`, `S+0xb0` | `u16+0x32c`, `u16+0x338` | `+0x3c0`, `+0x3c4` |
| 1 | `s16+4`, `s16+6` | `S+0xa4`, `S+0xa8` | `u16+0x32e`, `u16+0x33a` | `+0x3c2`, `+0x3c6` |

O buffer guarda o par 0 em `base+0x40+24*i`, par 1 em
`base+0x44+24*i`, e botões em `base+0x30+24*i`. A ligação desses slots ao
Circle Pad, C-stick ou acessório ainda **não está provada**. A presença de dois
pares, seus intervalos e a aparência de poses não bastam para essa atribuição.

Antes da conversão para inteiros, `0x4855d0` aplica `0x4859c8` separadamente a
ambos os pares, com configuração em `S+0x230` e `S+0x254`. A função seleciona
`0x571a64`, `0x571960` ou `0x571b48` por modo `0/1/2`. Depois, `0x5b8328` usa
`0x4858f8`, multiplica seu resultado float32 por `32767.0` e converte para
inteiro com truncamento. Esse helper usa uma diferença de parâmetros de
calibração ou `105`, conforme o modo. Os parâmetros, sua inicialização e os
helpers de calibração não foram fechados. Portanto `[-32767,32767]` não foi
adotado como precondição universal nem como conversão host já validada.

Evidence central: 054/055 (`0x694644`, pseudo
`ev_910559ce9e34b8477a853f1cb9ba56b283fa09727a08212f1c5906d2ab4e91da`),
068/069 (`0x5b814c`), 086–089 (`0x5b6fa0`, `0x5b8328`), 094–097
(`0x4855d0`, `0x4858f8`), 103/104 (`0x571fcc`), 109/110 (`0x4859c8`).

## Conversão derivada em `0x694644`

No ramo habilitado (`S.byte30 != 0`, `S.byte33 != 0` e
`(*(0xfb6b7c)).byte2f7c == 0`), para cada par:

1. Divide cada inteiro por 256 com truncamento em direção a zero; armazena o
   byte baixo e o relê como signed8. Essa redução é observada, não uma operação
   saturante inventada no host.
2. Se ambos são zero, usa ângulo zero. Caso contrário chama `0xd12724` com
   `s0 = segundo componente`, `s1 = primeiro componente`. O helper tem a forma
   de `atan2`; a identificação matemática é uma derivação do código, não uma
   prova de orientação física. Se o resultado é negativo, adiciona o float32
   `0x40c90fdb` (aproximação de `2π`).
3. Multiplica, nessa ordem, pelos float32 `0x3e22f983` e `0x43b40000` e faz
   `VCVT.u32.f32` com truncamento. Guarda graus inteiros em `S+0x334+2*i`.
4. Calcula `trunc(sqrt(x*x+y*y))`, limita a `127`; se o resultado é menor que
   `30`, zera tanto graus quanto magnitude. Guarda a magnitude temporária em
   `S+0x338+2*i` e depois a copia para `S+0x330+2*i`.
5. Converte os graus inteiros para o círculo de 16 bits em `S+0x32c+2*i`.
   `SMLAL`, `ASR` e correção de sinal equivalem a
   `floor(graus*65536/360)` para os 360 graus inteiros examinados. Não é uma
   conversão angular contínua: os graus já foram quantizados.
6. Para magnitude menor que `37`, zera o setor de oito direções. Nos demais
   casos usa tabela `0xfb82c0` indexada por `trunc(graus * float32(2/45))`.

O item 4 merece atenção: os consumidores do jogador copiam **`S+0x338/33a`**,
não os temporários presumidos a partir do layout. Ambos recebem o mesmo valor
neste produtor. O limiar `50` observado nos seletores do jogador é posterior à
zona morta `30`; não substitui esse filtro.

O produto de duas constantes float32 e a aproximação do helper angular podem
afetar fronteiras. Não foi executado um oracle de ponto flutuante deste trecho.
`angle-integer-check.json` cobre apenas a etapa inteira graus→círculo, incluindo
0 e 359, preservando a multiplicação assinada e o acumulador de 64 bits.

## Aquisição e composição em `0xb1c750`

No modo `param_2 == 0`, o produtor:

- copia os campos anteriores (`3d0→3cc`, `3f4..400→3e4..3f0`), zera
  `3d4/3d8/3dc/3e0` e atualiza `451` por `0x2b1dcc`;
- retorna cedo se `0xd04924(player)` é zero;
- se `pw.float13c > 0` ou `0x19faf4` é não zero, limpa `0xc4` bytes a partir
  de `pbw+0x3a0` e retorna;
- copia os pares da tabela acima, palavras de botões e estado anterior/atual
  de `sMhPad`; `pbw+3a0` é derivado do held de `S+30c` e `pbw+3a4` do rising
  de `S+310` (`current & ~previous`);
- num gate adicional (`0x6b398c(...,5)` ou `pbw.float464 > 0`), limpa o bloco,
  mas recopia ângulos, magnitudes e palavras direcionais `3b8/3ba/3bc`.

Depois processa oito temporizadores, chama `b288b4`, `b27928`, `b3cac0`,
`b3c5c4`, aplica outros gates, chama `b1631c` e virtual `+0xc0` e processa
mais condições. No modo `param_2 != 0`, pula a aquisição/limpeza inicial e
processa o estado existente. Os modos não são intercambiáveis.

As seguintes são **escritas diretas observadas**, não um contrato completo de
todos os produtores de `3dc`:

| OR em `pbw+3dc` | Predicado local |
|---|---|
| `8` | `held & 0x3ff != 0`, em `b1c750` |
| `0x400` | `rising & 0x40 != 0`, em `b1631c` |
| `2` ou `4` | `held & 0x40` presente ou ausente, respectivamente |
| `0x10` | `rising & 1`, ou conversão condicional de `rising & 0x8000`, ou bit1 de `pbw+454` |
| `0x800` | `pw.byte59d != 0` |
| `0x80` | `rising & 0x20 != 0` |
| `0x2000` | `rising & 0x80 != 0` |
| `0x100` | gate ao fim de `b1631c`, dependente de `3d4`, `b07e80`, `pl_type` e held |

Chamadas auxiliares podem modificar o estado. O caller ainda pode remover
bits, inclusive `0x800/0x100`. `0xb37420`, chamado no ramo rising `0x40`, não
foi reduzido aqui. Não se nomeiam esses bits como botões físicos ou ações de
gameplay. A lógica de combinações em `452/454/458/45c/45e` contém janela de
tempo e prioridades; um OR independente por botão não a reproduziria.

Evidence: 013/014 `b1c750`, pseudo
`ev_da469173be107bd09f2d8e670b9b88c890e324ce8dd7b43dcfed7b6d4e8c8e99`;
010/011 `b1631c`; readbacks 126/127. A tabela física de nomes dos botões não
foi recuperada; `0xfb8260/82a0` preserva remapeamentos numéricos e opções.

## Ordem recuperada e fronteira do agendador

O descriptor `0x10b5358` é registrado com nome `sPlayer` em `0xd2dc14`; o
getter `0xd04120` retorna esse descriptor. Sua vtable `0xe6b8d0` contém
`0xaf8ae8` em `+0x1c`. Os bytes mostram um loop por jogadores. No ramo
`pbw.byte30 == 0`, a ordem local é:

`b16bc0(player) → b1c750(player,0) → d04924 → b341e0 → b14d44 → b1ba94`.

Os dois últimos só são chamados se os gates anteriores permitirem. O ramo
`pbw.byte30 == 1` chama `2c4408 → 2c5340`. Não é correto alimentar todos os
jogadores pelo primeiro ramo. O método não chama diretamente o update virtual
`+0x24` do jogador.

Separadamente, a vtable do `uPlayer` tem `+0x24 → 0xb48230`. Esse método só
avança quando `pbw.byte93d4 != 0`; inicializa via `b312dc` quando `byte4 == 0`
e, no estado `byte4 == 1`, chama `b1407c` ou `b316cc` conforme `1a1eb8`.
`b1407c` contém as chamadas ao dispatcher `b66a20`, com possibilidade de uma
segunda chamada condicionada por `pbw.byte376`.

Não foi demonstrado o scheduler que ordena `sMhPad+1c`, `sPlayer+1c` e
`uPlayer+24`, nem se cada um roda uma vez por frame. O incremento temporal de
`sMhPad` vem de `(*(0x105718c))+0x5c`; não foi igualado a delta SDL, tick de
rede ou frequência fixa.

Evidence: 048/049 (`b48230`), 016/017 (`b1407c`), bytes145/146/147/148,
153/155 e derivação privada `manager-af8ae8.asm`. O novo recorte não altera
as conclusões de [player-input-state.md](player-input-state.md).

## Consultas auxiliares concluídas

**`model+0x220` é contagem de juntas.** O loader de `rModel`, `0x6806c0`, lê
header MOD de `0x80` bytes, valida magic/version `MOD/0xe6`, usa o `u16` em
header+6 para alocar registros de `0x18` bytes em recurso+64 e duas tabelas de
matrizes de `0x40` bytes em +6c/+70, e grava a contagem em recurso+68.
`0x960b50` a transfere, reduzida por `&0xff`, para model+220 e aloca estados
de juntas de `0xa8` bytes em +224 e samplers de `0x34` por junta/camada.
Isso corrige o antigo nome de trabalho ligado a eventos, sem invalidar os
oráculos que declararam explicitamente o campo zero. Não prova que o caminho
de juntas não zero já esteja implementado pelo kernel nativo.

Evidence: 066/067, pseudo
`ev_ac970a9c539982d77f9cc30697ff6c3cf31a3c4571372e2c3c08690ea735e2e9`;
019/020, pseudo
`ev_fe1fee4e44bf7c1ab7be006c382337fed1fb70a05bde91f2b561ddc9c84ed542`;
readbacks112–117 ligam loader, factory e vtable de `rModel`.

**Dependências do statewriter `b1c050`:** 034–043 e readbacks121–125 retêm
`d04924`, `d0473c`, `2b555c`, `b04e40`, `10b778`. `d04924` depende de modo e
índices globais/jogador; não é uma constante verdadeira. `d0473c` classifica
valores numéricos de `pl_type`; `2b555c` modifica uma sequência u16;
`b04e40` escreve um float no destino condicionado; `10b778` entra em limpeza
de memória com caminhos fora do pequeno entrypoint. Os bytes de entrypoint
não cobrem sozinhos todos os helpers de limpeza.

`b151b4(player,255,255)` (074/075) escolhe dois bytes na tabela `100c4fa`,
indexada por `pl_type`, com override/gates e fatores float antes de gravar
`pw+290/291`. Ainda depende de `d047b4` e de estado que não foi recuperado.
Virtual `+134` da vtable base do `uPlayer` aponta para `b31c64`, que é somente
`BX LR`; não inclui o vizinho `b31c68`. `b6cdf0(player,0x20000000)`
(078/079) faz OR no `pw+f0`; seu ramo bit31 seleciona `pw+f4` e remove bit31.
Isso preserva o efeito observado sem declarar um statewriter completo.

O consumidor de prefixos `94be10` foi retido em052 e permanece sem função
reconhecida pelo Ghidra. `b1407c` inclui a variante de até dois IDs em
`pw+97c/97e`; `b44ad8` (072/073) não contém leitura desses IDs. O significado
dos IDs e seus consumidores posteriores ficam com o recorte de eventos.

## Próxima pergunta mínima

Para fechar a entrada física: seguir o produtor do anel `0x11177a0` e os modos
de aquisição (`47cbf8/485b28`), depois a inicialização e as três rotinas de
calibração de `4859c8`. Candidatos de referência ao anel estão nos literais
`47cba8`, `47d404` e `d3ad94`; são sementes, não conclusões de hardware.
Para fechar ordem: seguir os callers virtuais dos slots acima e sua lista de
unidades, preservando gates e fontes de tempo. Nenhum desses pontos deve ser
substituído por um retorno constante para justificar política original.
