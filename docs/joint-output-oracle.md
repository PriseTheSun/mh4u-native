# Oráculo composto de transição e saída das juntas

O ensaio executa os corpos originais completos de **95b490 → 95b940**, com
seus helpers, getter virtual e callback normal. São **401 casos**: **399**
coincidem bit a bit com a composição nativa `motion_transition` → `joint_output`;
**2** produzem quaternion de norma zero no ARM e recebem erro numérico explícito
no perfil nativo. O Unicorn é ferramenta de análise, sem participação no porte.

As variantes normal e ASan/UBSan passaram no macOS ARM64, com **22.503 checks**
cada. Foram comparados **29.417.472 bytes de saídas ARM**, nas duas fronteiras,
e **14.929.920 bytes de verificações de imutabilidade**. O total de 44.347.392
bytes não deve ser apresentado como se fosse integralmente saída ARM.

## Fonte e execução

O ponto de partida é [joint-output.md](joint-output.md), cujo manifesto privado
tem SHA-256 `bbc9ac86489cfa8bf1975a62d137354fcd81d57de8562394e38acefcb97eb739`.
O produtor histórico e seu snapshot em `arm-delivery` são idênticos, SHA-256
`baba458b732439916f7fba6caf4ea45be48651a4ac3c9cf7277216077bf21278`.
O novo produtor verifica esses pins e reutiliza o dono da extração ELF,
intervalos de código/dados, guardas ARM, helpers de representação e os 150
cenários históricos. Seus resultados finais foram confrontados com os hashes
retidos do ensaio anterior; nenhum arquivo histórico foi sobrescrito.

