# Sampler nativo com cursor e aritmética binary32

`motion_sampler` reproduz o dispatcher original `0xaea0e4` e os codecs
1, 2, 4, 5, 6, 7, 11, 12 e 13 no perfil descrito abaixo. **32.425 consultas
ARM completas** coincidiram bit a bit com o C++ nos quatro componentes
retornados e no cursor. As execuções nativas normal e ASan/UBSan passaram
**129.735 checks cada** no macOS ARM64.

O sampler recebe bytes de recursos já identificados, sem interpretar outro
LMT, importar glTF ou converter frames em segundos. Não usa como oráculo a
curva Python em double nem os resultados do sampler nativo anterior.
Codecs ainda não implementados e dependências desconhecidas geram erro.

## Contrato e propriedade

Arquivos: `src/motion_sampler.hpp`, `.cpp` e `_tests.cpp`. Namespace
`mh4u::motion_sampler`:

```cpp
Result sample(const motion_binding::Channel&,
              const TrackView* resolved_track,
              uint32_t query_frame_bits,
              Limits = {});
```

`Result` é `variant<Sample,Error>`. `Sample` contém `value_bits`, os quatro
words binary32 retornados em s0..s3, e o `Channel` atualizado. O tipo
`motion_binding::Channel` é o único dono da representação de estado:

| Campo | Papel nesta função |
| --- | --- |
| `track` | Identidade do track, preservada |
| `frame_bits` | Frame acumulado do key atual; pode mudar |
| `key_data` | Identidade/offset do key atual; pode mudar |
| `weight_bits` | Preservado, sem multiplicar o resultado |

`TrackView` é uma vista não proprietária: `TrackReference identity`, metadata
raw, quatro `reference_bits`, `optional<KeyBuffer> keys` e
`optional<array<uint32_t,8>> extremes_bits`. `KeyBuffer` contém a referência
ao primeiro key e `span<const std::byte>` limitado ao buffer original.
Scale/bias têm quatro words cada, nessa ordem. O dono imutável do recurso
verifica SHA/estrutura, resolve a identidade completa e mantém os bytes
vivos durante a consulta. O sampler confere identidade de track e recurso,
limites e alinhamento do cursor; não recalcula SHA por amostra.

Ausência e desconhecimento são distintos. Canal sem track exige
`resolved_track=nullptr` e produz `[0,0,0,1]`, preservando **todos** os campos
do canal. Track presente sem resolução é erro `unresolved_track`. Nos codecs
1/2, os quatro words da referência são copiados literalmente, sem normalizar
quaternion, ler keys/extremes, mudar cursor ou validar o ambiente aritmético.
Esses caminhos preservam até NaNs, infinidades, −0 e cursores não consultados.
É a regra demonstrada em [root-motion-defaults.md](root-motion-defaults.md).

`Limits::key_steps` limita consultas de delta, por política explícita do
chamador; padrão 1.000.000. A amostragem não aloca buffers. Erros mantêm código,
mensagem e offset pertinente, sem publicar um cursor parcialmente avançado.
Não existe fallback zero para falha de formato, identidade ou aritmética.

## Busca temporal original

Frames são bits f32 da fonte. O cursor guarda o início do key atual, não o
último frame solicitado. A busca usa operações f32 individuais:

1. Se `cursor_frame > query` e `f32(cursor_frame-query) > query`, reinicia
   o ponteiro no primeiro key e o frame acumulado em zero.
2. Na busca adiante, delta zero encerra no key atual. Caso contrário, soma
   o delta e avança enquanto o próximo início é menor ou igual à query.
3. Para interpolar, calcula `f32(f32(query-cursor_frame)/delta)`.
4. Na busca para trás, recua um stride antes de consultar o delta e subtrai
   esse delta do frame acumulado, repetindo enquanto necessário.

O detalhe de `0xaea1e8` e `0xaea248` foi corroborado nos bytes REA retidos:
ambos são `CMP r0,r4` (`0xe1500004`). `r0` contém o primeiro key, `r4` o
endereço do **Channel**; o cursor móvel está em `r6`. Não se corrigiu isso
para uma comparação com `r6`. No perfil de armazenamento disjunto declarado
por esta API, os ponteiros comparados não podem ser iguais. Um recuo que
atravessaria o início do buffer gera erro de extent, sem clamp inventado.

O dispatcher não conhece o número de frames da ação. Portanto esta função
não recorta em N−1, não aplica loop e não atribui FPS. Delta terminal zero
é que mantém o último key. Queries depois de N−1 e cursores sintéticos em
2^24 foram exercitados para distinguir essas regras de um clamp geométrico.

