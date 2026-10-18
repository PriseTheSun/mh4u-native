# Codecs de quaternion: comparação com o ARM de MH4U

Os codecs 6, 7, 11, 12 e 13 diferem das equações de referência usadas no
exportador v1. O codec 6 usa complemento de dois; os demais subtraem 8 dos
componentes quantizados e usam divisores diferentes. Além disso, o consumidor
original mistura **quaternions brutos**, corrige o hemisfério e só então
normaliza. Normalizar os endpoints antes da mistura não preserva exatamente
essa curva.

A investigação inicial foi somente estática. Após revisão, o perfil
`mh4u-lmt67-arm-sampler-v2` integrou as equações e a representação CUBICSPLINE
abaixo; a revisão anterior foi preservada. As conclusões sobre o jogo continuam
sendo derivações estáticas do executável desta ROM, não execução de ARM ou
medição de uma sessão do jogo. O impacto máximo observado no corpus foi
**7,754245° em um key do codec 7**. Há uma representação por CUBICSPLINE que
preserva algebricamente a curva derivada; a validação de serialização e do
consumidor nativo está registrada ao fim deste documento.

## Fonte, limites e arquivos preservados

Foi reutilizado o ELF de SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435` e o bundle
`research/private/rea-root-motion-evidence.json`, SHA-256
`34d03e7fe5b4daafb4e41fe50ea45d227830dbe9c63d416d959fbe98ef34b175`.
O dispatcher, seu estado inicial e o caminho do modelo já tinham Evidence REA;
ver [root-motion-findings.md](root-motion-findings.md). O dispatcher aponta à
tabela em 0xe315a4, com registros de 12 bytes: stride, getter do delta e sampler.

| Evidência reutilizada | Uso nesta investigação |
| --- | --- |
| `ev_02441f313eda39bd06c1494f9ebfa8bec85876427ca64be0dde2515738c9d660` | Dispatcher 0xaea0e4: seleção por codec, cursor e fração temporal |
| `ev_7137a10978e33a9884d4000eef8ce0dd1b2087a7e283794e0806f9f0c21a3265` | Literal com endereço da tabela |
| `ev_48961f4696c206e583bcd8a6a85e574dd40fa782f2ddf9a1737f100b6602c7d5` | Bytes da tabela que incluem o registro do codec 6 |
| `ev_dd2d5709fb192acce83e6f7116cdeb0e0105d7ed2b39752aa6b21c562e2e8159` | Inicialização comum: cursor zero, primeiro key e peso |

Os destinos novos foram lidos diretamente dos segmentos inicializados do mesmo
ELF e desassemblados com LLVM `--triple=armv6-none-eabi --mattr=+vfp2`.
O prefixo sobreposto da tabela foi comparado com os bytes REA. Cada intervalo,
offset de arquivo, hash e saída completa permanece em
`research/private/quaternion-codecs/source-manifest.json` e nos arquivos
`.bin`/`.asm` desse diretório. Não são novos registros REA e não se atribui
pseudocódigo Ghidra a esses corpos. Não foi necessária outra importação:
a sessão anterior já estava fechada, e nenhuma sessão ficou aberta nesta etapa.

Os literais após os retornos são dados usados por instruções relativas a PC;
não devem ser interpretados como instruções alcançadas. A auditoria usa
equações em double com constantes float32 exatas. Preserva a sequência de
operações F32 como evidência, mas não reproduz arredondamento por instrução,
FPSCR, denormais ou todos os casos não finitos do processador.

## Dispatch e cobertura

| Codec | Stride | Getter do delta | Sampler / helper de componentes | Tracks | Keys |
| --- | ---: | --- | --- | ---: | ---: |
| 6 | 8 | 0xde4340 | 0xae8ff0 | 623 | 26.599 |
| 7 | 4 | 0xde43c4 | 0xae89d8 / 0xae96b8 | 1.465 | 33.617 |
| 11 | 4 | 0xde43d8 | 0xae9abc | 264 | 7.048 |
| 12 | 4 | 0xde43d8 | 0xae9cd0 | 243 | 6.504 |
| 13 | 4 | 0xde43d8 | 0xae9ed8 | 3 | 45 |

Foram auditados os cinco LMT anteriores, entradas 9521, 9522, 9523, 54913 e
92608 do índice. São 2.598 tracks e 73.813 keys desses codecs. O corpus também
tem 119 tracks quaternion de codec 2 sem buffer; eles usam a referência direta
do dispatcher e não participam desta comparação numérica.

O delta do codec 6 vem dos bits 56–63 do u64 little endian. Seu getter usa
o helper de conversão numérica em 0xdebc60. Nos demais, o getter extrai
bits 28–31 do u32 e converte para float. Os tempos continuam o somatório dos
deltas, começando em zero. Delta terminal zero mantém o key atual conforme
o dispatcher já demonstrado; não há referência artificial em frame zero.

## Extração e reconstrução

Defina `S = extremes[0..3]`, `B = extremes[4..7]`, todos floats da fonte.
`F32bits(h)` significa a constante float32 de representação hexadecimal h.
Quaternions têm ordem x,y,z,w.

### Codec 6

Os quatro campos de 14 bits estão nos shifts 42, 28, 14 e 0. O ARM alinha cada
campo com shift de 2 bits, aplica `SXTH`, converte para float e multiplica por
`F32bits(0x38800000) = 1/16384`. A equação equivalente é:

```
signed14(u) = u, se u < 8192; caso contrário, u - 16384
raw[i] = signed14(field[i]) / 4096
```

Não há scale/bias nem reconstrução de w por raiz: os quatro componentes estão
empacotados. A função normaliza esses componentes no retorno. A regra antiga
usava `u-16383` nos negativos e fator `4/16383`; mesmo depois da normalização,
a diferença de sinal desloca a direção. Todos os 16.384 valores possíveis de
um campo foram comparados entre a fórmula e a expressão `SXTH(u<<2)`.

### Codec 7

Os campos x,y,z,w têm 7 bits, nos shifts 21, 14, 7 e 0. O helper 0xae96b8
subtrai 8, converte com `VCVT.F32.U32`, multiplica por
`F32bits(0x3c124925)` e aplica scale/bias por componente:

```
quant[i] = float32(uint32(field[i] - 8)) * F32bits(0x3c124925)
raw[i] = B[i] + S[i] * quant[i]
```

O multiplicador é aproximadamente 1/112; a regra antiga era `field/127`.
A conversão é **unsigned**, diferente dos codecs 4/5 e 11/12/13. Para campos
menores que 8 há wrap u32 antes da conversão, não clamp nem número negativo.
Nenhum campo abaixo de 8 apareceu nos 33.617 keys amostrados; o comportamento
de wrap é derivado das instruções, sem observação de um recurso que o use.

### Codecs 11, 12 e 13

Dois campos de 14 bits estão em bits 0–13 e 14–27. O primeiro altera,
respectivamente, x, y ou z; o segundo altera w. Os dois outros componentes
permanecem iguais ao bias, que não precisa ser zero.

```
qAxis = (int(fieldLow) - 8) * F32bits(0x38802008)
qW    = (int(fieldHigh) - 8) * F32bits(0x38802008)
raw = B
raw[codec - 11] += S[codec - 11] * qAxis
raw[3]         += S[3] * qW
```

O multiplicador é aproximadamente 1/16368; a regra antiga dividia por 16383
sem subtrair 8. A conversão usa `VCVT.F32.S32`. Também não há reconstrução de
w a partir dos outros eixos. Nos ramos de fração zero, a normalização ocorre
depois da aplicação de scale/bias.

## Interpolação nativa

Todos os cinco samplers têm a mesma política matemática entre keys. Sejam A
e B os quaternions **brutos**, antes da normalização, e t a fração temporal:

```
B' = -B se dot(A,B) < 0; caso contrário B
q(t) = normalize((1-t)*A + t*B')
```

Em t=0 o caminho especializado decodifica e normaliza A. No caminho geral,
o teste de hemisfério usa o dot bruto; o código combina os componentes por
VMUL/VMLA e calcula sqrt da soma dos quadrados, divisão e multiplicações
para normalizar **apenas o resultado**. Não chama acos/sin nem normaliza
os endpoints antes de combiná-los. Dot igual a zero segue o ramo sem negação.

Os comprimentos brutos medidos não são constantes. No codec 7 variam de
0,9927567471 a 1,0059248287. Assim, nlerp dos keys previamente normalizados
também é uma aproximação; a diferença máxima medida desse atalho foi
0,05711957°. Há 18 segmentos com dot negativo no codec 6 e nenhum nos outros
quatro codecs da amostra. Não houve dot exatamente zero no corpus; esse
limite foi testado com casos analíticos separados.

## Comparação numérica

O script privado `audit.py` usa o parser existente para layout e bounds e
calcula as equações derivadas separadamente. O relatório `comparison.json`
preserva hashes do produtor e dos LMT, piores casos com offsets/campos brutos,
estatísticas e escopo. Foram amostrados 1/4, 1/2 e 3/4 de 71.215 segmentos
dentro do domínio [0,frameCount−1], total de 213.645 pontos. Estes são máximos
dos pontos examinados, não limites globais contínuos.

| Codec | Diferença nos keys | Só interpolação v1 vs nativa¹ | Diferença combinada v1² |
| --- | ---: | ---: | ---: |
| 6 | 0,048468° | 0,768952° | 0,790527° |
| 7 | 7,754245° | 0,596467° | 7,735090° |
| 11 | 0,053917° | 0,641422° | 0,659119° |
| 12 | 0,052295° | 0,605282° | 0,624526° |
| 13 | 0,001401° | 0,000160° | 0,001348° |

¹ Usa endpoints corrigidos em ambos os lados. O helper `slerp` da revisão v1 usa
aproximação linear normalizada quando dot > 0,9995. ² Usa a quantização antiga
e esse helper contra quantização e curva nativas. A distância é angular de
rotações, ignora a equivalência q/−q e não é deslocamento de vértices.

O pior key é entrada 92608, slot 128, track 10, bone ID 12, key 10,
offset 429652 do LMT. Os campos codec 7 são [13,119,8,8]. O pior caso isolado
de interpolação é entrada 92608, slot 104, track 14, bone ID 8, entre frames
24 e 25, fração 0,75. Não se atribuiu nome de ação a esses slots.

## Contrato implementado em v2 para preservar a curva em glTF

O significado de `LINEAR` não deve ser substituído globalmente. A especificação
glTF recomenda slerp para rotação LINEAR e define CUBICSPLINE com tangentes por
segundo, três elementos de saída por key, ao menos dois keys e normalização
do quaternion interpolado. [Khronos, interpolação](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#interpolation-cubic).

A seguinte construção é uma **derivação matemática desta investigação**.
Após escolher B' pelo hemisfério, defina a=|A|, b=|B'|, ka=1/a, kb=1/b e
d=kb−ka. A curva polinomial

```
P(t) = ((1-t)*ka + t*kb) * ((1-t)*A + t*B')
```

é quadrática, tem endpoints unitários e um multiplicador escalar positivo.
Portanto `normalize(P(t)) = q(t)` em todo o segmento. Hermite cúbico representa
esse polinômio exatamente em aritmética real. Para duração Δ em segundos:

```
valueA = ka*A
valueB = kb*B'
outA = (d*A  + ka*(B'-A)) / Δ
inB  = (d*B' + kb*(B'-A)) / Δ
```

As tangentes de entrada e saída de um mesmo key são calculadas separadamente
pelos segmentos adjacentes. Não devem ser normalizadas nem receber flip
automático do consumidor. A primeira in-tangent e última out-tangent podem ser
zero. A conversão não precisa de uma extensão proprietária para esses samplers;
um formato nativo com raw keys também é possível, mas exigiria contrato
explícito e suporte obrigatório, não um fallback silencioso para LINEAR.

### Hemisfério, corte e constantes

Para uma sequência de endpoints brutos originais, transporte sinais:
`s[0]=1`, `s[i+1]=s[i]*(dot(raw[i],raw[i+1])<0 ? -1 : 1)`.
Use `s[i]*raw[i]` para produzir os valores e tangentes. Isso mantém a escolha
original inclusive quando o dot é zero. Fazer a comparação depois de já
alterar um endpoint pode escolher outro caminho nesse limite.

Se `frameCount−1` corta um segmento em fração tc, primeiro calcule o endpoint
**bruto** `Bc=(1-tc)*A+tc*B'`. Use A, Bc e a duração truncada nas fórmulas.
Normalizar Bc antes dessa etapa altera a curva. Um key terminal mantido gera
segmento constante. Codecs 1/2 podem usar uma constante sem spline. Quando
`frameCount=1`, use STEP/LINEAR constante com um key; CUBICSPLINE não aceita
um único key segundo a especificação.

A equivalência cúbica teve erro angular máximo de 4,09e−14° em double nos
213.645 pontos do corpus. O script também verificou 80 combinações analíticas
de corte/fração, endpoints com comprimentos diferentes, dot negativo/zero e
transporte de sinais. Essa precisão é da álgebra em double, **não** um limite
do glTF serializado em float32 ou do consumidor C++. A correção v2 validou separadamente a serialização, as tangentes por segundo,
os cortes e a normalização no consumidor, conforme os resultados abaixo.


## Integração v2 e verificação no consumidor nativo

O produtor agora possui uma única fronteira para o tempo e os sinais:
`raw_channel_keys` preserva as magnitudes brutas, transporta sinais e recorta
em N−1; `channel_sampler` gera os valores e tangentes. `channel_keys` expõe
somente os valores normalizados para inspeção. O perfil é registrado em cada
clip e na proveniência, com hash do produtor e do manifesto estático de bytes.
A conversão do codec7 abaixo8 aplica wrap uint32 e arredonda VCVT para F32 antes
do multiplicador; os demais cálculos permanecem em double, explicitamente.

A revisão v1 foi copiada com hashes para
`research/private/animation-corrections/20261017-arm-sampler-v2/before/`.
Os quatro outputs originais foram preservados também em `replaced/`; os
candidatos v2 passaram em staging antes da promoção. Uma quinta exportação usa
o candidato estático de sete peças. As aceitações continuam 28/8 em em062 e
44/78 no player; eventos, root motion e nomes semânticos não foram liberados.

A comparação independente em `tools/resources/verify_rotation_curves.py` lê
LMT diretamente, sem importar o produtor e sem usar a fórmula de spline como
oráculo. Ela consulta o loader/sampler C++ real e compara a rotação resultante
com o nlerp bruto de cada par original. Foram 225.131 consultas nos cinco
artifacts, máximo angular 0,000103087°. Os limites são amostragem discreta,
float32 serializado/C++ e referência matemática double; não há execução do
ARM, reprodução de FPSCR nem comprovação de comportamento de gameplay.

Nos recursos aceitos não há corte de uma faixa quaternion antes do próximo key.
Um fixture LMT sintético separado testa esse caso, magnitudes diferentes e
transporte de sinal com dot exatamente zero. Os seus 14 pontos passaram no
mesmo leitor nativo, erro máximo 0,000007565°. O teste analítico cobre também
underflow uint32 do codec7 e campos extremos. Os 63 testes de recursos passaram;
os cinco glTF passaram no Khronos 2.0.0-dev.3.10 sem erros ou avisos.

Relatórios em `research/private/animation-corrections/20261017-arm-sampler-v2/`:
`native-curves-staging.json`, `native-curves-seven-piece.json`,
`native-fixture/verification.json`, `revision-staging.json`,
`khronos-stage-*.json`, `khronos-seven-piece.json`, `promoted-manifest.json`.
A comparação histórica das equações v1 permanece em
`research/private/quaternion-codecs/comparison.json`, com seu hash do produtor;
não deve ser confundida com uma medição do perfil v2.
