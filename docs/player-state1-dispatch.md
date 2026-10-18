# Despacho do estado1 para pedidos de animação

Foi fechada a rota numérica de `act_st=1, act_no={1,2,4,9}` até os wrappers de pedido de animação. O ensaio original executa os prefixos de entrada e seus helpers reais, **parando antes de executar o wrapper de pedido**. Portanto comprova roteamento, efeitos anteriores e argumentos; não comprova aceitação, retorno do handler completo ou política de repetição. Nenhum módulo nativo foi alterado, e os IDs não receberam nomes como andar/correr.

Alvo: ELF privado SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Artefatos em `research/private/player-state1-dispatch-v1/`. A investigação anterior [player-action-controller.md](player-action-controller.md) permanece congelada.

## Tabelas e propriedade dos campos

`b66a20` zera `pbw.byte376` e lê `pbw.byte371` (`act_st`). Para1, segue `b66a80 → b6a424`. Este lê `pbw.u16(+372)` (`act_no`), limita o índice a34 e usa a tabela de34ponteiros em `b6a444`.

| act_no | Label | Destino imediato |
| --- | --- | --- |
| 1 | b6a4f0 | b67430(player, mode0) |
| 2 | b6a544 | b1d58c(player, mode0) |
| 4 | b6a58c | handler inline em b6a424 |
| 9 | b6a694 | handler inline em b6a424 |

`mode0` é o argumento injetado pelo dispatcher, não o número da ação nem o selector LMT. O subestado que os quatro handlers testam é `pbw.byte5`. O selector atual vem da virtual+84: na vtable `uPlayer=e6bca4`, o slot `e6bd28` contém `b085bc`, que lê `player.u16(+244)` (primeira camada). Esse getter real é executado pelos casos1/2.

## Entrada: pbw.byte5==0

Todos os quatro caminhos primeiro escrevem `pbw.byte5=1`, depois chamam `b22094(player,0,0,0)`. O helper real:

- grava **`pbw.pl_st(+370)=0`**;
- grava o float zero em `pw+a10`;
- ajusta `pw+f0` para `(anterior|1)&~6` via `b6cdf0` e dois clears.

Logo, uma integração futura não pode apenas pedir uma animação e preservar arbitrariamente `pl_st`. O oráculo verifica esse efeito com flags anteriores não nulos, sem zerar o objeto inteiro.

A ordem e os pedidos observados são:

| act_no | Antes do wrapper | Selector solicitado | Wrapper | s0 / s1 |
| --- | --- | --- | --- | --- |
| 1, atual diferente de4 | `pw+f0 |= 0x80`; instala tabela f2efb8; lê selector atual | 3 | b0c198 | 6 / 0 |
| 1, atual igual a4 | mesmos efeitos; `pbw.byte5=2` | 4 | b0c198 | 0 / 0 |
| 2, atual diferente de4 | `pw+f0 |= 0x180`; `pw.byte17c |= 1`; `pbw+39c=1.25`; `pw+c4=1.25`; lê selector atual | 3 | b0c198 | 4 / 0 |
| 2, atual igual a4 | mesmos efeitos; `pbw.byte5=2` | 4 | b0c198 | 4 / 0 |
| 4 | `pw+f0 |= 0xc0`; copia `pw+b0 → pbw+24` | 0x2a (42) | b0c060 | 0 / 0 |
| 9 | `pw+f0 |= 0x40`; `pw+50=20` | 0x31 (49) | b0c060 | 6 / 0 |

Os valores com prefixo0x são hexadecimais; os floats são decimais. Os bits de `pw+f0`, flags do pedido, atributos da camada e flags do header LMT são campos diferentes.

Os campos `pbw+39c` e `pw+c4` têm nomes embarcados `mPlayerSpeed` e `mPlayerSpeed2` na evidência anterior. A gravação1.25 é comprovada; sua interpretação em unidades físicas ou composição posterior com velocidade da camada não é inferida nesta entrega.

`b0c198` é o wrapper de pedido condicionado à diferença do selector atual, conforme o contrato já recuperado de [motion-command-runtime.md](motion-command-runtime.md). Assim, a chegada ao wrapper com selector4 quando o atual já é4 pode terminar sem pedido ao setter. O mesmo vale para selector3 já atual. A observação do prefixo não é uma afirmação de que houve troca de animação. Os wrappers fixam a velocidade solicitada em1 na implementação já investigada; esse passo posterior não integra a execução desta entrega.

## Efeitos depois do primeiro pedido: apenas leitura estática

Os corpos e literais completos delimitam os efeitos seguintes, ainda não executados pelo novo oracle:

- ID1/mode0: depois de retornar do wrapper, escreve `pw+50=20`.
- ID2/mode0: escreve `pw+50=20`, instala f2efb8 com argumento de atraso0, escreve `pw+144=30` e copia `pbw+24 → pw+b0`.
- ID4: após o pedido, tailcall `b6a97c(player,f2f2d0,0)`.
- ID9: após o pedido, tailcall `b6a97c(player,f2efb8,0)`.

