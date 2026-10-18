# Filtros inteiros e normalização nativa de entrada

`src/input_filter.hpp/.cpp` implementa o perfil numérico dos modos fonte 0 e 1
de `0x4859c8`, seguido da conversão `0x4858f8` e dos stores numéricos em
`0x5b83f8..0x5b8470`. O resultado pode alimentar `input_quantization::convert`.
Os dois componentes foram compostos em testes contra o ARM original.

O módulo recebe amostras signed16 e configurações explícitas. Não recebe
eventos SDL, não lê serviços HID/IR e não implementa o scheduler ou setters
dirty. A origem desses campos está documentada em
[player-input-acquisition.md](player-input-acquisition.md). Este recorte não
atribui frequência, FPS, correspondência de dispositivo ou significado de
movimento do caçador aos valores numéricos.

## API e propriedade

Namespace `mh4u::input_filter`:

- `Config{source_mode, limits[2]}` mantém o byte de modo e os pares signed16
  `(inner,outer)` dos modos 0 e 1. Não converte modo desconhecido em default.
- `filter(config, Axes)` retorna `Axes` ou `Error`.
- `normalize(config, Axes)` retorna `Normalized{ratios,words}` ou `Error`.
  `ratios` são os float32 anteriores à multiplicação; `words` são signed32.
- `compose(configs[2], input[2])` retorna `Sample{filtered,normalized}` ou
  `Failure{reason,phase,pair}`. Cada par usa seu próprio filtro; **os dois
  pares usam `configs[1]` na normalização**, reproduzindo a leitura fixa C1
  feita por `0x4858f8`.

Todos os resultados são valores. Não há alocação, referências retidas,
modificação das entradas ou publicação de estado parcial. `Failure` informa
o primeiro estágio/par rejeitado. O módulo não interpreta arquivo binário
nem duplica o dono da quantização. O consumidor usa os `words` produzidos,
sem recalcular razões, clamps ou redução signed8.

`Config` inicializado com zeros é inválido; a integração deve fornecer
limites observados ou escolhas explicitamente rotuladas. Apenas o par de
limites selecionado pelo modo é validado/lido. Campos do modo inativo podem
permanecer desconhecidos para a operação atual.

## Perfil aceito e domínio aritmético

Os modos 0 e 1 exigem `0 <= inner < outer`, ambos signed16. Isso é uma
precondição do perfil nativo, não uma alegação de validação existente no
jogo. O constructor observado escreve 40/145 para modo 0 e 36/145 para modo
1; seus setters posteriores podem alterar esses valores. O setter impõe
`inner>=40` ou `inner>=36` e `outer<=145`, mas não garante `inner<outer`.
O módulo não executa nem substitui esse setter.

As configurações sintéticas com `inner=0`, `outer>145` ou outros limites
fora dos valores produzidos pelo setter são admitidas apenas quando
obedecem ao domínio matemático abaixo. Elas foram usadas para verificar a
rotina numérica isolada, e não estabelecem que o jogo as produza normalmente.

O cálculo verifica os intermediários em largura maior antes de aceitar
uma execução equivalente à aritmética signed32 original:

1. No modo 0, `n=x*x+y*y` precisa caber em signed32. Se `n<=inner²`, retorna
   zero sem calcular raiz.
2. No modo 1, primeiro reduz cada eixo por sua deadzone individual; depois
   calcula `n`. Se `n<(outer-inner)²`, retorna os eixos reduzidos sem raiz.
3. Somente no ramo que calcula raiz, exige `0<n<=131071`, pois o original
   usa `n<<14` como signed32 positivo. Valores fora desse domínio retornam
   `unsafe_arithmetic`, sem reproduzir overflow ou divisão por zero.

As saídas antecipadas conservam seu domínio maior. Por exemplo,
modo0/limites32766..32767 aceita `(32000,0)` e retorna zero;
modo1/limites0..32767 aceita `(32766,0)` sem raiz. A condição foi aplicada
ao produtor do intermediário, e não como clamp arbitrário dos eixos.

