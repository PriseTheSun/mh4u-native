# Seleção, pedido de animação e transição no mesmo estado

`player_controller_step` compõe os donos `player_selection`, `motion_command`
e `player_transition`, na ordem do original. Foram comparadas **257 chamadas
ARM completas até o retorno real**, incluindo os modos 0, 1 e 2 do router,
com **372 chamadas ao setter, 160 aceitas**. O C++ coincidiu em todos os
campos declarados e argumentos observados. Não há substituição de helpers
por retorno/no-op no oráculo desta composição.

Esse resultado fecha um perfil numérico com memória e recursos declarados.
Não é uma captura de partida, nem demonstra a política que decide chamar
esses ramos. O sampler de movimento de modelo, binder, relógio e scheduler
não fazem parte da execução aqui. O módulo não foi ligado ao controlador
interativo pelo trabalho deste recorte.

## Atualização da API canônica (marco0.13.0)

A composição agora recebe `span<const motion_pending::Layer>` e `active_count`
separadamente. `State.motion` contém somente `motion_command::Fields`; o dono das
camadas continua no driver. O resultado devolve as camadas para substituição
atômica após sucesso. A projeção de peso usa `layers[1].animation.layer.weight`.
Storage insuficiente ou cópia acima dos limites têm fase `storage`. As duas APIs
aceitam `motion_command::Sampling` e `Limits`, encaminhados ao comando completo,
que agora executa958884 e preserva os cursores canônicos.

Os257 casos históricos continuam passando na nova representação (3.854 checks),
com a mesma limitação do corpus: evitam a consulta live. A prova live independente
está em [motion-command-runtime.md](motion-command-runtime.md), e a integração visual
usa o comando diretamente, não a seleção completa do jogador. A composição completa
seleção→consulta live→transição ainda não tem um novo oráculo ARM próprio.

O gerador atual é `research/private/player-controller-live-v1/generate_legacy.py`;
verifica o ELF antes de extrair globals e conserva o oráculo histórico intacto.
`generated-final/manifest.json` tem SHA36a3695267f3bcb3a711d1192da21394e9e6557ba6b969c4ab65dd69da9927e4.
Os registros do fixture são arrays fixos, materializados em camadas canônicas por
caso, evitando grandes inicializadores dinâmicos na compilação.

As seções abaixo descrevem o ensaio histórico. Na tabela e na API, leia o novo
contrato indicado acima; os relatórios históricos preservam as fontes daquela versão.

## Um dono para os bytes compartilhados

As bases de layer são `model+0x240` e `model+0x370`, com stride `0x130`.
Logo, `layer1+0x48`, o peso mantido por `motion_request`, ocupa exatamente
os mesmos quatro bytes de `transition.player.field3b8`.

| Região original | Dono na composição |
| --- | --- |
| `model+3b8..3bb` | somente `layers[1].animation.layer.weight` |
| `model+d10..d1b` | `State.player.fieldsd10` |
| layers e `model+234` | span e active_count do caller |
| `d48..d63`, `d68..d6b`, `d70..d7f` | `State.motion` |
| campos de `pbw`/`pw` e global da transição | os respectivos tipos de seus donos, em `State.player` |
| `model+f0`, banco `model+c04` e ações/eventos | `motion_request::Model` externo, imutável durante a chamada |

O inventário byte a byte dos campos representados encontra **somente
`0x3b8..0x3bb`** na interseção de seleção/transição com comandos de animação.
`pbw` e `pw` são alocações apontadas distintas: offsets numericamente iguais
nelas não representam aliases do modelo. Vtable e ponteiros concretos
`model+e30`/`a29c` pertencem à inicialização do oráculo; não são estado nativo
duplicado. A conclusão de interseção é limitada aos campos e às duas layers
representadas, não ao objeto inteiro desconhecido.

`selection_view()` projeta o peso por `bit_cast<uint32_t>`. `PlayerFields`
deliberadamente não armazena `field3b8`. Depois de executar o comando,
a composição produz **uma nova projeção** para a continuação. Isso importa:
o pedido sentinel da layer1 pode escrever peso 1 antes de a transição
preservar esse valor pelo bit 0 das flags. Retomar a seleção com uma cópia
anterior restauraria incorretamente o peso antigo.