A instalação de f2efb8 **antes** do pedido no ID1 foi executada com o instalador e os fills originais. Nos demais IDs a instalação está depois da fronteira parada e é apenas evidência estática. `f2efb8` contém35registros de22bytes e sentinel; `f2f2d0` contém31registros e sentinel. Os704bytes da segunda tabela foram retidos integralmente em REA026, sem interpretar seus predicados neste recorte. Não se tratam as duas tabelas como equivalentes.

## Continuação e gates retidos

Para ID1/mode0, subestado1 chama primeiro `b1d48c`. Apenas se ele retornar0, consulta `b0893c`; se essa consulta retornar não zero, incrementa `pbw.byte5` e chama `b0c060(player,4)` com `s0=0,s1=0`. No subestado2 chama `b1d48c` e retorna. Outros subestados não seguem essa sequência.

Para ID2/mode0, subestado1 usa `b1d974` antes de `b0893c`, com a mesma regra posterior de incremento/pedido4. No subestado2, mode0 encaminha diretamente a `b1d974`. Os ramos para outros valores de mode não pertencem aos quatro IDs selecionados.

`b0893c` é um thunk para `b05758`, que passa `player+240` a `d024b0`. O último retorna booleano do **máscara0x4 dos flags u16 em layer+6**. Essa origem é mais precisa que chamar o gate apenas de “fim de animação”; nenhum campo do header LMT é consultado por essa folha.

Para ID4, subestado1 consulta esse gate e, se verdadeiro, encaminha `b235f8(player,1,5,0)`. Para ID9, subestado1 consulta `d04924`, depois a virtual+98. Quando o resultado virtual é não zero, encaminha `b3ca88`. Quando é zero, testa `pw+50`; se <=0, chama `b1d114` com argumentos0/4/0 e quinto argumento0. A semântica completa desses encaminhamentos não foi executada neste recorte.

Os helpers de continuação foram retidos sem ampliar a investigação para toda a árvore:

| Helper | Fatos estáticos / próxima fronteira |
| --- | --- |
| b1d48c | d04924, chamadas repetidas da virtual+98, pw+50, b07e80 e b1eeac; pode encaminhar b235f8 com pares1/3 ou1/1a, ou chamar b3ca88. Os calls repetidos não foram colapsados em um valor presumidamente estável. |
| b1d974 | d049b4(player,0x100), b464fc, d04924, virtual+98, b24204 e outros helpers; encaminhamentos numéricos incluem1/9,1/3,1/b,1/d e2/0. Não há gate fabricado para executar esses ramos. |
| b22094 / b39f84 | Corpos completos retidos e efetivamente executados nos prefixos; o segundo faz OR do byte pw+17c. |

Essa é a fronteira necessária para recuperar handlers completos: continuar após o pedido real, preservar suas mudanças, e executar os gates das próximas chamadas sem substituí-los por retornos escolhidos. O novo documento não propõe ligar o driver apenas por um mapa de selectors.

## Prova e limites

`arm-first/validation.json` é o resultado autoritativo: **48 prefixos positivos,4 controles negativos,5.784 instruções originais**, Unicorn2.1.4/ARM11MPCore. O produto cartesiano cobre4IDs, selectors atuais0/3/4/ffff e poisons00/a5/ff. Todo caso chega ao endereço original do wrapper; o hook para **antes da primeira instrução desse wrapper**, registra r0/r1/s0/s1, sem alterar registradores nem fabricar retorno. Não há continuação após a parada.

As verificações cobrem o alvo de despacho, selector, bits exatos binary32 de s0/s1, campos anteriores ao pedido, flags preservados e os writes próprios de cada caminho. Os negativos retiram helper real, selector ou flags inicializados; outro escolhe subestado1 e deve parar no próximo helper não permitido. Todos falham explicitamente. RAM inicial desconhecida permanece poison e não satisfaz o guard de leitura inicializada.

O ensaio levou0,134s de parede,0,136s de CPU e pico RSS do processo Python de102.514.688bytes; esses valores não incluem REA nem representam família completa de processos. A importação/resolução fria REA levou150,531s. Pico de memória/CPU da família REA e tokens não foram medidos.

Ghidra reconhece apenas parte dos blocos de switch de b6a424/b67430; seu pseudo remove blocos e pode incorporar vizinhos. Não reconheceu b1d58c como função. Os mapeamentos e argumentos, portanto, se apoiam nas tabelas/bytes completos, LLVM/VFP2 e execução do prefixo, sem criar funções no provider ou alterar o alvo. Os readbacks009/010/011 incluem os corpos, tabelas e pools necessários; a auditoria verifica24intervalos declarados contra9readbacks novos (4.368bytes) e leituras anteriores reutilizadas.

REA sessão1792306020796, perfil focused, heap3GiB: export027, close028 com snapshot, bridge42455 saiu0 e não restaram processos próprios REA/Ghidra. Bundle com25Evidence, índice `source-facts.json`, traces completos comprimidos, lifecycle e manifesto privados. Nenhum launcher foi manipulado.

Reprodução em novo destino:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-state1-dispatch-v1/verify_prefix.py --output research/private/player-state1-dispatch-v1/recheck
python3 research/private/player-state1-dispatch-v1/retain_facts.py
```

O segundo comando audita o snapshot `arm-first` existente. A integração permanece adiada: este recorte identifica pedidos e efeitos concretos sem afirmar startup, scheduler, nomes de movimento ou handlers completos.
