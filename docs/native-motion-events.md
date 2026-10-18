# Consumo nativo dos eventos LMT67

`src/motion_events.hpp/.cpp` lê um pacote binário local de **56.576 bytes** e
consulta as máscaras dos 701 registros recuperados. O pacote inclui todos os
204 slots dos dois LMT: 122 ações interpretadas e 82 slots vazios. Os quatro
grupos, os 32 valores u16 de cada prefixo e cada par mask/duration permanecem
distintos. A consulta numérica de prefixos é descrita abaixo; não há
interpretação de comandos, ataques ou callbacks.

O produtor `tools/resources/native_motion_events.py` recebe o hash do inventário
e os hashes dos dois relatórios ARM. Para cada fonte, lê os LMT retidos e exige
igualdade completa do inventário reconstruído por `animation_probe.parse_lmt`
e `lmt_events_probe.parse_events`. Os valores consultados são produzidos pelo
dono `kernel_mask`/`range_mask`; C++ não interpreta LMT nem JSON.

O pacote em `assets/local/motion-events-native-v1/motion-events.bin` tem SHA-256
`7e95034eeaa35e43c8b0967ab754ff93dfcea4fbce58929cfdd824f527eac209`.
O inventário original, os pacotes pareados e o pacote de curvas nativo v2
continuam intactos. O novo catálogo não contém política de tempo ou dispatch.

## API para integração

Namespace `mh4u::motion_events`; erros são `Result<T> = variant<T, Error>`, com
`ErrorCode`, mensagem e byte offset. O arquivo gerado é
`assets/local/motion-events-native-v1/motion_events_identity.hpp`; sua função
estável é `mh4u::motion_events::generated::expected_package()`.

Além das consultas por intervalo, `Package::first_prefix(identity, group, mask)`
retorna `Result<std::optional<PrefixHit>>`: o índice do menor bit ativo e o
`u16` correspondente na tabela de 32 entradas. Máscara zero em tabela presente
produz `nullopt`; um valor selecionado igual a zero continua sendo um hit.
Identidade incorreta, ausência e rejeição da tabela mantêm os erros do método
`group()`, inclusive para máscara zero. Não há fallback silencioso para zero.

Essa operação numérica foi isolada do consumidor original `0x94be10`, que usa
o grupo 0 no contexto analisado. A API permite consultar qualquer tabela
explicitamente selecionada, sem afirmar que todos os grupos ou atores seguem
a mesma política. Ela não detecta bordas da máscara, não dispara callbacks e
não está ligada automaticamente ao controlador do caçador. Evidências e
diferenças de escopo estão em [motion-event-dispatch.md](motion-event-dispatch.md).

```cpp
#include "motion_events.hpp"
#include "motion_events_identity.hpp"
namespace events = mh4u::motion_events;

auto expected = events::generated::expected_package();
auto loaded = events::Package::load(binary_path, expected);
// Tratar Error antes de obter Package. Mantê-lo vivo durante group().
auto& package = std::get<events::Package>(loaded);
events::ActionIdentity id{54913, 3, expected.sources[0].lmt_sha256};
auto strict = package.range(id, 2, 6, 6);      // Result<uint32_t>, mask1.
auto raw = package.kernel_range(id, 2, -1, -1); // Sem clamp ou ordenação.
```

`ExpectedPackage` deve vir de um pin selecionado independentemente do binário.
`identity.json` é uma representação para inspeção/produção; o loader não o lê
automaticamente ao lado do arquivo e não confia em um digest embutido no próprio
binário. O header contém identidades, estados e contagens derivados, sem masks,
durações ou prefixos. Ele é saída local privada e não exige editar C++ a cada
nova produção.

| Operação | Contrato |
| --- | --- |
| `load(path, expected, Limits={})` | Leitura limitada e SHA externo antes de decodificar |
| `decode(string_view, expected, Limits={})` | Mesmo parser, sem acesso ao filesystem |
| `range(ActionIdentity, uint32 group, int64 begin, int64 end)` | Frames inteiros inclusivos; exige `0≤begin≤end<N` |
| `kernel_range(ActionIdentity, uint32 group, int32 begin, int32 end)` | Entradas signed32 brutas; permite negativos, invertidos e fora de N |
| `group(ActionIdentity, uint32 group)` | `Result<const Group*>`; prefixo raw, registros, soma e disponibilidade do núcleo |
| `actions()` / `sources()` | Metadados imutáveis, em ordem source-bound |

O ponteiro retornado por `group()` dura enquanto o `Package` existir e não for
movido. `ActionInfo.frame_count` é optional: um slot vazio não tem header de
ação e não recebe um N fabricado. Identidade é sempre entry ID + slot + SHA do
LMT, não nome de arquivo, índice do clip ou grupo isolado.

