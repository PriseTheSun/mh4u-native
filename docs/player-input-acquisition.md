# Aquisição e calibração dos dois pares de entrada

Este recorte liga os pares de `sPad` a seus leitores e filtros no executável
europeu de MH4U. O par 0 vem de `hid:USER`; o par 1, de pacotes de `ir:USER`.
O layout do primeiro coincide com o Circle Pad em libctru. O segundo coincide
com o protocolo público de Circle Pad Pro. Isso não estabelece o caminho de
C-stick de New Nintendo 3DS, a conversão de um dispositivo SDL, a frequência
de atualização, nem o scheduler completo do caçador.

O motor nativo e os pacotes não foram alterados. A execução citada abaixo é de
funções ARM isoladas, em Unicorn, com estado sintético explícito; os serviços,
threads e o jogo completo não foram executados.

## Identidade, evidência e reprodução

- ELF: `research/private/mh4u-analysis-bss.elf`, SHA-256
  `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
- Sessão REA `1792285120229`, Ghidra 12.1.4, ARMv6, 3 GiB, perfil focused
  com `Decompiler Switch Analysis` desabilitado. A sessão anterior
  `1792283675798` já estava encerrada antes desta importação.
- Saídas completas: `research/private/rea-session-1792285120229/`.
  Bundle e snapshot: `research/private/player-input-acquisition-v1/rea-evidence.json`
  e `rea-snapshot.json`; 163 Evidence. Exportação 165 e fechamento 166 tiveram
  sucesso; o cliente terminou com código 0.
- `evidence-index.json` no mesmo diretório relaciona cada número abaixo com
  Evidence ID, parâmetros e hash. `readback.json` confirma 48 leituras REA,
  11.100 bytes incluindo sobreposição, iguais aos segmentos PT_LOAD do ELF.
  Pseudocódigo é derivação do decompilador, conferida com assembly/bytes.
- `calibration_oracle.py`, `calibration-cases.tsv` e
  `calibration-validation.json` preservam a execução isolada. Reexecutar com:

```sh
research/private/root-motion-runtime/venv/bin/python research/private/player-input-acquisition-v1/calibration_oracle.py
python3 research/private/player-input-acquisition-v1/retain_evidence.py
```

O primeiro comando exige Unicorn 2.1.4. Os arquivos privados contêm evidência
local do usuário e não entram no pacote distribuível. `manifest.json` fixa
os hashes das saídas e deste documento. O código público usado para
corroboração está fixado em `public/sources.json`; não foi integrado ao motor.

## Origem dos pares

Use `S = *(u32*)0x10572d8` como o objeto básico de entrada, preservando a
distinção entre esse objeto e o receiver de suas chamadas. A cadeia de consumo
até `pbw` foi documentada em [player-input-production.md](player-input-production.md).

| Par | Amostra intermediária | Campos posteriores de S | Destino pbw |
|---|---|---|---|
| 0 | `s16+0`, `s16+2` | `+ac/+b0`, depois `+32c/+338` | `+3c0/+3c4` |
| 1 | `s16+4`, `s16+6` | `+a4/+a8`, depois `+32e/+33a` | `+3c2/+3c6` |

### Par 0: buffer HID

`0x10698c` passa o objeto `0x110e488` para a inicialização em `0x107c6c`.
O corpo abre `hid:USER`, obtém seis handles por `0x109e6c`, mapeia a memória
com `0x10be1c` e grava a base mapeada em `0x110e488+4`. O getter `0x485ad0`
devolve esse objeto. Ghidra incorporou o tail de `0x107c6c` à função
`FUN_0010698c`: não se deve confundir a fronteira do decompilador com uma
função contígua. Evidências 105, 108–111, 118–119.

`0x571df8` lê a região PAD de registros de 16 bytes: índice em `base+0x10`,
held em `base+0x28+16*i`, eixos signed16 em `base+0x34+16*i` e `+0x36+16*i`.
Ele confere os metadados temporais após a cópia e pode repetir a leitura
quando detecta atualização concorrente. Evidências 099–101.

Esse layout e a ordem dos eixos coincidem com `hidScanInput`/`hidCircleRead`
de [libctru, commit 9b55eda](https://github.com/devkitPro/libctru/blob/9b55eda44cf80b971503991e0c78f9bc8fe50425/libctru/source/services/hid.c#L185).
O [tipo público circlePosition](https://github.com/devkitPro/libctru/blob/9b55eda44cf80b971503991e0c78f9bc8fe50425/libctru/include/3ds/services/hid.h#L50)
identifica `dx/dy` como Circle Pad. A associação física é uma corroboração de
layout e API pública, além da origem HID comprovada no binário.

### Par 1: pacote IR e anel combinado

O inicializador `0x47c09c` abre `ir:USER` e prepara a fila `0x1119d3c`;
`0x47e504` consome essa fila. `0x47c2dc` é o produtor do anel combinado
`0x11177a0`; seu wrapper `0x47cc68` passa cinco parâmetros globais e marca
status 5 quando retorna. Evidências 078–083, 093–098, 148–150.

O produtor aceita um payload de seis bytes com `p[0]==0x10`, extrai:

```text
rawX = p[1] | ((p[2] & 15) << 8)
rawY = (p[2] >> 4) | (p[3] << 4)
```

Os bits 7/6/5 de `p[4]` são tratados como ativos em zero, além de outros
campos de status. `0x47db2c` calibra esses dois valores. No mesmo ciclo,
`0x571df8` fornece a amostra HID para o par 0; ausência de uma nova amostra
HID zera esse par no ramo observado. O produtor grava os dois pares, os
botões e metadados em registros de 24 bytes do anel de oito entradas.
Publica status 4 em `0x1064b53`. `0x47cbf8` lê esse byte e `0x485b28`
testa igualdade a 4; esse estado seleciona o leitor composto.

O pacote tem o mesmo identificador e packing de 12 bits da implementação
de Circle Pad Pro em [iruser.c, commit b646fc4](https://github.com/syelan34/libctru/blob/b646fc4f03c11ed57eb2e59f8678acf1602a721f/libctru/source/services/iruser.c#L454).
Essa fonte pertence à [PR libctru 568](https://github.com/devkitPro/libctru/pull/568),
aberta e não mesclada na consulta; serve como corroboração do protocolo,
não como prova de comportamento completo em hardware. O caminho New 3DS
`ir:RST` não foi estabelecido neste executável.

`0x47dff4` também transforma o par calibrado em bits direcionais: x positivo
pode produzir bit24, x negativo bit25, y positivo bit26, y negativo bit27,
sob testes de proporção controlados pelo global `0x1064b70`.
Evidências 159, 161–163. Os nomes públicos desses bits são direita, esquerda,
cima e baixo no [header HID fixado](https://github.com/devkitPro/libctru/blob/9b55eda44cf80b971503991e0c78f9bc8fe50425/libctru/include/3ds/services/hid.h#L27).
Isso corrobora x crescente à direita e y crescente para cima no protocolo;
o valor/produção do limiar global e a transformação para câmera/mundo
continuam separados desta aquisição.

### Fallback sem o anel IR pronto

Quando status não é 4, `0x4855d0` usa `0x485cf8` com o leitor em `S+0x280`.
Esse leitor usa diretamente o mesmo buffer HID, calibra os eixos no seu
próprio config e aplica gates de sistema antes de retornar. O caller copia
o resultado no par 0 e zera o par 1. O config desse fallback é **S+0x28c**,
não `S+0x230` nem `S+0x254`. Evidências 023, 069–071, 075–077 e leitura
retida de `0x4855d0` na investigação anterior.

## Calibração anterior ao filtro do jogo

`0x47db2c` mantém dois valores anteriores em `0x107ed1e/+20`. Uma diferença
signed16 de módulo menor que 6 reutiliza o anterior; caso contrário, aceita
o valor novo. Um gate em `0x107ed1c` também coleta extremos, sem estabelecer
neste recorte quem ativa esse modo. Depois:

1. Subtrai os centros signed16 em `0x1119cd8/+da`.
2. Multiplica por ganhos float32 em `0x1119cd0/+d4`; converte por truncamento
   signed32 e estreita para signed16.
3. Divide o módulo inteiro por 8; incrementa o quociente se o resto for
   **maior que 2**, e restaura o sinal. Não é arredondamento usual ao mais próximo.

Evidências 024, 072–074. A fórmula é uma derivação estática; essa função não
foi incluída na execução isolada dos filtros abaixo.

Os centros/ganhos não foram inferidos de BSS zero. `0x47d8c0` chama
`0x47d944`; sucesso copia os parâmetros e inicializa os valores anteriores
com os centros. `0x47d944` primeiro tenta quatro registros de 16 bytes,
do último ao primeiro, obtidos via `0x47c24c` no offset 0. Valida marcador
baixo 0 e um byte de verificação calculado por `0x56c7b4`; recupera centros
de 12 bits e dois ganhos de 32 bits. Se nenhum registro serve, `0x47da3c`
tenta registros de outra região a partir de 0x400, com marcador 0xfe.
Somente após falha desse fallback instala ganhos **1.0/1.0** e centros
**2048/2048**. Falha na primeira leitura retorna erro sem produzir esse
default. Evidências 112–117, 121, 124–126.

`0x47c24c` divide as requisições em blocos de até 0x40 bytes e chama
`0x47cfd8`; este envia requisição de tipo 2 e espera resposta 0x11 pelo
transporte IR. Isso vincula a calibração ao dispositivo remoto, sem executar
o transporte nem provar o conteúdo de um aparelho específico. Evidências
134, 143–145, 152, 156–158.

## Configuração dos três filtros

O constructor `0x5b86a0` chama `0x4857b4(S+0x228)`. Este chama
`0x485a5c` nos configs `S+0x230` e `S+0x254`, e inicializa o leitor fallback
em `S+0x280`, cujo constructor `0x485e5c` chama o mesmo inicializador para
`S+0x28c`. Evidências 057–059, 066–068, 075–077 e constructor retido.

Cada config C tem os seguintes writes explícitos; padding não é considerado
inicializado:

| Campo de C | Inicialização observada |
|---|---|
| s16+0 / +2 / +4 | 40 / 36 / 40 |
| s16+6 / +8 / +a | 145 / 145 / 145 |
| byte+c | modo 0 |
| s16+e | 141 |
| float+10 / +14 | 1.5 / 141.0 |
| float+18 / +1c / +20 | 0 / 0 / 0 |

`0x48597c` restringe os campos inner+0 a pelo menos 40 e inner+2 a pelo
menos 36; restringe os três campos outer a no máximo 145. Não é validação
completa de `inner < outer`, nem de todos os valores signed16 ou do modo.

### Atualização dirty antes da leitura

O caller `0x5b814c(R)` passa `R+0x2c` como estado H para
`0x5b6fa0(S,H,amostra)`, depois de testar bytes `R+0x30` e `R+0x33`.
`0x5b6fa0` só lê quando `H.byte+6==0`. Se `H.s16+0x1a != 0`:

1. Lê seletor `H.u16+0x18` e obtém um byte de `0xe362dc[seletor]`.
2. `0x485778(S+0x228,modo)` escreve modo em `S+0x260` e escolhe
   0/1/2 para o fallback `S+0x298`; modo fora desse conjunto vira 0 somente
   no fallback. O caller grava também `S+0x23c`.
3. `0x485e28` atualiza os limites do segundo config e do fallback com
   `H.s16+0x14/+0x16`; `0x4858b0` atualiza os do primeiro config.
4. Zera `H.s16+0x1a`, antes da chamada de aquisição `0x4855d0`.

Em modo 0, o setter impõe inner>=40; em modo 1, inner>=36. Ambos limitam
outer<=145. Para os outros modos, o setter escreve só outer+0xa.
O readback de quatro bytes em `0xe362dc` é `00 01 00 00`; isso confirma
valores dos índices 0 e 1, **não prova o tamanho da tabela**, nem que modo2
seja selecionado pelo fluxo normal. O índice não tem bounds check neste
consumidor. Evidências 132–142, 147, 151, 153–155, 160, 164.

Portanto os valores do constructor não são defaults imutáveis. A origem
das alterações em `R+0x40/+0x42/+0x44/+0x46` ainda precisa ser seguida.

## Contrato numérico dos modos e validação isolada

`0x4859c8(C,outX,outY,x,y)` despacha para:

| Modo | Função | Efeito com parâmetros iniciais |
|---|---|---|
| 0 | `0x571a64` | Zona morta circular de raio40; raio externo145; amplitude resultante105 |
| 1 | `0x571960` | Remove36 de cada módulo de eixo; limita o vetor resultante ao raio109 |
| 2 | `0x571b48` | Zona morta radial derivada de quadrado36, limitada ao raio40; amplitude externa105 |

Em modo0, seja `n=x²+y²`. Se `n<=a²`, ambos os eixos viram zero. Fora
disso, `r=isqrt(n<<14)` no domínio validado; `h=(b-a)<<7` quando `n>=b²`,
ou `h=r-(a<<7)` dentro do limite externo. Os resultados são
`trunc(x*h/r)` e `trunc(y*h/r)`, estreitados para signed16.

Em modo1, cada eixo vira `sign(v)*max(abs(v)-a,0)`. Se o módulo resultante
é menor que `b-a`, ele é preservado; fora disso a mesma raiz inteira e
divisão signed o limitam a esse raio. A divisão original `0x2fe0e0` foi
executada, não interceptada. Casos que poderiam extrapolar o domínio
inteiro seguro não foram generalizados a todos os signed16.

Em modo2, para `n>0`, o raio da zona morta é geometricamente
`d=min(40,36*sqrt(n)/max(abs(x),abs(y)))`. Dentro dele, resultado zero.
Fora dele, o ganho é `(b-40)/sqrt(n)` quando `n>=b²`, ou
`(b-40)*(sqrt(n)-d)/((b-d)*sqrt(n))` antes disso. O ARM usa instruções
float32 e truncamento; esta expressão real descreve a geometria e não
substitui a ordem das operações em uma implementação bit a bit. O parâmetro
inner do config não é lido por essa função: 36 e 40 são literais.

`0x4858f8(S+0x228,v)` usa **sempre o config S+0x254**, inclusive para
normalizar os quatro eixos no consumidor `0x5b8328`. O denominador D é
`sxth(outer-inner)` dos campos do modo0/1, ou literal105 no modo2. Retorna
0 para `v==0`, 1 quando `D<=v`, ou `float32(v)/float32(D)`. Não há clamp
negativo correspondente: `v=-109,D=105` produz valor inferior a -1.
O consumidor multiplica por32767.0 e trunca para inteiro, antes da
quantização signed8 do nível `sMhPad`. Não adicionar clamp simétrico por
conveniência. Modo desconhecido não tem contrato utilizável nessa função.

A execução isolada cobriu **1.875 casos**: 25 valores por eixo, cruzados
nos três modos, incluindo vizinhos das fronteiras36/40/145, sinais e zero.
Os modos0/1 coincidiram exatamente com referência independente baseada em
`math.isqrt` e divisão signed. Modo2 coincidiu com a expressão geométrica
em todos os resultados inteiros observados; a tolerância pré-declarada
era uma unidade inteira. Mais **27 casos de normalização** coincidiram
com a fórmula float32. Dois controles negativos detectaram config não
inicializado e escape para helper proibido. Foram executados o constructor,
dispatcher, três filtros e divisão ARM; nenhum helper matemático foi
substituído por resultado do host. O oráculo não valida periféricos reais.

## Ordem local, limites e próxima fronteira

A ordem local comprovada é:

```text
sMhPad+1c = 694644
  → 5b814c(receiver)
    → gates receiver+30/+33
    → 5b6fa0(S, receiver+2c, sample)
      → gate state+6; dirty config update
      → aquisição composta ou fallback
    → 5b8328 normaliza os eixos
  → quantização, ângulo e magnitude para campos S+32c..33a
```

O consumidor `b1c750` e seus gates subsequentes estão na investigação
anterior. **Não foi demonstrado** que o scheduler chama `sMhPad+1c` antes
de `sPlayer+1c=af8ae8` e de `uPlayer+24=b48230` em cada tick. Tampouco
foi medida frequência do produtor IR, do HID ou do consumidor. Timestamps,
esperas, callbacks de vtable e nomes de classes não bastam para fixar FPS.

As próximas perguntas concretas são os writers de
`receiver+0x40/+0x42/+0x44/+0x46` e dos gates `+0x30/+0x33`, seguidos do
registro/iterador que chama as três entradas virtuais de atualização. A
inicialização dos limiares globais de direção e a aquisição do C-stick de
New3DS são fronteiras distintas. Esse conhecimento é necessário antes de
ligar uma entrada física PC ao controlador original sem inventar política.