No ramo da raiz, o módulo calcula exatamente `floor(sqrt(n*16384))` por
busca inteira limitada. A execução ARM usa sua própria rotina iterativa e
o helper original de divisão signed32 `0x2fe0e0`, ambos executados no oracle.
Não há `sqrt` de host no produtor nativo.

Modo 0 usa `h=(outer-inner)*128` quando `n>=outer²`; caso contrário,
`h=root-inner*128`. Modo 1 usa `h=(outer-inner)*128` após a redução por eixo.
Cada saída é `trunc(eixo*h/root)`. No domínio aceito, `abs(eixo)<=362` no
ramo da raiz e `h<=root<=46340`; os produtos cabem em signed32 e as saídas
em signed16. Os casts nativos não dependem de overflow ou narrowing fora
da faixa. O modo 1 também preserva a comparação estrita `<` na saída sem
raiz e o modo 0 a comparação `<=` na deadzone.

## Normalização e ordem float32

`D=outer-inner` é positivo e cabe em signed16 neste perfil. Para cada
valor signed16 `v`, `0x4858f8` retorna:

```text
v == 0  -> +0.0f
v >= D  -> 1.0f
demais  -> float32(v) / float32(D)
word    -> trunc_signed32(float32(ratio * 32767.0f))
```

Não existe clamp simétrico negativo: com D=105 e v=-109, a razão fica
abaixo de -1 e o word é -34015. A sequência divide, arredonda float32,
multiplica, arredonda float32 e trunca para inteiro. Ela não é substituída
por uma única divisão inteira ou operação em double. As conversões de `v`
e `D` para float são exatas no domínio signed16. Stores `volatile` separam
as operações, e `__FAST_MATH__` gera erro de compilação.

O ambiente aceito é float32 IEC559, arredondamento para o mais próximo
com desempate par e exceções de ponto flutuante mascaradas como no runtime
usual. `fegetround()!=FE_TONEAREST` retorna `unsupported_rounding`.
Nenhum caminho aceito gera NaN, infinito ou subnormal: a menor razão
não zero em módulo é `1/32767`. FTZ/FZ/DAZ não altera esses operandos; não
foi adicionado um guard de subnormal sem necessidade. Flags de exceção
acumuladas não pertencem ao contrato de resultado. O módulo não muda o
arredondamento ou outros controles do processo.

O menor word possível é -1073709056 (`v=-32768,D=1`) e o maior é 32767;
a conversão signed32 não sai da faixa. Não se deve posteriormente presumir
que todos os words estejam limitados a [-32767,32767]. A quantização
existente preserva divisão por 256 com truncamento e wrap do byte baixo.

## Evidência e oracle independente