A auditoria dos traces observou 175 transições que zeram esse campo e 79
que preservam o valor posterior ao comando; destas, 39 preservam 1 depois
do sentinel. Os três retornos do estado 3 conservam os bits anteriores.
Os 126 pedidos sentinel de layer1 retornam falso no setter original;
seus efeitos anteriores ao retorno continuam preservados.

## API e ordem

Arquivos: `src/player_controller_step.hpp`, `.cpp` e `_tests.cpp`.
Namespace `mh4u::player_controller_step`:

```cpp
SelectionView selection_view(const State&, std::span<const motion_pending::Layer>) noexcept;
Result apply_common(const State&, std::span<const motion_pending::Layer>, uint32_t active_count,
                    const player_selection::CommonArguments&, const motion_request::Model&,
                    const motion_command::Globals&, const player_transition::Constants&,
                    const motion_command::Sampling& = {}, motion_command::Limits = {});
// apply_wrapper recebe o mesmo storage/count e acrescenta Wrapper/WrapperArguments.
```

`Result` é `variant<Output,Error>`. `Error` mantém a fase
`selection`, `animation_command` ou `transition` e o erro do dono original.
A função recebe valores imutáveis e só publica `Output` após a composição
inteira. Uma dependência desconhecida não produz estado parcial nem sucesso.
O falso **válido do setter original** é diferente de um erro do perfil:
fica em `ObservedRequest::accepted=false` e a transição prossegue.

`Output` conserva o estado final, o pedido opcional de seleção ao router,
até dois pedidos realmente processados pelo dono do setter, seus argumentos
e resultados, e os efeitos opcionais de transição. A observação de dispatch
da transição é uma chamada já retornada pelo gate original declarado; não é
um comando novo para o integrador executar depois.

A sequência é: prefixo de seleção → router/comando → setter(s) e seus efeitos
→ projeção atualizada → continuação/transição. `pw+594` é consultado pelo
dono da seleção **depois** do retorno do comando. Modo 0 com selector igual
pode retornar sem setter; isso não apaga a observação do pedido ao router.
O estado geral 3 termina no prefixo sem consultar os recursos de animação.

Permanecem os perfis dos donos: os argumentos gerais completos aceitos são
1 e 3, não apenas seus bytes baixos; os ramos adicionais de seleção são
rejeitados. Conversões signed32→binary32 pertencem a `player_selection`,
com `FE_TONEAREST` e proibição de fast-math. O módulo de composição não
reimplementa conversões, seleção, evento, setter ou transição. Consulte
[seleção](player-selection-runtime.md), [comandos](motion-command-runtime.md) e
[transição](player-transition-runtime.md) para seus demais gates.

## Fonte e perfil executado

