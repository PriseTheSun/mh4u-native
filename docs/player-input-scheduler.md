# Configuração do receiver e ordem parcial da entrada

Este recorte fecha os produtores de seis campos usados pelo receiver e liga
`sMhPad+0x1c` à passagem que pode chamar `uPlayer+0x24`. Ainda não estabelece a
posição de `sPlayer+0x1c` nessa passagem: seu caller foi encontrado, mas é o
callback `+0x20` de outro tipo de objeto, cujo agendamento permanece aberto.
Não há nova política de gameplay nem alteração no runtime nesta investigação.

## Fonte e validação

Fonte: ELF local `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Sessão REA `1792286478819`, Ghidra 12.1.4 ARMv6, heap 3 GiB, perfil focused
sem Decompiler Switch Analysis. A sessão foi exportada e fechada; os artefatos
estão em `research/private/player-input-scheduler-v1/`:

- `rea-evidence.json` e `rea-snapshot.json`: respostas completas e snapshot;
- `evidence-index.json`: Evidence IDs, consultas, hashes e erros preservados;
- `readback.json`: comparação de todos os readbacks completos com PT_LOAD do ELF;
- `manifest.json`: arquivos, hashes, cobertura e encerramento;
- `retain_evidence.py` e `reference_scan.py`: reprodução da verificação e da
  busca conservadora de referências ARM B/BL e palavras alinhadas.

Esta rodada é **estática**, com bytes REA confrontados com o ELF e disassembly
LLVM ARMv6/VFP2. Não executou o scheduler, callbacks, construção completa ou
hardware. Pseudocódigo é apoio, e os offsets/ordem vêm do assembly. Em
`644c64`, por exemplo, o decompilador apresenta um cast inteiro para o produto
temporal, enquanto `VMUL.F32`/`VSTR` preservam o float; vale o assembly.

Funções não reconhecidas (`af4c00`, `644e98`, `65a8bc`, setters de clamp e
outros) têm leitura de bytes e disassembly derivado, sem pseudocódigo inventado.
As chamadas equivocadas a nomes não reconhecidos e os pedidos com chave
`size` em vez de `length` foram rejeitados e estão no índice de falhas. As
funções Ghidra `af3dac` e `2b0dd8` possuem corpos não contíguos; seus ranges
constam das respostas. Uma lista vazia de callers não prova ausência de
chamadas indiretas ou de branches não reconhecidos.

## Receiver: identidade, nomes e inicialização

Usamos `S = *(0x10572d8)` para o objeto de pad, `R` para o argumento receiver
de `5b814c`, e `H = R+0x2c` para o bloco de estado. Os offsets abaixo são de
`R`, não do recurso LMT, do player ou do estado MOD. No caminho normal de
`sMhPad`, R é o objeto de pad. Nomes vêm da reflexão `5b7490`; `dirty` é um
nome derivado dos writes e do consumo em `5b6fa0`, não uma string recuperada.

| Campo | Representação observada | Nome da reflexão / função | Produtor comprovado |
| --- | --- | --- | --- |
| R+30 = H+4 | byte | `Be_flag` | constructor H: 0; `5b8238(R,0,attr)`: 1 |
| R+33 = H+7 | byte | `Kind` | constructor H: 0; `5b8238`: **12** |
| R+40 = H+14 | s16 | `Clamp_min` | constructor/reset: 40; setter `5b7f80` |
| R+42 = H+16 | s16 | `Clamp_max` | constructor/reset: 145; setter `5b7f64` |
| R+44 = H+18 | u16 | `Clamp_mode` | constructor/reset: 0; setter `5b7f9c` |
| R+46 = H+1a | halfword, lido signed | dirty | constructor/reset: 1; setters: 1 se diferente; consumo: 0 |

`R+33` não pode ser representado como um valor booleano original: o branch
testa zero, mas o produtor grava o identificador 12. Também foram recuperados
`R+31 = Rno`, `R+32 = Pad_no`, `R+34 = Ability`, `R+38 = Input_attr` e
`R+3c = Socket_no`; isso não atribui significado físico a todos os bits.

O constructor `5b86a0` usa o helper de array Thumb `1001f4` para construir
um bloco de stride `0x90` em `S+2c`, com callback `5b7ff8`. Este callback
inicializa as áreas do bloco, instala vtable `e7a23c` e grava explicitamente
40/145/0/1. O mesmo `5b86a0` chama `5b8238(S,0,8)` em `5b87d4`.
`694bd8` chama `5b86a0` e depois instala a vtable de `sMhPad`, `e658cc`.
São efeitos desses caminhos de construção, não defaults deduzidos de BSS e
nem prova de que os valores permaneçam imutáveis após configuração.

`5b8238` só modifica o receiver quando o argumento `r1` é zero; caso contrário
retorna zero. No caminho aceito, limpa estado anterior quando Kind é não zero,
grava Pad_no=0, Kind=12, Ability=`0x281`, Rno=1, Be_flag=1, Socket_no=0 e
Input_attr=`r2`, além de limpar resultados e estado de repetição. Seus writes
em R+48/+4a pertencem ao estado de repetição, não aos limites de clamp. Não
altera R+40/+42/+44/+46 nesse caminho.

Os setters de min/max leem o halfword com extensão de sinal e comparam com
**todo** o argumento `r1`; o setter de modo lê unsigned e também compara com
todo `r1`. Se diferentes, armazenam os 16 bits baixos e dirty=1. Não foi
observada validação do domínio no setter. O callback `5b7fb8`, exposto como
`Clamp_Init`, grava min40/max145/mode0/dirty1 incondicionalmente. A reflexão
expõe esses callbacks; não foi provado que ela seja o único mecanismo capaz
de escrever esses campos.

O consumidor recuperado na rodada anterior (`5b6fa0`) usa dirty e Pad_no=0
para aplicar o modo e limites às três configurações, limpa dirty antes da
aquisição e então continua. A representação do selector é distinta do modo:
o selector indexa `e362dc`, cujos primeiros quatro bytes retidos são
`00 01 00 00`; a extensão total da tabela ainda não foi provada. Não ampliar o
domínio por essa amostra. Ver [aquisição](player-input-acquisition.md) e
[filtros](input-filter-runtime.md) para o contrato dos consumidores.

Evidence: 020/021/022 (`5b8238`), 027/028/029 (`5b7ff8` e setters), 084
(`5b86a0`), 095/103 (`5b7490`), 104 (setters). O pseudo de reflexão é
`ev_2ff7b546e9f55f4e3edad5d1145a1f1ccbcc6498e4e35fbdb58359055583108c`.

## Ordem local do gerente principal

`af4f54` constrói o objeto principal, instala vtable `e6b7e4`, publica-o em
`105718c`, constrói `sMhPad` em seu campo +2b0 e o gerente de unidades em
+298, com vtable `e6b87c`. `af3dac` constrói o `sPlayer` em +31c; `af94f8`
instala vtable `e6b8d0` e publica o objeto em `fb83e8`. Criação e publicação
não estabelecem a ordem dos updates.

O callback principal `af4c00` está em `e6b7e4+1c`. A subsequência comprovada,
omitindo outros callbacks intermediários sem alterar a ordem relativa, é:

| Ponto no callback principal | Ação observada |
| --- | --- |
| af4c38 | Se `*(fb6b7c)` existe, calcula `g=2b19ec(obj)`; caso contrário g=0 |
| af4c44 | Chama `643d68(main)`, que também atualiza campos temporais |
| af4cf4 | `main+2b0` virtual +1c → `sMhPad:694644` |
| af4cfc | Chama `2af838(main+2fc)` |
| af4d54 | Se g=0: `main+298` virtual +1c → `644e98` |
| af4d7c | Se g=0: mesmo gerente virtual +2c → `6450b0` |
| af4d88 | Se global `fb6b7c` não nulo: `2b5910` |
| af4da4 | Se g=0: gerente +298 virtual +28 → `645d78` |
| af4db0 | Se global `fb6b7c` não nulo: `2b5d74` |

`2b5910` pode chamar `main+310` virtual+1c, e `2b5d74` pode chamar
`main+30c` virtual+1c, após seus próprios gates. Nenhum desses offsets foi
igualado ao objeto de cenário descrito abaixo sem prova de associação.

Evidence: read032 (`af4c00`), 040/041/043 (`af4f54`), 038/039/042
(`af3dac`), read068/069/111 (vtables e constructor `sPlayer`),
079/080/082/083 (callbacks condicionais). Como `af4c00` não foi reconhecido
pelo Ghidra, sua ordem é derivada de `main-update.asm` sobre bytes read032.

## Passagem que pode chamar uPlayer+24

No gerente de unidades com vtable `e6b87c`, `644e98` percorre índices de
grupo crescentes e chama seu virtual+38, `644c64`. O loop usa a contagem em
`(*(10572f0))+614`. A identidade/conteúdo dessa lista em uma sessão real não
foi observado. Antes do loop, a passagem só continua se
`main.word50 != 0 || main.byte57 == 0`.

`644c64(manager,group)` acrescenta os seguintes gates e efeitos:

- repete o gate de main+50/+57; ignora grupo cujo flags em
  `manager+24*group+1c` contenham o bit de valor 2;
- lê o primeiro objeto em `manager+24*group+20`, guarda o próximo em
  objeto+0c antes dos callbacks e percorre a lista;
- grava `objeto.float14 = main.float5c * grupo.float28` com operação float32;
- pode chamar inicialização virtual+18 quando `objeto.flags4 & 7 == 1`,
  mudando primeiro esses bits para2;
- chama virtual+24 apenas para estado2, bit`0x400` presente e ambas as máscaras
  `objeto+18 & ~manager+d20` e `objeto+1c & ~manager+d24` iguais a zero;
- trata estados3/4 como transição/desligamento e remoção; não são ações do
  caçador deduzidas por nome.

Para objeto cuja vtable é `e6bca4`, virtual+24 resolve `b48230`, o update de
`uPlayer` já recuperado. Logo, **nessa passagem e se os gates permitirem**,
o callback de pad retornou antes do callback do caçador. Isto não prova que
seja a única invocação possível desses métodos no programa.

A fase posterior `6450b0` chama virtual+40 do gerente, `644f84`. Ela pode
inicializar objeto em estado1, chamar também seu virtual+24 nesse caminho
e depois virtual+2c para objeto ativo permitido. Não reduzir a execução
original a exatamente um callback+24 por frame. `645d78` chama virtual+3c,
`645c3c`, não aprofundado aqui.

A rotina de inscrição `645128` insere objeto na frente da lista de seu
grupo, mantendo links +0c/+10 e campos da lista. Portanto a ordem não é
determinada apenas pelo índice do grupo: os produtores da inscrição e a
ordem real das inserções ainda importam. Não foi estabelecido o grupo do
caçador nesta rodada.

Evidence: 074/075/read072 (`644c64`), 076/read073 (`644f84`), 077/078
(demais fases), read098 (`644e98`), read112 (`645128`), read107 (vtable
uPlayer). A Evidence de assembly de `644c64` é
`ev_bd869f5367fb2f463bddf3d4535db3c1f24d89b0871613a3714ed910ea7f6898`.

## sPlayer: caller recuperado, agendamento ainda aberto

Há duas chamadas concretas a `(*(fb83e8))->virtual+1c`, que resolve
`af8ae8` para a vtable construída de sPlayer:

1. `2b5a40` chama em `2b5adc`; é alcançada por `65bb38` em `65be7c`, ou por
   `659630` em `6596ac`. Este último é alcançável pelo tailbranch `65be50`.
2. `2b0dd8` chama em `2b0e1c`; é alcançada pelo tailbranch `65c9dc` dentro
   do bloco de `65c80c`.

O dispatcher `65a8bc` escolhe essas rotas por bytes do objeto de cenário:
`obj+82` seleciona um conjunto e `obj+60` um estado. Para `obj+82` em 0..4 e
`obj+60==2`, tailcall `65bb38`; para `obj+82==5` e `obj+60==2`, tailcall
`65c80c`. Os destinos possuem gates e estados adicionais: esses dois
predicados não bastam para assegurar um update de sPlayer em toda chamada.
Não foram atribuídos nomes de missões, idle ou movimento a esses estados.

**Correção de uma hipótese intermediária:** o constructor em `cfd0a4`
instala **vtable `e65598`**, e `65a8bc` ocupa **+20**, não +24. O slot+1c
é `65a7f4`, que inicializa estado e deriva `obj+82` de gates externos.
Logo não há prova de que `644c64` execute esse dispatcher. A palavra zero
em `e65594` é anterior à base efetiva, e não pode deslocar os offsets.
O getter de tipo `cfd170` retorna descriptor `10825a4`; esse é um bom ponto
para descobrir o gerenciador que possui o objeto.

Evidence: 048/049/read070 (`2b5a40`), 050/read071 (`2b0dd8`), read099/100
(`65bb38/65c80c`), read102 (`65a8bc`), read105 (`659630`), read106/110/116
(vtable, constructor e inicialização). Branches de origem e palavras de
tabela foram conferidos com LLVM; `source-references.json` é catálogo de
candidatos, não prova de completude do call graph.

## Limites e próxima pergunta

A próxima pergunta pequena é: **qual gerenciador chama virtual+20 de objetos
com descriptor `10825a4` e vtable `e65598`, e em que ponto de `af4c00` ele
roda?** Seguir a criação/inscrição de `cfd0a4` e o gerenciador base `5dd874`
é mais preciso que buscar qualquer `BLX` no executável. Depois, confirmar a
associação ao campo do gerente principal e seus gates. A posição de
sPlayer em relação ao pad e ao uPlayer permanece desconhecida até essa
ligação; não se autoriza escolher a ordem por conveniência.

Não foram provados frequência fixa, vínculo com SDL, relação com relógio de
rede, ausência de chamadas adicionais ou comportamento com callbacks que
alteram as listas. O clock principal `643d68` foi apenas lido, não executado
ou recuperado integralmente. Calibração e aquisição físicas permanecem no
recorte anterior, sem nova generalização de hardware.

Consultas auxiliares nesta sessão fecharam o getter de ações `cd7540`,
`b07480`, getter `b085bc`, literais/tabelas usados pelo root e literal float
`32767` em `5b8694` para o agente de filtros. A consulta BSS `10e6370`
(read090) retornou **0 de12bytes, complete=false**; não demonstra zeros e
não substitui a execução de inicializador retida em outro recorte.