ELF fixado: `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.

`research/private/input-filter-v1/evidence-index.json` liga requests,
Evidence IDs e hashes às sessões REA retidas:

- `1792283675798`: assembly 089/097/110 e bytes 136/139/140 cobrem stores,
  normalizador e dispatcher.
- `1792285120229`: assembly 049/052, bytes 050/053 e 129 cobrem os dois
  filtros e o helper real de divisão.
- `1792286478819/067`: literal em `0x5b8694`, bytes `00feff46`,
  bits `0x46fffe00` = 32767.0f. Evidence
  `ev_5fb4bc99f7c24d68d8a9812dc05ac5d408c05bd0e6828baa4d8369f2d667314f`.

`source-readback.json` confirmou sete leituras, 2.228 bytes incluindo
sobreposição, iguais ao ELF. Os assembly LLVM retidos são derivações dos
mesmos bytes. Nenhuma sessão nova foi aberta por este componente.

`verify_arm.py` executou os bytes ARM originais no Unicorn 2.1.4,
CPU ARM11 MPCore/FPSCR0. O leitor PT_LOAD fixado é reutilizado do oracle
de quantização. Não há substituição de funções, retorno forçado, stub de
divisão ou matemática de host dentro da execução. O hook em `0x485970`
apenas observa os bits de s0; não altera o estado. A equação Python com
`math.isqrt` é uma comparação adicional, separada da produção das fixtures.

O estado é sintético e explícito: configurações, amostras, argumentos e
registradores que o prefixo omitido prepararia. O bloco numérico começa em
`0x5b83f8`, termina antes de `0x5b8470` e usa botão0 para o teste intermediário
que só afetaria r4. Não se executa aquisição, inicialização de serviços,
threads, hardware ou o jogo completo. A composição transfere saídas entre
essas fronteiras numéricas isoladas; não demonstra o scheduler entre elas.
O harness posiciona H em S+0x2c por escolha de layout sintético; isso não
afirma identidade universal entre o receiver e o objeto básico global S.

Guardas verificam identidade do ELF, faixa executável, leituras previamente
inicializadas, faixa de writes, SP e contexto preservado. O stack gravável
do filtro é `[SP-80,SP)`; só as duas saídas signed16 são graváveis fora dele.
O normalizador só pode gravar os quatro words de destino. Cada chamada tem
limite de 4.096 instruções e 100 ms. O maior caso positivo usou 621
instruções. Dez controles negativos verificaram falta de config/literal,
helper proibido, write proibido, execução em padding mapeado, limite de
instruções e identidade errada.

## Cobertura executada

| Verificação | Cobertura |
|---|---:|
| Filtros ARM, modos0/1 | 180.478 casos, 1.719 configs distintas |
| Grade exaustiva dos limites do constructor | 84.681 pares `[-145,145]²` por modo |
| Normalização | 33.052 blocos, 132.208 eixos |
| Subconjunto exaustivo da normalização | Todos os 65.536 signed16 para D105 e D109 |
| Composição filtro→normalização→quantização | 2.048 casos com dois pares |
| Comparação nativa normal e ASan+UBSan | 318.902 checks por variante, zero divergências |

As demais configurações/casos incluem fronteiras de deadzone, raio externo,
`n<<14`, signed16, denominadores pequenos/grandes e amostras determinísticas.
Não foi executado o produto cartesiano completo de todos os valores signed16
e configurações possíveis. O domínio além da grade combina essa cobertura
com os limites aritméticos expostos acima.

O ARM realizou 219.674 chamadas de filtro/normalização, 54.660.648 instruções,
4.287.060 leituras e 3.457.184 writes; a quantização composta acrescentou
576.106 instruções. Tempo observado: 72,17 s de parede, 79,17 s de CPU,
pico de RSS do processo 136.331.264 bytes em macOS. O relatório nativo
registra comandos, tempos, hashes e pico cumulativo dos filhos separadamente.
Não há baseline de desempenho nem medição de tokens. Essas medições são
da verificação, não do jogo.

## Reprodução e limites

```sh
research/private/root-motion-runtime/venv/bin/python research/private/input-filter-v1/verify_arm.py
python3 research/private/input-filter-v1/verify_native.py
research/private/root-motion-runtime/venv/bin/python research/private/input-filter-v1/retain_evidence.py
```

Os dois primeiros scripts recusam sobrescrever resultados já congelados;
uma nova execução deve usar uma cópia versionada do diretório, preservando
o contexto de caminhos do projeto. O teste nativo aceita:

```text
--filter-oracle PATH --normalizer-oracle PATH --composition-oracle PATH
```

Sem argumentos, executa semântica e rejeições. O alvo de testes deve ligar
`input_filter.cpp` e `input_quantization.cpp`; a biblioteca de filtro não
depende da quantização. As fixtures são `filter-oracle.bin`,
`normalizer-oracle.bin` e `composition-oracle.bin`, no diretório privado.
`manifest.json` fixa os arquivos públicos, evidências e comandos executados.

O modo 2 é rejeitado por `unsupported_mode`, inclusive em `normalize`,
para não apresentar um caminho completo sem filtro comprovado bit a bit.
A investigação anterior só comparou sua equação contínua com tolerância de
uma unidade inteira. Modos desconhecidos também são rejeitados. A calibração
IR anterior em `0x47db2c`, os setters dirty, seleção de config por dispositivo,
fallback e atualização do caçador continuam fora deste componente. Não há
conversão automática de teclado/joystick nem alteração de recurso/pacote/UI.

As execuções nativas aqui citadas foram em macOS arm64. Compilação ou
execução Windows é uma verificação de integração separada.
