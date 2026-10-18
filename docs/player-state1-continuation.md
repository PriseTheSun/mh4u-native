# Continuação do estado1: IDs1,2,4,9

Foram executadas **375 chamadas completas** do dispatcher original `b66a20`, com `act_st=1`, `act_no` entre1/2/4/9 e `pbw.byte5!=0`, até o retorno real. São estados diagnósticos explicitamente inicializados, recursos LMT reais e helpers originais: não há retorno fabricado nem mudança de PC. O recorte fecha caminhos de espera, pedido de animação, alteração de orientação/posição e transição de estado. **Não fecha todas as dependências**: a consulta espacial sem cache e o manager com objetos presentes permanecem fronteiras concretas. Nenhuma integração nativa, câmera, launcher ou pacote foi alterado.

Fonte: ELF SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Evidência privada em `research/private/player-state1-continuation-v1/`; as entradas `pbw.byte5==0` continuam documentadas separadamente em [player-state1-dispatch.md](player-state1-dispatch.md) e no oráculo `player-state1-runtime-v1`.

## Campos e gates compartilhados

`player+e30` aponta PBW; `player+a29c` aponta PW. O dispatcher sempre grava zero em `pbw.byte376` antes de selecionar `act_st/act_no`. Os quatro caminhos deste recorte vêm de `b6a424`; ID1 usa `b67430(mode0)`, ID2 usa `b1d58c(mode0)`, IDs4/9 são inline. Os outros modos desses helpers não integram a prova.

| Operação | Entrada e efeito exatos |
| --- | --- |
| `b0893c → b05758 → d024b0` | Booleano de `u16(player+246)&4`: flags da camada0, sem ler header LMT. |
| `d04924` | Lê o objeto via `*(fb6b7c)`. Se byte+2b==2: `pbw.byte30!=3 && pbw.byte33==0`; senão compara **signed8** do global+2f a `pbw.byte33`. Sem stores. |
| virtual+98, vtable uPlayer `e6bca4` | Slot `e6bd3c → b417a4`. Retorna0 quando `d0473c(player,0) && b07e80(player) && pw.byte50d==1`; caso contrário chama `b0a41c(player,0)`. |
| `b0a41c(player,0)` | `u16(pbw+3c4)` em0..49→0,50..109→1,110..65535→3. Não são nomes de ações. |
| `b3ca88` | Chama a referência angular real `b1061c`, depois grava `u32(pw+b0) = low16(referência−0x4000+u16(pbw+3c0))`. Não modifica os pares de entrada nem o timer. |
| `b235f8(player,state,action,flags)` | Escreve `pbw.byte376=1`, chama **diretamente** `b1c050` com os argumentos, depois escreve `pbw.byte6fa=0`. Não chama o seletor de animação nessa função. |

Os calls repetidos da virtual+98 permanecem executados e registrados, sem substituir chamadas por uma única avaliação. No perfil observado, seus helpers são leitores; `b3ca88` altera orientação, não a magnitude classificada. Isso não autoriza eliminar calls em um futuro scheduler ou outra implementação virtual.

## ID1: b1d48c, depois gate da camada

No subestado1, o handler chama `b1d48c`; retorno não zero encerra. Somente no retorno0 chama `b0893c`. Se o bit4 da camada está ligado, incrementa `pbw.byte5` e chama `b0c060(player,selector4,s0=0,s1=0)`. No subestado2 chama somente `b1d48c`. Subestados3/255 testados retornam sem esses helpers; a tabela/branches mostra o mesmo para os demais valores diferentes de0/1/2.

A ordem de `b1d48c` é:

1. Se `d04924` retorna0, retorna0.
2. Consulta a virtual+98. Se não zero, chama `b3ca88` e retorna0.
3. Se `pw.float50<=0`, consulta a virtual novamente; se ela retornar0, escolhe a transição abaixo e retorna1.
4. Nos demais casos, consulta a virtual outra vez; se não zero, chama `b3ca88`; retorna0.

A transição do passo3 é `b235f8(player,1,0x1a,0)` quando `b07e80==0 && pw.byte594!=0` (`b1eeac`); caso contrário é `b235f8(player,1,3,0)`. Os dois destinos foram executados. Esse pedido é uma transição de `act_st/act_no`, não um selector LMT.

## ID2: b1d974, custo, deslocamento e transições

A estrutura posterior do handler é a mesma do ID1: subestado1 chama `b1d974`, e somente retorno0 permite o gate da camada e o pedido4; subestado2/mode0 chama `b1d974`. Os outros subestados retornam.

`b1d974` executa nesta ordem:

1. `d049b4(player,0x100)` lê `pw+f0 & 0x100`. Se verdadeiro, chama `b464fc(player,s0=−0.5)`.
2. Chama `d04924`. Se falso, chama `b2a318(player,s0=2.5)` e retorna0.
3. Se a virtual+98 é não zero **e** `b24204(player,0)` (bit0 de `pbw+3d4`) é verdadeiro **e** uma segunda leitura de `pw+f0&0x100` é verdadeira, segue o ramo ativo abaixo.
4. Caso contrário, consulta a virtual novamente. Resultado1→transição `(1,9,0)` e retorno1. Se `pw.float50<=0` e outra consulta virtual retorna0→transição `(1,3,0)` e retorno1. Nos demais casos, consulta a virtual de novo e, se não zero, chama `b3ca88`; retorna0.

O ramo ativo chama primeiro `b3ca88`, depois `b2a318(2.5)`, depois:

- `b3dfa8` verdadeiro: copia `pbw+24 → pw+b0`, transiciona `(2,0,0x0c)`, retorna1.
- Caso contrário, `b1ee88(player,150)` falso: transiciona `(1,0x0b,0)`, retorna1. O helper compara `pbw.float354` com o inteiro150 convertido para float32.
- Caso contrário, `b34268` verdadeiro e nova chamada `b24204(player,0)` verdadeira: transiciona `(1,0x0d,0)`, faz `pw.byte17c |= 1`, retorna1.
- Sem esses resultados: retorna0.

As transições2/0,1/11,1/9,1/3 foram executadas; **1/13 é somente estática**, pois requer o manager de objetos não vazio ainda não coberto. O bit0 do input não foi rotulado como botão físico. O bit0x100 de PW não é atributo de camada nem flag do header LMT.

### Atualização de pbw.float354

`b464fc` primeiro verifica `d04924`. Para o argumento negativo−0.5 deste caller, multiplica por3 se `pw+1f8&0x80`, senão por2 se `&0x40`. Procura IDs111/272 na lista real de20u16 em `pbw+2fc`; se presentes multiplica por0.5. Depois, se112 está presente, multiplica pelo literal binary32 `0x3f99999a` (aproximação de1.2). Não foram atribuídos nomes aos IDs.

Em seguida chama `b2a58c(player,0,valor)`:

- repete `d04924`; rejeita quando `(*(fbeffc)+ad50)==100`, via `6b3630` real;
- para valor negativo e gate verdadeiro, não altera354 se `pw.float220>0` ou `pw.float838>0`;
- se negativo e `pw+1f4&0x10000000`, aplica0.5 quando ID37 está presente, senão0.25 quando ID36 está presente;
- multiplica pelo float `player+14`, soma a `pbw.float354`, limita primeiro a zero e depois a `pbw.float358`.

A ordem das multiplicações, chamadas e clamps importa. O ensaio cobre os multiplicadores, prioridades, gate falso, valor inicial zero, limiar150 e limite superior. O perfil numérico validado é finito; não se generaliza uma política para NaN/Inf. A mesma palavra `pbw+358` é consumida como inteiro assinado em outro caminho de seleção; isso não autoriza duas propriedades independentes.

### Atualização de posição: b2a318

O helper não consulta colisão: soma a `pbw.float[10,14,18]` um deslocamento derivado dos low16 de `pbw.u32[20,24,28]`, do argumento2.5 e de `player.float14`. Usa `568ab8/568a80` e `aacf0c` reais. A tabela fixa é `e36e98`,256registros×16bytes; cada registro contém quatro floats. Para ângulo `a` limitado a16bits, índice=`a>>8`, fração=`(a&255)/256`; os dois coeficientes são `table[0]+table[2]*fração` e `table[1]+table[3]*fração`, na ordem float32 original. Nenhum seno/cosseno da biblioteca host participa da execução.

O helper faz as combinações na ordem X/Y/Z mostrada em `b2a318-llvm.asm` e força a componente intermediária Y a zero antes de escalar/somar. A auditoria independente reconstrói exatamente essas operações e compara198componentes; não converte para coordenadas glTF nem atribui unidades/FPS a `player+14`.

### Fronteiras concretas do ramo ativo

`b3dfa8` retorna1 diretamente quando `pw.byte7f0==1`; caso contrário retorna0 se `pbw+478&0x40000` ou `pw.byte9a8!=0`. Sem esses gates, chama `b40b88(player,0,s0=−60)`.

O ramo **cache presente** de `b40b88` (`pw.byte6a8!=0`) lê `pw.float6a4` e compara com `float32(pbw.float14−60)`. Compara estritamente menor. Esse caminho inteiro foi executado, inclusive ambos resultados. Cache ausente segue preparação espacial e `2ba230` a partir de `b40cb0`: não foi executado; o guard para antes de substituir qualquer resultado. Não foi inferida validade do cache em jogo nem sua ordem de produção.