Estados de ação: `empty_slot`, `events_absent`, `parsed`, `rejected`. Presença
dos grupos: `unavailable`, `absent`, `present`. Um estado rejeitado conserva o
código do erro do dono. Consultas devolvem `empty_slot`, `events_absent` ou
`events_rejected` separadamente; um grupo presente com máscara zero devolve
sucesso com valor zero. Prefixo zero também é preservado e não significa
ausência. Esses estados alternativos têm fixtures sintéticas; o corpus real
contém somente ações parsed e slots empty.

## Núcleo signed sem política temporal

`lmt_events_probe.kernel_mask()` passou a ser o dono Python do núcleo;
`range_mask()` valida o domínio e chama esse dono. Snapshots e inventário
anteriores permanecem idênticos. `kernel_range()` nativo executa a mesma ordem:

```text
cumulative = 0; mask = 0
para cada registro:
    cumulative += duration
    se begin < cumulative: mask |= record.mask
    se cumulative > end: terminar
```

O resultado de uma consulta invertida não é definido por interseção geométrica.
Não se ordenam os endpoints e não se usa clamp. N=1 pode produzir `[-1,-1]` no
planejador original; o núcleo pode retornar a primeira máscara nesse caso.
Essa particularidade foi executada no ARM, e não inferida de uma função de
intervalos convencional. Planejar as consultas do advance, lidar com freeze e
combinar os dois passes são responsabilidades do consumidor, fora deste módulo.

Durações zero são conservadas; a soma cumulativa é calculada em u64 ao carregar.
Quando ela ultrapassa `INT32_MAX`, `group()` ainda permite inspecionar os dados,
mas as consultas retornam `unsupported_accumulator`. Isso evita aplicar
comparações signed sem modelar overflow. A soma diferente de N não é ajustada
ou descartada. Todas as 488 somas do corpus real são N, conforme
[lmt-events.md](lmt-events.md).

## Container v1

Little endian explícito, sem conversão numérica ou quantização. Limites padrão:
8 MiB de pacote, oito fontes, 4.096 ações, 250.000 registros agregados e 96 bytes
para um código de rejeição. Counts e bytes restantes são verificados antes de
reservar memória. Todos os slots declarados por cada fonte devem ter exatamente
uma identidade; IDs, grupos repetidos, estados incoerentes e bytes excedentes
são rejeitados.

| Bloco | Campos em ordem |
| --- | --- |
| Header,32 bytes | Magic8 `MH4UME01`; u32 marker `0x01020304`, total bytes, source count, action count, kernel profile1, reserved0 |
| Binding,32 bytes | SHA-256 raw do inventário |
| Fonte,44 bytes | entry u32, LMT SHA raw32, slot count u32, reserved0 |
| Ação,32 bytes | source index, slot, N ou0 para empty, state, reason length, group count4, reserved0, reserved0; todos u32 |
| Rejeição | ASCII lowercase/números/underscore de tamanho declarado, sem terminador |
| Grupo,16 bytes | ordinal0..3, presence, record count, reserved0; todos u32 |
| Grupo present | 32 × u16 prefix raw, depois count × `[u32 mask,u32 duration]` |

Grupos absent/unavailable não armazenam prefixo nem registros. N=0 serializado
em empty representa ausência do campo por esse estado, nunca uma ação de zero
frames. Action state0/1/2/3 significa empty/absent/parsed/rejected; group
presence0/1/2 significa unavailable/absent/present. Profile1 identifica o
núcleo signed sem overflow, não um decoder LMT novo.

O hash externo pina todos os bytes de prefixo/registros; o parser compara ainda
SHA do inventário, fontes, domínios e estados contra `ExpectedPackage`. Não há
dependência de imagens/glTF neste container: o pareamento com juntas usa a
mesma identidade entry/slot/LMT SHA, e o outro pacote possui seus próprios
bindings de arquivos.

## Evidência e testes

O `range-oracle.tsv` tem **97.017 consultas**. As 89.697 consultas estritas são
o conjunto anterior completo: o produtor compara os hashes dos resultados,
em ordem por grupo e por kernel, ao relatório ARM fixado. As 7.320 consultas
signed adicionais foram executadas nos mesmos dois núcleos originais,
`0x958638–0x958678` e `0x9586ac–0x9586ec`, antes de produzir o pacote.

O relatório novo é
`research/private/motion-events-native-v1-arm/signed-arm-validation.json`:
14.640 execuções ARM sobre os 488 grupos reais, mais 114 execuções sintéticas
com N=1, grupo presente vazio, masks zero, bit31 e duração zero. Inclui
`INT32_MIN/INT32_MAX`, negativos, endpoints invertidos e fora de N. Zero
diferenças de máscara e zero acesso fora dos spans permitidos. O harness usa
Unicorn 2.1.4 já instalado, o ELF fixado e somente os 128 bytes dos núcleos;
não executa o jogo, o planejador, callbacks ou serviços.