## Decodificadores e ordem numérica

| Codec | Stride | Delta | Dados e retorno |
| --- | ---: | --- | --- |
| 1/2 | não consultado | não consultado | cópia de track+0x10, quatro words |
| 4 | 8 | u16 em key+6 | vetor quantizado u16, scale/bias, lerp; W zero |
| 5 | 4 | u8 em key+3 | vetor quantizado u8, scale/bias, lerp; W zero |
| 6 | 8 | bits 56..63 | quatro componentes signed14, quaternion |
| 7 | 4 | bits 28..31 | quatro componentes de 7 bits, scale/bias, quaternion |
| 11/12/13 | 4 | bits 28..31 | eixo X/Y/Z e W de 14 bits, demais bias preservados |

Os fatores e conversões vêm das instruções e literais do executável. O codec
7 conserva o wrap unsigned ao subtrair 8; campos menores que 8 foram
executados em fixtures sintéticos. O codec 6 conserva a extensão de sinal
de `SXTH` e não requer scale/bias. Nos vetores, somente XYZ de scale/bias
são acessados; os words W não consultados não são rejeitados por conteúdo.

Quaternions comprimidos são decodificados **brutos**. Para fração diferente
de zero, o dot é acumulado na ordem XYZW; se negativo, inverte o segundo
quaternion. Mistura os componentes e só então normaliza, com soma ordenada
dos quadrados, sqrt f32, divisão e multiplicações. Fração zero segue a
decodificação/normalização do primeiro key sem ler o próximo. Isso preserva
a ordem ARM, sem normalizar antecipadamente os endpoints.

Cada operação é arredondada para binary32. `VMLA.F32` deste VFP2 é modelado
como multiplicação e soma **separadas**, sem FMA. Build fast-math é recusado.
Os caminhos comprimidos exigem `FE_TONEAREST` e underflow gradual; uma probe
com operandos volatile detecta FPCR.FZ/MXCSR FTZ/DAZ sem constant folding.
Somente esses caminhos fazem aritmética e consultam a probe.

O perfil comprimido requer query/cursor finitos, componentes scale/bias
efetivamente acessados finitos e intermediários finitos. Uma normalização
degenerada gera `degenerate_quaternion`. O oráculo executou separadamente
um quaternion codec6 totalmente zero: o original retornou quatro
`0x7fc00000`. A API **rejeita esse caso**, em vez de afirmar equivalência
fora do perfil finito ou produzir identidade/zero. Exceções FPSCR, todas as
regras de NaN na aritmética comprimida e modos de arredondamento diferentes
não foram implementados como estado observável.

As derivações anteriores em [quaternion-codecs.md](quaternion-codecs.md)
continuam documentando a auditoria estática e o exportador double/glTF.
Esta etapa acrescenta a execução dos consumidores ARM e a comparação f32;
não transforma os testes históricos em medições que não fizeram.

## Fontes e execução independente

