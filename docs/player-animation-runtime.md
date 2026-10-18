# Composição persistente da animação do jogador

`player_animation` liga o controlador recuperado ao frame de animação, à mistura de transição e à saída hierárquica de joints. O componente recebe um estado inicial explícito com duas camadas ativas e oito posições de armazenamento. Ele não cria o jogador original, escolhe equipamento, associa teclas, decide um estado de gameplay ou inventa uma taxa de execução.

## API e dono do estado

`State.frame` é o `model_animation_frame::State` canônico: contém somente as duas camadas ativas, modelo TRS, pose J e snapshots B. `State.inactive` contém as seis camadas físicas restantes. Não existe outro `mp::State` persistente. `State.controller` guarda os campos auxiliares do controlador, sem duplicar `layer1.weight`; o compositor existente projeta esse peso antes da seleção e novamente depois do pedido, antes da transição.

`Hierarchy` guarda apenas matrizes produzidas, metadata canônica, profundidade, ordem, flags e fatos explícitos de callback/owner. Cada pose permanece em `frame.animation.joints`. O modelo TRS e os flags de cálculo também não são duplicados nessa estrutura. O chamador inicializa todos os bits do estado; nenhuma matriz ou template omitido é tratado como identidade.

- `apply(state, command, command_context)` executa o controlador Common ou Wrapper sobre uma projeção temporária e limitada das oito camadas. Reinsere o prefixo ativo e as seis inativas no mesmo resultado. Preserva pose, snapshots e hierarquia.
- `step(state, frame_context)` compõe `model_animation_frame::step` (9573b8 e seus donos), `motion_transition::apply` (95b490) e `joint_output::apply` (95b940), nessa ordem. Preserva o controlador e as camadas inativas.
- `tick` executa `apply` e depois `step`, devolvendo somente o resultado completo. Uma falha posterior não publica o prefixo que já foi calculado.

As operações devolvem `Output` ou `Error`, com a fase e a razão tipada do dono correspondente. Observações de pedidos, queries, amostras, loops, eventos e atualização de joints ficam em `CommandEffects`/`FrameEffects`. Essas observações não são comandos para reaplicação. Erros de identidade de track, resolução e segunda query permanecem aninhados, sem convertê-los em canais ausentes.

O contexto fornece bancos, globals, constantes, resolver de tracks/ações/eventos, remapeamento de ossos e referências de modelo/pai. Os donos `motion_resource`, `motion_command`, `motion_sampler`, `motion_pending` e `motion_clock` continuam interpretando os mesmos fatos; não há outro parser LMT, sampler, relógio ou álgebra float neste driver. Os recursos e spans são emprestados durante a chamada. Efeitos externos de resolvers pertencem ao chamador e não podem ser desfeitos pelo resultado transacional.

O único delta vem de `FrameContext.pending.initialization.model_delta_time_bits`. Não existe contador de tempo independente, passo SDL, FPS presumido ou loop de glTF. Metadata usada pelo frame e pela transição vem da mesma `Hierarchy` usada para formar as matrizes. As projeções temporárias para os donos existentes são limitadas e não são retidas como um segundo estado.

## Fronteiras

O perfil exige exatamente duas camadas ativas e seis inativas; não reduz uma entrada inválida para uma camada. A contagem de poses deve coincidir com a contagem de joints de saída. Antes de copiar `State`, valida joints, snapshots, ordem e a soma dos bindings de todas as oito camadas. Defaults: 256 joints, 256 snapshots, 256 itens de ordem e 2.048 bindings. Cada dono mantém ainda seus limites de trabalho específicos. Falhas de capacidade/alocação retornam `resource_limit`.

Campos não consumidos, inclusive NaNs brutos nas camadas inativas e padding de T/S, são preservados. A política de aritmética finita, binary32, ambiente e máscaras pertence aos donos já validados. Parent desconhecido, callback desconhecido consumido, recurso não resolvido, domínio numérico inválido ou bootstrap incompleto não recebem um resultado substituto.

O tipo pode receber os ramos de seleção já fechados pelo dono `player_selection`. A prova ARM nova deste componente cobre estados fonte 1 e 3; não é uma prova adicional do estado 0. A extensão de campos opcionais 6f9/358/7fc é preservada, sem preencher valores omitidos.

## Oracle do controlador live

A fonte executada é o ELF SHA256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. `research/private/player-animation-driver-v1/verify_controller_arm.py` reutiliza os donos históricos do controlador e de `motion_command` para executar:

`b3fc50 / b1d114 / b23b74 → router/wrappers → b04e4c → 958884 → samplers reais → 9575dc → b1c050 e helpers`.