O teste C++ completo passou **304.245 assertions**, sem divergência nas 97.017
queries. Verificou 10.538 truncamentos reais, counts hostis em todos os 816
descritores de grupo, cada corte do fixture sintético, hashes/identidades
divergentes, versões/reservados inválidos, duplicatas, limites agregados,
overflow, estados ausentes/rejeitados e arquivo local alterado. Passou também
com AddressSanitizer e UndefinedBehaviorSanitizer. Testes Python: dez do dono
e quatro do produtor, incluindo readback integral com identidade e edição
detectada do inventário mesmo após novo pin do JSON.

Compilação standalone (sem modificar o build usado pelo projeto):

```sh
clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -DMH4U_MOTION_EVENTS_GENERATED -Isrc -Ivendor/picosha2 \
  -Iassets/local/motion-events-native-v1 \
  src/motion_events.cpp src/motion_events_tests.cpp \
  -o research/private/motion-events-native-v1/motion-events-tests
research/private/motion-events-native-v1/motion-events-tests \
  assets/local/motion-events-native-v1/motion-events.bin \
  assets/local/motion-events-native-v1/range-oracle.tsv
```

Sem o define e sem o include local gerado, o mesmo teste roda somente os
fixtures sintéticos. Para sanitizadores, acrescentar `-O1 -g
-fsanitize=address,undefined -fno-omit-frame-pointer`. O módulo não depende de
SDL nem altera `root_motion`, `action_motion`, viewer, CMake ou pacotes anteriores.
Os testes isolados do módulo não demonstram a política do planejador. A
verificação de processo abaixo cobre o perfil diagnóstico que o integra.
Não foi executado Windows nesta validação standalone.

Identidades, inputs, script de reprodução, snapshots e logs finais ficam em
`research/private/motion-events-native-v1/`. O header/API não atribui qualquer
significado ao prefixo; falta o consumidor que liga bits a handlers de gameplay.

## Comparação dos processos nativos

`tools/action_events_smoke.py` compara o perfil
`diagnostic-one-layer-events-v1` da CLI e, opcionalmente, o modo `--inspect-only`
do viewer com o oráculo ARM retido. Exige pins externos do pacote de movimento,
do pacote de eventos, do manifesto pareado e do manifesto do oráculo. Confere os
hashes e tamanhos dos TSV, as identidades entry/slot/LMT SHA, N e clip index.
Os executáveis e inputs são novamente conferidos ao final da execução.

O formato numérico de TICK e suas tolerâncias pertencem ao helper existente
`action_motion_smoke.tick_values`: vetores têm tolerância absoluta de `1e-4`,
quaternions `1e-5`, campos de relógio/flags são exatos. O perfil legado continua
sendo o default desse helper. EVENTS tem parsing independente como uint32
decimal: nenhuma máscara é convertida para float ou comparada por tolerância.

O relatório `research/private/motion-events-native-v1/action-process-validation.json`
registra **2.604 TICK e 2.604 EVENTS** nas 14 sequências das sete ações, em loop
e clamp. São **20.832 máscaras** atuais/anteriores exatas, com erro máximo de
vetor `1,1920928955078125e-7` e de quaternion zero. O viewer passou em **68
inspeções**, incluindo início, meio, final, passagem pelo loop e o primeiro
evento não zero atual/anterior quando observado. Os quatro controles negativos
rejeitaram pacote ausente/alterado, tanto na CLI quanto no viewer, antes de
publicar qualquer TICK/EVENTS/MOTION_STATE/MOTION_EVENTS ou inicializar OpenGL.
Foram 86 processos, 2,02 segundos nessa execução local; CPU e pico de memória
da família desses processos não foram medidos. Não há execução ARM nova nesse
comparador: ele verifica os resultados retidos, sem abrir GUI ou executar o jogo.

Exemplo com o conjunto local fixado:

```sh
python3 -B tools/action_events_smoke.py \
  --binary build/mh4u-action-motion \
  --package assets/local/model-motion-native-v2/model-motion.bin \
  --package-sha256 ffb34e83d3f807272ca41b3a9d4d4cf4414735203a3cceaa4cd5d7ec2166d139 \
  --events-package assets/local/motion-events-native-v1/motion-events.bin \
  --events-sha256 7e95034eeaa35e43c8b0967ab754ff93dfcea4fbce58929cfdd824f527eac209 \
  --paired-root assets/local/paired-player-actions-v1 \
  --paired-sha256 30f85c1c2707cad8406b42d3e07bc2be2945f20ee781dbec3c05de075ffca261 \
  --oracle research/private/root-motion-runtime/real-events-sequences-v1 \
  --oracle-sha256 e446cdde7b2194f4225a6cce77d582586b1ac882bca7efebf1006b35d15890c7 \
  --viewer build/mh4u-resource-lab
```

`--viewer` é opcional; `--output caminho-novo.json` retém relatório sem
sobrescrever um anterior. A comparação cobre essa política diagnóstica e esses
recursos; não prova combate, remapeamento de bits, callbacks ou gameplay completo.