`b34268` retorna0 se `pw.byte9a8!=0`. Senão, `6823a8` percorre32slots reais em `(*(f32004))+18+4*i`; para entradas não nulas chama `911e48` e, quando necessário, `91a2a8`. O ensaio cobre a lista explicitamente vazia, sem fabricar um retorno false. Entradas presentes são uma fronteira não coberta; o controle negativo conserva a falha no primeiro helper. Isso bloqueia provar a transição1/13 nesta entrega.

## IDs4 e9

ID4/subestado1 chama `b0893c`; se verdadeiro, faz `b235f8(player,1,5,0)`. O estado muda, mas não há pedido LMT nessa chamada. Outros subestados não zero retornam. Ambos resultados do gate foram executados.

ID9/subestado1 chama `d04924`; falso retorna. Quando verdadeiro, consulta virtual+98. Não zero→tail `b3ca88`. Zero→se `pw.float50>0` retorna, senão chama `b1d114(player,0,4,0,stack0)`.

**Os argumentos0/4/0 desse wrapper não são state0/action4.** São `pl_state0`, interpolação4 e frame0, encaminhados ao seletor `b3fc50`. O ensaio usa o contexto explícito `pw9a8=0,pw7fc!=1,pbw6f9!=1,pw594=0,signed(pbw358)<=0x43160000`. O seletor original pede **selector0x203 (515 decimal), co2 slot3**, passa pelo wrapper/setter real e então chama `b1c050` com `act_st0/act_no0`. A interpolação efetiva entregue ao setter depende do wrapper; os argumentos exatos estão registrados, não são igualados automaticamente ao argumento4 de entrada. Bank2/co2 vem do dono já fixado de [player-selection-area.md](player-selection-area.md); este recorte não executa o carregador de bancos.

## Prova reproduzível e limites

Resultado autoritativo: `arm-second/validation.json`, fixture `state.bin` SHA-256 `eabd01c957bea2eb23f560c456bf021033dd916de1662c544d7d33e423692a0e`. São375retornos completos,134.217instruções,44.060leituras,20.458stores,64setters/32aceitos,55transições,236calls virtual+98 e66atualizações de posição. Guards verificam bytes inicializados, regiões de escrita, código permitido, orçamento, ABI e imutabilidade de fontes/curvas/templates. Os três recursos co0/lco0/co2 são privados e fixados por hash; somente cópias relocadas autorizadas pelos donos são mapeadas.

Seis negativos removem virtual real, timer inicializado, tabela trig ou helper de custo, ou chegam aos ramos espacial sem cache/manager com objetos. Nenhum sucesso é reportado nesses caminhos. O corpo espacial novo foi retido inteiro, mas o guard de execução mantém somente seu ramo cache. O dispatcher/handlers permanecem inteiros; a limitação é explícita nas dependências, não uma substituição de função.

`artifact-validation.json` verifica5.583relações de arena/estado/pedido,198componentes de posição e104atualizações de354 por leitura independente da fixture. O protótipo `arm-grid` falhou por campo de área não inicializado em ID9; foi preservado. A revisão seguinte escolheu um contexto direto explicitamente declarado e co2 real, sem mudar código alvo. Uma asserção experimental da auditoria confundiu bit1 com máscara1 do pending; foi corrigida para máscara2 conforme o setter original e registrada em `failure-notes.json`.

Comando de reprodução, sempre em destino novo:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-state1-continuation-v1/verify_continuations.py --output research/private/player-state1-continuation-v1/recheck
python3 research/private/player-state1-continuation-v1/verify_artifacts.py
python3 research/private/player-state1-continuation-v1/collect_facts.py
```

Os dois últimos auditam `arm-second`, não reexecutam o jogo. A execução isolada levou5,77s de parede; CPU e pico RSS Python estão no relatório. Não foram medidos tokens nem pico de família REA. A resolução fria REA levou147,338s, após open1,710s. O bundle novo contém29Evidence: sessão1792306753156, export031, close032, bridge85892 saiu0; cleanup registrado em `lifecycle.json`. `source-facts.json` diferencia os27.992bytes mapeados dos bytes realmente exercitados: todos os bytes fonte executados/lidos tiveram readback REA conferido com o ELF. Corpos herdados não exercitados podem ter somente identidade ELF/assembly anterior; isso não é prova de execução.

Este recorte permite implementar continuações no perfil exposto com falha explícita fora dele. Não comprova bootstrap real, tempo do scheduler, decremento do timer, produção/validade do cache, integração de colisão, comportamento dos estados de destino, unidades físicas ou nomes como andar/correr.
