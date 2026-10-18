# Conversão numérica de sMhPad

`src/input_quantization.hpp/.cpp` reproduz o bloco numérico original
`0x6947f8..0x6949b8` em C++ nativo, sem executar ARM no runtime. O oracle privado
executou **66.183 casos do bloco ARM original**, incluindo todos os 65.536
pares signed8 nos dois canais e 647 casos adicionais de redução signed32.
A comparação C++ passou em compilação normal e com UBSan.

Este é o próximo recorte de [player-input-production.md](player-input-production.md).
O documento anterior conserva seu caráter de investigação estática; aqui há
execução isolada adicional. A aquisição, calibração, gates, mapeamento físico,
botões e agendamento continuam fora desta API. Não se atribui um dispositivo,
taxa de atualização ou nome de ação aos pares.

## Fronteira e API

```cpp
#include "input_quantization.hpp"
namespace iq = mh4u::input_quantization;

const std::array<iq::PairInput,2> fields{{{32767,0},{0,-32768}}};
auto result = iq::convert(fields); // variant<Sample,Error>
```

Os pares correspondem aos inteiros de `S+ac/b0` e `S+a4/a8`, nessa ordem,
após aquisição e calibração. Cada `Pair` devolve os dois signed8 reduzidos,
graus inteiros, ângulo u16, magnitude e setor numérico. `Sample.direction_bits`
corresponde à palavra direcional produzida em `S+31c`.

Todo o domínio signed32 é aceito. A divisão por 256 trunca em direção a zero;
o byte baixo do quociente é reinterpretado como signed8, sem saturação.
Por exemplo, `INT32_MAX → -1`, `INT32_MIN → 0`, `65536 → 0` e
`-255 → 0`. Esses valores são entradas numéricas deliberadas do ensaio;
não demonstram que a aquisição real os produza.

O perfil usa arredondamento float32 para o mais próximo, empate para par,
correspondente ao `FPSCR=0` declarado no oracle. A API retorna
`Error::unsupported_rounding` se `fegetround()` não for `FE_TONEAREST`.
Não altera o modo de arredondamento. Flags de exceção de ponto flutuante não
fazem parte do contrato de saída. A inicialização real de FPSCR pelo jogo
não foi determinada neste recorte.

Não há alocação, referências retidas ou estado compartilhado. A função processa
exatamente dois pares; a raiz inteira usa no máximo oito passos por par.
Nenhum código de SDL, viewer, rede, launcher ou controlador de ação foi ligado
automaticamente a esta conversão.

## Operações preservadas

1. Redução signed32 → signed8 conforme o byte armazenado pela fonte.
2. Ângulo pelo caminho finito de `0xd12724`, que usa redução de quadrante e
   polinômio float32. Zero/zero produz zero. Resultado negativo recebe a
   constante float32 `0x40c90fdb`.
3. Multiplicações sequenciais por `0x3e22f983` e `0x43b40000`, seguidas de
   truncamento para graus inteiros. Os resultados observados foram 0..359.
4. Magnitude `min(127, floor(sqrt(x*x+y*y)))`. Abaixo de 30, magnitude e graus
   são zerados. O C++ usa raiz inteira; sua equivalência ao `VSQRT.F32` seguido
   de truncamento foi conferida em todos os pares signed8.
5. Círculo u16 `floor(graus*65536/360)`, após quantização e zona morta.
6. Setor zero abaixo de magnitude 37. Nos demais casos usa o índice truncado
   de `graus * float32(0x3d360b61)` na tabela de 16 bytes em `0xfb82c0`.
   A tabela e a máscara conjunta conservam valores numéricos sem nomes físicos.

O C++ não usa `std::atan2`. VFP2 `VMLA/VMLS` arredonda produto e soma
separadamente; as operações nativas usam stores float32 intermediários para
impedir contração em FMA. A compilação rejeita `__FAST_MATH__`. O helper
reconstruído é privado e limitado aos pares signed8: não é uma implementação
geral de atan2 para NaNs, infinitos ou subnormais.

## Prova original e recursos delimitados