ELF fixado: `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Não houve nova sessão Ghidra neste recorte. `retain_source.py` revalidou
**oito leituras REA, 1.184 bytes**, e dez intervalos LLVM/bytes já retidos.
`source-readback.json` preserva IDs, ranges, hashes e as diferenças de origem
entre Evidence REA e leituras locais do ELF.

| Evidência reutilizada | Conteúdo |
| --- | --- |
| `ev_acc88093f4d8fb85c426282fb83fa12c2d46bf8930884c3d54e81ea80b10d65f` | Dispatcher `0xaea0e4`, incluindo literais |
| `ev_48961f4696c206e583bcd8a6a85e574dd40fa782f2ddf9a1737f100b6602c7d5` | Parte da tabela de despacho, stride/getters/decoders |
| `ev_9f30f4f3d29cfe67b5e7931b86ab1bea6a5424568cac03c2dda1d9a5692c214c` | Decoder4 `0xae9910` |
| `ev_dfbc336ac239e12306c696c4a0f7022e2d6d64553b55f1ae440b0fdbe70f84a2` | Decoder5 `0xae92e4` |
| `ev_0269438971bf5485fa0d3bcb6de218f10937baf292f1620567cbd1a99da0ec22` | Getters de delta4/5 |

O harness privado `research/private/motion-sampler-v1/verify_arm.py` usa
Unicorn 2.1.4, ARM11MPCORE e FPSCR 0. Executa as instruções originais do
dispatcher, getters, decoders e helpers, incluindo `0xae96b8` e `0xdebc60`.
Não intercepta retornos nem substitui helpers. Pools de literais são
separados dos ranges executáveis. Os buffers e stack começam com poison;
leituras exigem bytes inicializados e escritas só podem alcançar a stack
declarada e os oito bytes mutáveis do Channel. O retorno real, stack,+registradores callee-saved e campos inalterados são verificados.

O corpus usa bytes das entradas LMT **54913 e 92608**, fixadas pelo inventário
de recursos. Foram 28.215 queries de **2.565 tracks em 122 ações**, mais
4.210 queries sintéticas. O parser Python dono fornece layout/bounds e
proveniência; os resultados esperados vêm exclusivamente dos registradores
e memória após a execução ARM, não de seu decoder double.

| Caminho | Queries comparadas |
| --- | ---: |
| Nulo | 16 |
| 1 | 632 |
| 2 | 599 |
| 4 | 1.974 |
| 5 | 1.556 |
| 6 | 4.753 |
| 7 | 15.885 |
| 11 | 3.224 |
| 12 | 3.213 |
| 13 | 573 |

As sequências incluem consultas crescentes e decrescentes com estado
transportado, limites de key, meios de frame, terminal zero, múltiplos keys
atravessados, reinício, dot zero/negativo, campos quantizados extremos,
subnormais e overflow unsigned de codec7. Seis controles ARM rejeitam leitura
ausente de track/key/extremes, escrita proibida, orçamento de instruções e
recuo antes do buffer. O caso degenerado é observação separada, não uma das
32.425 comparações do perfil finito.

Executadas 7.143.237 instruções, 2.012.552 leituras e 956.829 escritas nos
casos positivos. Medidos 11,436 s de parede, 12,645 s de CPU e
567.525.376 bytes de pico RSS deste processo no macOS. Cache frio, memória
da família de processos e tokens não foram medidos. Hardware 3DS real não
foi usado. As arenas são sintéticas, disjuntas e explicitamente inicializadas;
isso não demonstra alcance dessas entradas em uma partida.

## Artefatos e reprodução

Tudo deste ensaio está em `research/private/motion-sampler-v1/`:

| Arquivo | SHA-256 |
| --- | --- |
| `arm-final/oracle.bin` | `532e018e3d4e0002a15cd2b6585751f2f9ffc7bd84dba17eb8e26e9bd87c2c6f` |
| `arm-final/arm-validation.json` | `47bb6940e5a35b2cffd0ef79bb81bccaea11439c66e6eae490685450347249dd` |
| `native-validation.json` | `301aa96d9928c3257c951600f52d628c90bf581caaef7c8f905c7cef4dc135b7` |
| `source-readback.json` | `6bf6f7ff1928c89f0edad677213742e4382a3d11b6cb5c43eefe66f2108f0472` |

O relatório nativo fixa fontes, snapshots, compilador, argv e executáveis
normal/sanitized. A suíte compara os quatro words e Channel completo por
igualdade, sem tolerância. Também verifica desconhecimentos, identidades,
bounds, truncamento, limite de passos, não finitos, arredondamento, FPCR.FZ,
NaNs de referência preservados e ausência de mutação após erro. Alterar o
oráculo sem atualizar o pin e remover o arquivo produzem falhas explícitas.

Na autorrevisão foi identificada uma probe de ambiente com operandos
constantes, suscetível a ser pré-calculada. A versão anterior e seu relatório
ficaram em `native-final/`; a corrigida está em `native-complete/`.
`probe-regression.json` demonstra que o novo teste com FPCR.FZ real falha
contra o CPP anterior e passa com operandos volatile. Não foi necessário
repetir ARM: os bytes e resultados originais permaneceram iguais.

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/motion-sampler-v1/verify_arm.py --output NOVO_DIRETORIO

# A compilação standalone exata está em native-validation.json.
research/private/motion-sampler-v1/native-complete/tests-normal \
  research/private/motion-sampler-v1/arm-final/oracle.bin \
  532e018e3d4e0002a15cd2b6585751f2f9ffc7bd84dba17eb8e26e9bd87c2c6f
```

Sem argumentos, o executável roda os testes de fronteira. Com arquivo e SHA,
roda também o oráculo fixado. Não requer macro de fixture. O teste inclui
`vendor/picosha2`; a implementação só depende dos tipos de `motion_binding`.
Os artefatos ROM/fixture ficam privados, sem inclusão de bytes proprietários
nos fontes públicos. A integração CMake e o owner de recursos pertencem a
escopos separados.

Codecs 0, 3, 8, 9, 10 e outros continuam `unsupported_codec`; a tabela de
dispatch conhecida não basta para afirmar suporte. Tampouco se demonstraram
blending entre layers, skinning, aplicação de movimento, relógio ou seleção
de ações por input neste módulo. A validação nativa desta etapa executou
macOS; a execução Windows não integra estes resultados.
