# Aplicação nativa das animações pendentes

`motion_pending::apply`, em `src/motion_pending.hpp/.cpp`, implementa
`0x9577ec` compondo os donos já existentes da inicialização de animação e da
restauração de joints. O [contrato recuperado](motion-pending.md) contém a
proveniência REA e a ordem original. A função não avança relógios, calcula a
pose animada dos joints ou dispara comandos de gameplay.

## Estado e dependências

`State.layers` contém `motion_initialization::State` e somente os dois vetores
adicionais escritos em `L+98` e `L+c8`. Frames, flags, eventos e channels
continuam com os tipos anteriores. `State.joints` usa `joint_pose::Joint`;
`State.snapshots` possui os índices, flags e transforms da tabela de transição
`B`. As três estruturas são independentes, como nas alocações originais.
As extensões dos vetores de layers/joints são as contagens ativas explícitas
`C/N`; capacidade extra da alocação original não é inferida desses valores.

O recurso rModel tem uma única representação, `Parameters.model_reference`,
com presença e pointer da tabela de matrizes conhecidos, nulos ou desconhecidos.
Os templates T/Q vêm dos mesmos campos de `initialization.templates` usados
pelo initializer. O único template adicional é S, `scale_10e6380`. Não há
segunda cópia independente dos valores globais em um objeto de reset.

Um `ActionResolver` resolve exatamente o banco e slot solicitados. Ausência
explícita de banco, tabela de ações nula ou slot vazio pode retornar `nullopt`.
Desconhecimento, erro de leitura ou índice fora do recurso são erros, sem
conversão para ausência. A identidade, banco e slot da ação presente são
conferidos antes de usá-la. O sampler conserva seu resolver canônico de
`motion_initialization`; os dois adapters usam o mesmo Resource imutável no
teste composto. Política de montagem dos bancos e mapa do caçador real ainda
não são escolhidos por esse módulo.

As views e os recursos precisam sobreviver a `apply`. O resultado não retém
views nem callbacks. Os erros conservam a layer/joint e o erro específico do
resolver, initializer ou restaurador. Toda modificação acontece em uma cópia
limitada do estado; falhas tardias não publicam snapshots parcialmente trocados.

## Comportamento

Sem layer pendente, retorna o estado sem consumir recursos, templates ou
matrizes. Com pendência e nenhum selector ativo, limpa o bit de modelo ativo;
rModel explicitamente nulo retorna nesse ponto, preservando as pendências.
Se o modelo está presente, copia os transforms atuais para B **antes** de
chamar `joint_pose::restore`. O dono gravado é a maior layer pendente.

Com selector ativo, percorre as layers de trás para frente, até a primeira
pendente. As layers não pendentes de peso exatamente +1 participam das flags
temporárias da transição. Para cada layer pendente, marca os snapshots
necessários, limpa somente track/weight dos channels, conserva cursor/tempo,
zera bindings de eventos e preserva suas máscaras. Reinicializa os vetores
especificados pelo original e chama o [initializer completo](motion-initialization-runtime.md),
inclusive quando a ação é explicitamente ausente. Depois aplica as cópias de
transição e preserva somente o bit20 final das flags de B.

A comparação de interpolação usa `int32(bits(interpolation)) > 0x34000000`.
O predicado de peso positivo é a comparação IEEE do original: +Inf é positivo,
NaN é unordered e ±0 não é positivo. O restante diferente de ±0 inclui NaN
e acrescenta o bit100 da layer; o caso zero não limpa esse bit já existente.
Esses predicados são feitos sobre bits no C++, evitando dependência de FTZ/DAZ
do host em um caminho que apenas copia dados. Aritmética efetivamente consumida
continua sujeita aos domínios do initializer, sampler e restaurador.

Os limites padrão de oito layers e 256 joints são políticas de trabalho/cópia,
não máximos gerais do formato. Todos os vetores que serão copiados são
limitados antes da alocação. Tabelas de snapshots e channels precisam cobrir
os joints quando acessadas; falta de uma tabela não é reparada por criação
de canais. As allocations originais são assumidas válidas e disjuntas;
aliasing e overflow de pointers ARM não pertencem ao perfil.

## Verificação e limite atual

Os testes públicos verificam as ausências/desconhecidos, resoluções em ordem
decrescente, identidade trocada, reset seletivo, flags persistentes, snapshots
antes da restauração, erros transacionais, budgets e predicados IEEE. O teste
composto em `motion_pending_resource_tests.cpp` carrega os LMT pelo
`motion_resource::Resource` e compara o resultado com a função ARM inteira e
seus helpers reais. Não usa resultados C++ como oracle e não mantém outro
parser LMT.

O [oracle completo](motion-pending-oracle.md) contém **1.664 execuções ARM**,
incluindo 122 ações reais e 288 casos de predicados especiais. O C++ comparou
**17.448.568 bytes** de arenas, com **259.170 checks** e cinco rejeições typed;
os testes públicos acrescentaram **190 checks**. Ambos passaram em builds
normal e ASan/UBSan, com fontes e entradas fixadas antes/depois. Os resultados
incluem a ordem dos getters, as chamadas ao initializer/reset, a preservação
do estado de entrada e as regiões que não deveriam mudar. O digest da fixture
é `0048bd4f08d087312c59f7c96ff02339612d085375816d7c79801b22f6dbe67b`.

Os relatórios privados estão em `research/private/motion-pending-runtime-v1/`
(producer ARM/fixture/comparação) e `research/private/motion-pending-native-v1/`
(builds normais e sanitizados, inputs fixados e manifesto). Os mapas, bancos,
estado de entrada e snapshots globais desses ensaios são explicitamente
sintéticos; headers e chaves das ações reais vêm dos LMT fixados. Isso ainda
não prova o estado inicial do caçador, o scheduler completo ou a transição
visível durante uma caçada. A integração de build macOS/Windows é registrada
separadamente e não afirma execução em Windows.

A próxima ligação é a [atualização de pose e a mistura das transições](joint-update.md),
reutilizando esses mesmos joints, snapshots e channels. O visualizador mantém
por enquanto o controlador de diagnóstico.