ELF: `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Cada intervalo vem de um único PT_LOAD inicializado, com limites de arquivo,
permissão executável para código e digest conferidos antes da execução.

| Intervalo, fim exclusivo | Papel |
| --- | --- |
| `6947f8..6949b8` | Redução, quantização, magnitude, setores e máscara |
| `d12724..d12934` | Helper angular original |
| `d119d4..d11a00` | Classificação original de float |

Não houve stubs, substituição de chamadas ou matemática host no oracle.
`VSQRT.F32` executou na CPU emulada ARM11 MPCore do Unicorn 2.1.4.
`d119d4` foi executado no ramo de razão zero; `d14370` não foi alcançado
em nenhum caso e permanece fora do conjunto executável permitido.

O ensaio começa em um **bloco isolado**, não na entrada de `694644`.
`source-readback.json` conserva a derivação dos registros do prefixo:
`r6=S`, `r5=S+30c`, `r1=S+8c`, `r7=30`, `r10=0`, `s16=+0` e SP de scratch.
As cargas do próprio bloco inicializam suas demais constantes.
Gates habilitados são a fronteira escolhida, não retornos falsos fabricados
para os helpers anteriores.

Leituras são permitidas apenas nos quatro campos inicializados, constantes
originais declaradas e bytes escritos pelo caso atual. Writes só são aceitos
nos campos de saída e no intervalo de stack declarado. O maior uso observado
foi `SP-16..SP+6`, com os intervalos exatos retidos; o limite é `SP-64..SP+6`.
O código não pode executar padding de página nem sair dos três intervalos.
Há limite de 1.024 instruções e 100.000 microssegundos por caso; o máximo
observado foi 308 instruções. SP e registros de contexto preservados são
conferidos ao alcançar o fim do bloco.

| Validação | Resultado |
| --- | --- |
| Pares signed8 completos, dois canais transpostos | 65.536 blocos / 131.072 pares |
| Signed32: extremos, fronteiras ±1/±255 e valores determinísticos | 647 blocos |
| Controles negativos de identidade, domínio, leitura, escrita, código e limite | 10 rejeições esperadas |
| Comparações nativas normais | 197.929 checks, zero divergências |
| Mesmas comparações com UBSan | 197.929 checks, zero divergências |

O oracle executou 19.102.570 instruções, 3.250.446 leituras e 1.429.714 writes,
sem acessos positivos fora do perfil. Medição local de uma execução completa:
25,250 s de parede, 28,034 s de CPU e RSS de pico de 248.496.128 bytes.
Não há comparação cold/warm nem medição do runtime completo; esses custos
descrevem somente o processo Python/Unicorn de validação. A validação nativa
foi executada em macOS com Apple Clang; Windows não foi executado nesta entrega.

## Evidência e repetição

Tudo fica em `research/private/input-quantization-v1/`:

- `verify_arm.py`, `arm-validation.json`, `s8-oracle.bin` e
  `boundary-fixtures.json`: código de ensaio, contagem, limites e observações.
- `prepare_native.py`, `s32-oracle.bin`, `verify_native.py` e
  `native-validation.json`: leitura das observações pelo teste C++ e logs.
- `source-readback.json`: 6 intervalos REA / 2.152 bytes comparados com o ELF,
  referências e hashes dos registros completos.
- `manifest.json`: identidade dos produtores, fixtures, relatórios e fontes.

REA anterior `1792283675798`: 054/055, 092/093, 128/144. Nova sessão do dono
de aquisição `1792285120229`: 016/017, 027–032. Evidence centrais:
`ev_ea70e33ecd957af81633557828bade5d1c7d81ffe337c41c6a7b496d583c89fa`
(função de entrada em bytes),
`ev_9276b3d4494fb3ceede78a17bf255e01b3fd223e1963257a5a59c844bc9f32e1`
(helper angular em bytes) e
`ev_a37646009fe53dbbc4f51f09af036d6b9373dbfc9b0923916dcbd62c6e9b2c8d`
(classificador, pseudocódigo apoiado pelo assembly e readback).

O teste C++ básico não depende da ROM. Para comparar os dados privados completos:

```sh
input-quantization-tests \
  --s8-oracle research/private/input-quantization-v1/s8-oracle.bin \
  --s32-oracle research/private/input-quantization-v1/s32-oracle.bin
```

Os oracles contêm somente entradas/saídas numéricas observadas. Não se deve
confundir essa equivalência delimitada com validação de aquisição física,
calibração, estado completo de `sMhPad`, controles do jogo ou frame timing.