Os canais são previamente preparados pelo 9577ec original, com LMTs 54913 e 92608, em bancos explícitos 3 e 11. O banco 0 de destino também é uma associação explícita de teste. O bit pending 2 é removido do snapshot já preparado para alcançar o ramo de consulta live; não há bypass, stub ou substituição do retorno do sampler. Cada caso conserva as oito camadas físicas. As seis inativas possuem dados próprios e descritores/cursores/eventos explicitamente nulos no produtor.

São 501 chamadas completas: 281 Common, 49 Wrapper0, 171 Wrapper2; 498 com estado fonte 1 e três com estado 3. Os modos são 0, 1 e 2. Nove casos têm banco de destino ausente. Observações originais: 980 queries, 3.920 chamadas do sampler, 980 setters (324 aceitos) e 498 transições. Os cenários abrangem as 122 ações reais dos dois LMTs e preservam os cenários discriminantes do controlador histórico.

O snapshot de curvas 104e048..104e05f desse oracle é **explicitamente o conteúdo inicial do ELF**, não o valor do jogo após inicialização. A evidência posterior do inicializador d3f6fc está em `player-gameplay-bootstrap-v1/curve-init-first/curves-post-init.bin` (SHA256 `283bcb5502bd9e7c2daf2245fa36170f8f1f838accd1ed146ff3ba0f3e03b922`): `[0,1]`, `[0x3eaaaa3b,0x3f2aaa3b]`, `[0,0]`. O driver recebe esses globals pelo contexto e não possui default de bootstrap. A nova prova não reclassifica snapshots antigos como estado inicial original.

A execução usa Unicorn 2.1.4/ARM11MPCORE, instruções originais e FPSCR 0. Hooks somente observam chamadas e verificam intervalos de código, leituras inicializadas, writes permitidos, retorno, limites e registradores preservados. Os LMTs são read-only e relidos. Controles que retiram active_count, template, bytes fonte de track ou orçamento são detectados. As contagens são 810.073 instruções, 285.645 leituras e 171.414 stores observados, sem alegação de execução em hardware 3DS.

Fixture canônica: `arm-complete/oracle.bin`, SHA256 `16c6c82d3c1a635177c6823e21b915aa3876e3941e212803d02ae00ae6ceabad`. Ela conserva integralmente quatro regiões antes/depois: player 45.056 B e pbw/pw/global 4.096 B cada. O adapter C++ compara 28.729.344 bytes finais por execução, argumentos/retornos observados, identidades e imutabilidade do input. Uma falha deliberada na segunda query preserva o motivo 710 e índice 1, sem publicar o avanço parcial do cursor.

`source-closure-final/validation.json` registra replay original com 23 donos Python locais fixados antes/depois. O replay reproduziu a fixture canônica byte a byte: cerca de 17,0 s de parede, 18,4 s de CPU e 338.051.072 B de pico do processo filho no macOS. O relatório distingue esses valores do custo de compilação/validação nativa; tokens de toda a tarefa e comparação de desempenho com outra arquitetura não foram medidos.

## Validação da composição nativa

O teste de fronteiras cobre separação de donos, ordem transição→hierarquia, pai antes do filho, reconstrução de ordem dirty, camadas inativas com bits não finitos não consumidos, preservação dos novos opcionais do controlador, equivalência `tick` versus `apply`+`step`, erro tardio de parent desconhecido e limites antes da cópia. O primeiro teste sintético usava `State s` com arrays raw herdados indeterminados; o produtor foi corrigido para `State s{}` antes de preencher valores explícitos. Fonte e falha anteriores estão preservadas em `test-initialization-before/` e `boundary-first.json`; não houve correção especulativa no runtime.

O teste de recursos acrescenta 32 quadros persistentes, quatro casos com oito joints, sobre os LMTs reais e uma hierarquia sintética explícita. Executa `tick` seguido de sete `step`, verificando tamanhos dos donos, matrizes finitas, produção dos oito joints e consumo real dos samplers. Isso valida a ligação entre componentes previamente comparados ao ARM; **não é um oracle ARM novo de toda a cadeia de 32 quadros** e não prova que a hierarquia sintética seja um caçador original.

A validação final está em `native-final/validation.json`: 38 checks de fronteira e 30.762 checks de recursos em cada build normal e ASan/UBSan estrito, com `-fno-sanitize-recover=all`, stdout exato e stderr vazio. Dez processos negativos usam fixtures alteradas, truncadas, com tail, magic inválido e contagem excessiva; SHA recalculado permite alcançar o parser/comparador. Fontes e headers transitivos, fixture e recursos ficam fixados antes/depois. Tentativas e builds intermediários permanecem como histórico, separados da evidência final.

O componente não demonstra bootstrap completo de uPlayer, máquina de estados de combate, mapeamento físico de input ou coop. Não altera launchers, pacotes ou processos persistentes. O consumidor do mundo decide quando emitir comandos e chamar `step`, mantendo explícitas as políticas ainda não recuperadas.
