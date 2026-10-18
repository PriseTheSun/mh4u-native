# Inicialização original do modelo e das juntas

A cadeia observada constrói oito slots de camada, mas não mantém oito ativos. `961a54` (uModel) começa com quatro; `b07770 → b075b8` muda para zero; `b0c1d4` termina chamando `9574dc(this,2)`; o construtor derivado `b6d588` instala a vtable de uPlayer `e6bca4` e chama `b688ec`. Isso fecha o valor **duas camadas ativas na cadeia de construção identificada**, separado de mudanças posteriores e da ordem do scheduler. Não foi executado o construtor completo de uPlayer.

O novo módulo `joint_bootstrap` reproduz apenas a inicialização de metadados, pose, matriz e ordem de juntas do trecho `960f7c..9613b0`, incluindo a função original `95b260`. Não implementa o alocador, a troca de rModel, a criação de paletas GPU ou o bootstrap completo do caçador. O controlador e NativeActor não são alterados por esta entrega.

## Evidência e fronteiras

ELF local fixado: `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. REA/Ghidra12.1.4, ARMv6, perfil focused, heap3GiB, sessão `1792298838065`. Bundle e snapshot ficam em `research/private/model-animation-bootstrap-v1/`; 95 registros de Evidence exportadas, seguida de `098-close_binary.json` com fechamento confirmado. As 33 leituras novas, 13.374 bytes, foram comparadas integralmente ao ELF em `source-readbacks.json`.

| Assunto | Evidence na sessão nova |
|---|---|
| uModel961a54,8slots/4ativos | resolve002, pseudo012, assembly013, bytes033 |
| layer9606ec | resolve003, pseudo014/assembly015 de `FUN_00d024d8`, bytes034 |
| setter9574dc | resolve004, pseudo016, assembly017, bytes035 |
| jointctor95fcac | resolve005 não reconheceu função; bytes036 e LLVM; execução pelo endereço exato |
| profundidade/ordem95b260→95b39c | pseudo018, assembly019, bytes037; corpo Ghidra termina antes do fall-through, readback e execução incluem até95b484 |
| baseplayerb0c1d4 | pseudo020, assembly021, bytes038 |
| uPlayerb6d588/resetb688ec | pseudo048/assembly049, pseudo062/assembly063, bytes064/083 |
| base1a7ab8/6d4854/62775c | pseudo058/060/050, assembly059/061/051, bytes043/044/065 |
| resetb075b8/baseactorb07770 | pseudo090/092, assembly091/093, bytes081/082/096 |
| metadados960b50 | função completa previamente retida em sessão1792283675798/019–020; bytes novos069 cobrem o trecho executado |

O provider incorpora `9606ec` como bloco da factory `d024d8` e não reconhece `95fcac` separadamente. Isso está preservado; não foi inventado nome de função reconhecida. Callers vazios de644f84/645c3c não provam ausência de chamadas indiretas. As consultas022..042 e072..078 atenderam os bancos/lifetime do agente independente e não ampliam o contrato de bootstrap.

## Estado construído do modelo

Executamos **961a54 inteiro e todos os seus helpers reais**, sem interceptações, em três arenas anteriores preenchidas com0x25,0xa5,0xde. Antes disso foi executado o tail original de inicialização global `d1d4b8..d1d9a8`, também com seus helpers ARM/Thumb reais. O objeto anterior é entrada sintética declarada: nenhum zero de BSS foi presumido. Guardas registram leituras/escritas, ranges de execução e limites de instruções.

| Campo | Escrita original do construtor |
|---|---|
| M+40 xyz/padding | copia10e6370; padding0 |
| M+50 xyzw | copia10e6560 |
| M+60 xyz/padding | literais1,1,1,0 |
| M+70 e M+b0 | copiam a matriz10e6440 |
| M+220/+224/+22c | count0, juntasNULL, ordemNULL |
| M+228 | fallbackmap e32ec4 |
| M+230/+238/+23c | zero |
| M+234 |4, com8 objetos Layer construídos em M+240, stride130 |
| M+c00 | snapshotsNULL |
| M+bd0 | copia escala10e6380; padding0 |
| M+be0/M+bf0 | copiam10e6370/10e6560 |
| M+c04..c40 |16 banks NULL |

O inicializador executado produziu10e6370=(0,0,0),10e6380=(1,1,1),10e6440=I,10e6560=(0,0,0,1),10e6570=(0,0,0,0). O vínculo estático de startup já retido é a entrada REL32 `ebf64c→d1d458`, ordinal501/954, percorrida por1013b8 desde100010 (`root-motion-defaults/source-selection.json`). Não executamos startup inteiro nem provamos imutabilidade desses globais depois dele.

M+4 e M+104 **preservam bits anteriores**. A forma derivada das instruções é `(prior4 & 0xfc0f03f8) | 0x000f4ff9` e `(prior104 & 0x00f00fc0) | 0x08f00e03`. As três arenas observadas mantêm diferenças nesses campos. Não há fundamento para chamar o objeto inteiro de zerado. A bitmap de bytes escritos pelo construtor está no relatório; campos não escritos continuam desconhecidos fora da entrada sintética.

## Camadas e caminho do jogador

`9606ec` define selectorFFFF; flags6,previous8,attrC=0; interpolation40/44=0; weight48=1; frame4c/previous50/end54=0; speed5c=1; field60=0; previous_step18=1; field10/14=0; deltaQ20 copia10e6560; deltaT30 copia10e6370; eventosf8..104 e masks108..124=0; jointsamplers128=NULL. Padding3c/b4/c4/d4=0. **Nem todos os demais campos são escritos**: por exemplo, buffers de sampler e valores preparados não recebem um memset geral neste construtor.

Os controles de curva6c/70 copiam os oito bytes de104e058, que são zero no ELF fixado. Trata-se de um snapshot de dados inicializados, não de imutabilidade demonstrada nem de uma curva escolhida por conveniência.

A execução inclui9574dc inteiro no ramo resourceNULL, mudando4→2 sem alocação. A origem do argumento2 é separadamente estática: `b0c2b8..b0c2c0`. `b6d588` chama esse construtor-base e instalae6bca4. O reset derivado b688ec chama b075b8 para oito submodelos internos; não passa o próprio uPlayer a esse reset. O scan conservador de BLs para9574dc encontrou12 candidatos; no intervalo b0..b7 só b0763c e b0c2c0, ambos corroborados em assembly. Isso não exclui setters indiretos ou writes futuros.

A vtablee6bc00 da base tem+18=b0b93c; e6bca4 do uPlayer tem+18=b5bec8. O objeto de pl_make construído somente porb0c1d4 não pode ser identificado automaticamente com o uPlayer derivado. Recursos, primeiros pedidos e registro no manager são tratados separadamente em player-motion-banks e na investigação de placement.

## Inicialização de juntas e API pura

`960b50` obtém record24B em rModel+64, count em+68, A em+6c e remap emrModel+108. Quando precisa criar storage, aloca J com stridea8, order com um byte por junta, canais de cada camada ativa com stride34, snapshots com stride38. Zera canais e snapshots; em cada snapshot grava indexFFFFFFFF. M+220 recebe `count & 255`. As funções de alocação e paletas não foram executadas nesta entrega; esses fatos são estáticos.

A parte numérica executada recebe arrays já alocados e os registros/matrizes originais de body001 (19 juntas, SHA f63bc643e17f05156f77734e0797ee2e3f135f118fe29bed34e5112a339178a3) e em062 (12 juntas, SHA2a9e74fe29a24623a0aab96cfd71e918ada70a279355cd232c7799bac1cf2715). Executa primeiro95fcac por junta e então960f7c..9613b0, com95b260 e o fill reais.

| Saída | Origem/efeito |
|---|---|
| J+8,16words | cópia integral deA |
| J+48/J+78 xyz | **record+12/+16/+20**, padding0 |
| J+58 quaternion | extração deA com a mesma fórmula original do dono joint_pose; sem normalização extra |
| J+68 xyz/padding | copia10e6380, padding0 |
| J+88 callback/J+a0 owner | NULL/modelo próprio |
| J+8c | recordword+8, cópia de bits |
| J+94 | `boneID(byte0)<<24 | parent(byte1)<<8` |
| J+98 | `(prior98 & 0xfffe0000) | child_count<<8 | recordbyte2` |
| J+90/order | profundidade por cadeia de pais; ordenação original por swaps comparando profundidade |

95fcac limpa bit16 deJ98, mas não inicializa seus outros bits. O tail sobrescreve bytes0/1, limpa bit16 e preserva17..31. A API exige `prior_mapping98` explicitamente. Também rejeita pais fora do array/ciclos e count256, que sai do perfil seguro pelo estreitamento original a byte; não imita os acessos inválidos ou loops sem limite do original.

`joint_bootstrap::initialize(parsedRecords,A,prior_mapping98,scaleTemplate)` retorna pose, matriz, metadata, profundidade, parâmetro8c, ordem e fatos callbackNULL/ownerSelf. O dono MOD continua responsável por interpretar os registros; o módulo não reinterpreta bytes de recurso. Erros são transacionais. A matemática de quaternion usa o dono `joint_pose::matrix_rotation`, que exige domínio finito e ambiente binary32 apropriado; demais cópias preservam bits, inclusive valores não finitos que não entram nessa matemática.

Nos31 joints reais, recordT e A.translation coincidem bit a bit; isso não autoriza substituir a origem recordT pelo campoA em outros recursos. O byte2 difere do índice em vários joints de body001 e troca8↔9 em em062. A ordem resultante também difere da ordem do arquivo. O atual mapping98=ff de laboratório não é o resultado desse carregador.

## Validação e reprodução

`arm-complete/arm-validation.json`:3 casos de construtor completo+setter e15 casos de juntas (dois MOD reais mais três matrizes analíticas que cobrem os ramos de maior diagonal, cada um com três padrões anteriores),55.821 instruções observadas. Três controles negativos exigem memória anterior/globals inicializados e impedem que o perfil sem alocador escape para1076cc. Os guards não modificam registradores nem simulam retornos. As traces comprimidas registram PCs em ordem.

Fixture `arm-complete/joint-fixtures.bin` é SHA256 `149eb28604ac02d824f026b80043ad5d26f1e1226ff9a0b8b6aaace28b4f330f`,33.921 bytes. Normal e ASan/UBSan passaram **1.395 checks/15 casos cada**, comparando todos os campos retornados com bits exatos, incluindo matrizes, ordem, flags e metadados anteriores preservados. Os controles nativos cobrem pai inválido/ciclo, contagem256, extents, template ausente, erro matemático e preservação de cópias raw. Nenhum recurso foi enviado a serviço externo; ARM/Unicorn permanece apenas no oráculo privado, fora do executável entregue.

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/model-animation-bootstrap-v1/verify_arm.py --output /CAMINHO/NOVO
# Fixture nativa congelada: ver generate_fixture.py e native-validation.json.
```

Tentativas incompletas estão registradas em failures.json: literal de helper inicialmente fora do allowlist; leitura anteriorM104 revelada pelo guard; controle negativo refinado; LeakSanitizer indisponível no macOS. A execução final de ASan/UBSan usa detect_leaks=0 e halt_on_error=1; não reivindica leak coverage. A primeira análise REA levou142.651ms; demais consultas aproveitaram o mesmo import. Tempo/CPU/pico de memória do oráculo completo e tokens não foram medidos, portanto não há comparação de desempenho alegada.

Restam a política final de bootstrap do caçador, writes posteriores de flags/templates/curvas, a execução integral dos construtores derivados com suas dependências e o perfil real do alocador. Nenhuma dessas lacunas deve ser preenchida com defaults silenciosos no driver.