Fonte ARMv6 LE: `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
`research/private/player-controller-step-v1/` retém a composição e fixa os
donos anteriores por hash. Reutiliza os bytes REA já verificados, sem nova
importação. Os relatórios `source-readback.json` dos escopos de seleção e
comando preservam Evidence, endereços e comparação com esse ELF.

O harness executa `0xb3fc50` ou os wrappers `0xb1d114`/`0xb23b74`, o router
`0xb27c1c`, os wrappers de comando necessários, setter `0x9575dc`, getter
`0xcd7540` e transição `0xb1c050` com os helpers reais admitidos. A vtable
concreta `0xe6bca4` aponta `+0x70→0x9575dc`, `+0x84→0xb085bc` e
`+0x134→0xb31c64`. O último helper é um `BX LR` **nos bytes originais**;
não foi introduzido como stub.

O harness novo possui seus próprios hooks de observação e proteção. Reusa
somente a inicialização de memória/registradores do dono anterior: não
herda o hook que interrompia a antiga investigação antes do router.
Não altera instruções, LR ou PC para simular chamadas. Verifica retorno
real, stack e registradores preservados pela ABI, leituras inicializadas,
escritas permitidas e bytes inalterados fora dos campos graváveis.

O recurso real é LMT entry **92608**, SHA-256
`c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8`.
Seu slot 8 tem 199 frames e loop raw 0; o slot 24 é vazio. Cada um dos
quatro grupos do slot 8 tem um registro de máscara zero/duração 199.
A colocação desse recurso no banco 0 é **sintética e explícita**, não uma
associação recuperada da partida. Os fixtures nativos preservam a tabela
inteira: 136 slots, 103 ações presentes e 597 registros de eventos, em vez
de fabricar entradas vazias para os slots não consultados pela composição.

O estado de player/model/global é sintético, com valores não zero, derivado
dos estados declarados do oráculo anterior. O snapshot das três tabelas
`0x104e048..0x104e05f` vem dos bytes inicializados do ELF; não se afirma que
sejam os valores atuais em uma partida. O vetor `0x10e6370` permanece
desconhecido/ausente e não é lido neste perfil.

As chamadas que chegam ao comando usam o bit `2` inicial de flags da layer0
para evitar a dependência de sampling ainda aberta. As exceções com flags
zero retornam antes desse ramo por selector igual, ou pelo estado geral 3.
A ausência de banco é um caso declarado separado. Foram exercitados modos
0/1/2, count234 igual a 1/2, os três caminhos selector24/selector8, flags
0/1, tipos 0/2/9, os dois valores do gate local e conversões como −1, 0, 4,
16.777.217 e INT_MAX. Não se atribui nome de gameplay a esses números.

## Resultados e reprodução

Oráculo final: `arm-final/arm-validation.json`, SHA-256
`1865d7381c2b0977068110930de28ad597a973102de9d42c9982cc4b650377c0`.
Unicorn 2.1.4, ARM11MPCORE, FPSCR 0. Nos casos positivos foram executadas
204.366 instruções, 73.239 leituras e 53.194 escritas; 11 casos não pedem
setter. Medidos 5,124 s de parede, 5,181 s de CPU e 152.289.280 bytes de
pico RSS deste processo no macOS. Cache frio, memória da família de
processos e tokens não foram medidos.

Quatro omissões deliberadas — count234, banco, header da ação e registro de
evento — são recusadas no primeiro acesso. Um quinto caso com o gate de
sampling liberado alcança `0x958884` e é recusado como dependência não
mapeada. Ele não é contado entre os 257 retornos completos.

`generate_native.py` verifica hashes, usa os donos de projeção e transforma
os bytes observados em fixtures; não calcula o estado esperado a partir
do C++ ou de sua fórmula. `generated-final/manifest.json` tem SHA-256
`fb182f827750a389f5e552fff853dcf5e440b6cf7bf3259ee12be24de317eb93`.

Normal e ASan/UBSan passaram **3.853 checks cada**, zero divergências de
estado completo declarado, argumentos ou resultados do setter. Asserções
adicionais verificam os erros das três fases, ausência de mutação na
rejeição, recursos não consultados nos retornos antecipados e projeção
bit a bit, inclusive −0 e NaNs preservados. A revisão independente conferiu
os pins, a ordem dos traces, o alias e executou os dois binários novamente
sem divergência ou diagnóstico de sanitizer.

Relatório nativo: `native-validation.json`, SHA-256
`925b014f785360df117f2bde2f7274565ab199fc271bd8499da0f5c707f59006`.
Ele fixa fontes, snapshots, compilador, comandos, binários e fixtures.
A primeira compilação de snapshot em `native-final/` falhou por omissão do
novo header dependente `motion_event_kernel.hpp`; foi preservada. O snapshot
completo e ambas as execuções aprovadas estão em `native-complete/`.

Para reproduzir os produtores, usar destinos novos:

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/player-controller-step-v1/verify_arm.py --output NOVO_ORACULO
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/player-controller-step-v1/generate_native.py \
  --oracle NOVO_ORACULO/arm-validation.json --sha256 SHA_OBSERVADO \
  --output NOVOS_FIXTURES
```

Para compilar a suíte, definir `MH4U_PLAYER_CONTROLLER_STEP_GENERATED`,
incluir `player-controller-step-v1/generated-final` e
`player-transition-runtime-v1/generated`, e vincular os donos de seleção
e comando. O relatório nativo contém os argv completos da compilação
standalone; `validate_native.py` recusa sobrescrever seu destino congelado.
Os headers derivados da ROM permanecem privados. A integração CMake é feita
pelo dono do build.

Ainda faltam a origem da escolha de estado/modo, associação real de bancos,
sampler/binder e políticas temporais para unir isso ao controle de uma cena.
Não foram demonstrados animação visual comandada por essa composição,
combate, fidelidade de gameplay ou comportamento de rede. Este ensaio
executou os binários nativos no macOS; execução Windows fica fora dele.