O ELF fixado é `mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
A proveniência REA e os readbacks são os de `joint-output-v1`, sessão
1792291312711, já encerrada. Este recorte não abriu outra importação Ghidra.

O harness usa Unicorn 2.1.4, ARM11MPCORE, little endian. Cada função recebe
FPSCR inicial 0 e o mesmo ponteiro de modelo; a memória resultante da primeira
alimenta a segunda. Confere retorno, SP, r4–r11 e s16–s31. Os templates são
explicitamente fornecidos e devem permanecer imutáveis. Leituras exigem bytes
inicializados; escritas ficam limitadas aos campos declarados e stack. O fetch
de código deve pertencer aos intervalos fixados. Nenhuma instrução, PC,
resultado de helper ou retorno de callback é substituído.

As sete arenas são modelo (8192), juntas (8192), snapshots (4096), ordem (4096),
parent (4096), juntas do parent (4096) e mapa do parent (4096), totalizando
36.864 bytes. A fixture retém cada arena **antes**, **após 95b490** e **após
95b940**, incluindo seu complemento opaco. Isso evita que a segunda etapa
esconda um erro da primeira. Há 1203 snapshots completos verificados.

As entradas, endereços, layers, poses, snapshots, hierarquias, mapas, templates
e preenchimentos aleatórios são sintéticos e declarados. As funções executadas
e suas instruções são as originais. Não se afirma que o estado de teste foi
capturado de um frame real de gameplay.

## Cobertura discriminante

As categorias se sobrepõem:

| Recorte | Casos |
| --- | ---: |
| Históricos preservados | 150 |
| Mais de uma layer, até 8 | 205 |
| Dirty inicial não zero | 156 |
| Parent presente, inclusive tipo inválido | 73 |
| Ponteiro rModel explicitamente nulo | 8 |
| Seleção de callback nulo | 3 |
| Flag40 selecionando callback normal via flag100000 | 3 |
| Ordem limpa com índice repetido | 2 |
| Normalização de quaternion zero | 2 |

O conjunto inclui até 32 juntas, chains em ambas as direções, várias raízes,
empates de profundidade, pais ainda não atualizados e juntas com flag20.
**89 casos** distinguem os swaps exatos de pares de `95b39c` de um stable sort;
`artifact-audit.json` preserva profundidades, ordem original e alternativa
divergente. Nenhuma ordenação abstrata foi usada como substituto do original.

Os snapshots selecionam layers sem exigir que a layer proprietária tenha
flag100: basta que a varredura encontre essa flag em alguma layer. Sentinel,
flags41 e peso ±0 são exercitados na ordem original. Owners fora do limite são
ignorados somente quando um skip anterior impede sua leitura. Paddings raw
de T/S, flags de snapshot e complemento opaco são preservados; quando a mistura
é consumida, os paddings T/S são zerados conforme o original.

O getter real `d02258`, via vtable uPlayer, usa índices negativos, entrada
sentinel255 e redução ao byte baixo. Um caso usa índice `0x12345601` com bytes
distintos no mapa, evitando que um mapa uniforme mascare a redução. Modos de
parent 0/1/2/3 e valores em torno do limiar `0x34000000` são exercitados. A
matriz RT aceita quaternion zero sem normalizá-lo; isso é diferente dos dois
casos em que a mistura efetivamente calcula norma zero.

Translation raw do modelo inclui NaN, infinito e −0 em três casos sem parent
ou recurso que consumam aritmeticamente esses valores depois. Callbacks e
owner estrangeiros também aparecem em estados nos quais não são consumidos.
Callbacks alternativos de transformação continuam fora do perfil executável.

O trace original contém 401 chamadas de cada `6d385c`, `1a63fc` e `6d3948`,
393 de cada `95de18` e `95f114`, e 2116 de `9608d8`. Essas contagens incluem
os dois casos degenerados: o resultado nativo bem-sucedido contém 2111 juntas
atualizadas e 739 juntas misturadas. Não são contagens de frames de gameplay.

## Adaptação nativa e contratos

`src/joint_output_resource_tests.cpp` consome os donos canônicos:

- `joint_pose::Joint`, incluindo os quartos words de T/S;
- `joint_update::Metadata` para flags94/mapping98;
- `motion_pending::Snapshot` para o snapshot de transição;
- `motion_request::Layer` para flags e field60;
- `motion_transition::apply` seguido de `joint_output::apply`.

O resultado da primeira função alimenta a segunda. O adaptador projeta apenas
os campos possuídos sobre uma cópia integral da arena anterior e compara as
sete arenas contra o ARM. Compara explicitamente **callback e owner** de cada
junta retornada com a entrada: copiar seus ponteiros opacos da arena anterior
não pode mascarar mudança acidental nos fatos lógicos. A auditoria identificou
que O+c4c é um store de **32 bits**, não byte; o dono corrigiu a largura antes
da validação final. Há 400 casos em que esse word muda para 2.

`MH4UJO01` é um contêiner privado de teste little endian. Cabeçalho: magic8,
quantidade u32, sete pares (endereço u32, tamanho u32). Cada record é prefixado
por tamanho e contém seed, classificação de domínio, N, quantidade de layers,
template64 e os três snapshots das sete arenas. O consumidor exige SHA-256
externo antes de interpretar e limita arquivo, record e contagens antes das
alocações dependentes. Não contém um segundo parser de LMT, nem é pacote de
animação destinado ao jogador.

```text
joint-output-resource-tests oracle.bin oracleSHA
```

Fixture autoritativa:
`research/private/joint-output-composition-v1/arm-final/oracle.bin`, SHA-256
`df1dd4403982caa13e6a4aa2140d5a92dfdf2e81f7fbb2bf99468f7aa1abe566`.
As primeiras tentativas ficam preservadas: `arm-first` interrompeu apenas o
controle negativo, pois callback de junta raiz não era consumido; `arm-complete`
tem 393 casos. `arm-final` acrescenta oito casos discriminantes e confirma
todos os hashes históricos.

Os dois NaNs originais ficam retidos na fixture e no audit. O C++ deve retornar
`motion_transition::ErrorCode::arithmetic`, componente rotation, com
`degenerate_quaternion`; essas duas saídas não entram na equivalência bit a bit.
Não se fabrica pose finita nem se continua a segunda função nativa após erro.

## Negativos, custo e limites

Cinco negativos ARM exercitam template ausente, ordem ausente, orçamento,
callback de junta consumido e callback alternativo não recuperado. São falhas
dos guardas de análise, não políticas de gameplay. No consumidor nativo há
desconhecimento de modelo/parent/template, ordem incompleta, ciclo, callback
não suportado e extensão de snapshot/owner, com preservação transacional.

Nove controles de fixture verificam digest, truncamento, trailing byte, magic,
contagem, identidade de arena, tamanho de record e limites de N/layers. Outros
dois controles recalculam o digest após alterar **somente** a matriz
intermediária ou **somente** um byte opaco final. Ambos falham na comparação
esperada, demonstrando que os comparadores de ambas as etapas são ativos.

`native-strict/native-validation.json` fixa compilador, argv, hashes de fontes,
headers, executáveis e resultados normal/ASan; SHA-256
`b3c6bd35820f229d99a568ab0211e43430b4b05f0ac3bec68cb6dce54e7e944c`.
`native-strict/comparator-controls.json` guarda os dois controles adicionais.
Compilador: Apple Clang 21.0.0, target arm64-apple-darwin25.6.0.

O snapshot autoritativo `native-strict` usa `-fno-sanitize-recover=all`, exige
stderr vazio e todas as métricas esperadas, além do código de saída zero.
Inclui `motion_events.hpp` nos pins transitivos. `native-final` e o manifesto
anterior ficam preservados: seus logs também estavam limpos, mas o produtor
anterior permitia recovery padrão de UBSan e não exigia stderr vazio. O novo
snapshot resolve essa fragilidade da validação, sem alterar runtime ou adapter.

O produtor final mediu 3,031 s de parede, 3,202 s de CPU e RSS máximo de
247.480.320 bytes; executou 853.153 instruções, 264.873 leituras e 131.148
escritas. Replays nativos estritos: 0,484 s normal e 1,093 s sanitizado; builds
2,669 s e 3,416 s. São medições locais com arquivos disponíveis. Cache frio/quente,
memória da família completa e tokens não foram medidos.

A equivalência cobre estados explícitos, armazenamento disjunto e hierarquias
de mesmo owner no ramo dirty. Getters/callbacks adicionais, árvores entre
modelos, scheduler, relógios, skinning/GPU e mapeamento às inverse binds não
foram fechados. Operações nativas exigem ambiente nearest-even, gradual
underflow e valores/intermediários finitos quando consumidos; cópias raw são
preservadas. Flags de exceção FPSCR são retidas, mas não comparadas à API.
Este ensaio não demonstra comportamento de gameplay nem execução Windows.
