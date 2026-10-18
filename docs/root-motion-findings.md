# Deslocamento de cena nos LMT67

Os tracks de tipo 3 e 4 são ligados a samplers da **camada de animação** do
modelo, independentemente de existir uma junta correspondente. No corpus
selecionado eles usam bone ID 255. O consumidor original transforma as amostras
em movimento do modelo com estado anterior, escala, orientação, mistura de
camadas e atributos de runtime. Portanto, aplicar o track diretamente à junta
raiz do glTF ou somá-lo à posição em cada frame não reproduz esse caminho.

Esta investigação é estática. Não executou o jogo nem alterou produtores,
parsers ou runtime. As 86 ações antes bloqueadas continuam sem política de
deslocamento original implementada. O avanço aqui é a cadeia demonstrada e os
limites necessários para implementar essa política sem confundi-la com o codec.

## Cobertura e proveniência

O alvo é o ELF ARMv6 little endian de SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`, derivado
localmente da ROM identificada em [rea-findings.md](rea-findings.md). Foi usado
REA com Ghidra 12.1.4, heap de 3 GiB, importação efêmera e
`Decompiler Switch Analysis` desativado. Os bytes do jogo continuam privados.

As respostas completas estão em `research/private/rea-session-1792274318294/`.
O manifesto privado `research/private/root-motion/evidence-selection.json`
registra arquivos, hashes, operações, IDs e limitações. O bundle e snapshot
finais são relacionados nesse manifesto. O bundle
`research/private/rea-root-motion-evidence.json` contém 76 registros e tem SHA-256
`34d03e7fe5b4daafb4e41fe50ea45d227830dbe9c63d416d959fbe98ef34b175`.
A sessão foi fechada com snapshot em
`research/private/rea-root-motion.snapshot.json`; o processo terminou com código 0.
O snapshot reteve Evidence, mas informou zero bindings de replay, portanto não
se afirma que ele evita uma futura importação Ghidra. O loader LMT67 já observado na
sessão anterior foi reutilizado, sem uma nova importação para esse fato.

| Ponto | Evidência principal | O que demonstra |
| --- | --- | --- |
| Loader 0x23e3d8 | `ev_2341ca4a57100d3cb50d1e37279681b559e3e148d294bc09ad4206286775a533` | Magic LMT, versão 67, relocação de ações e tracks de 36 bytes |
| Ação 0xcd7540 | `ev_c327248a45ed9e767017836ceab95f1db5752228d11838a41ba111f64c5f7351` | Resource+0x64 aponta ao LMT; count u16+6 e tabela de ações +8 |
| Ligação 0x95aa90 | `ev_0efaabf914e4e2147baee4244d36d37940dfa9f09102fe65c14cb6348d918fcd` | Destino de cada tipo de track e inicialização por ação |
| Assembly 0x95aa90 | `ev_536563d3f719d8414b35f84687804ac09d3f3d0ced61bd6608dd9e918e4a0bda` | Confirma offsets, parâmetros de samplers e limite de frames |
| Sampler 0xae923c | `ev_dd2d5709fb192acce83e6f7116cdeb0e0105d7ed2b39752aa6b21c562e2e8159` | Guarda ponteiro do track, cursor, buffer e peso |
| Aplicação 0x95cab0 | `ev_4e8349fc0113a78ab43caecb13e541f11f14de84473797e23484307c2e074bf9` | Movimento agregado e escrita em posição/quaternion do modelo |
| Assembly 0x95cab0 | `ev_489b357b29cb3198fb345b7875af1996eb4494a1b6e95353acaaddd5c420bef1` | Fluxo e operações VFP do consumidor |
| Tempo 0x9573b8 | `ev_4c3dbbd0c7ef787f9b9a28cb4e41842c1e5c74561232066caf921b1cfdcb5dca` | Avanço, parada, repetição e compensação do estado anterior |
| Assembly 0x9573b8 | `ev_e2259ed88779fc4762b5b6172fb6e28dc12984979d10585b7ab717c839131cc8` | Confirma branches e offsets do avanço de tempo |

Antes do pseudocódigo foi usada a resolução da função contendo o endereço; as
consultas seguintes usam `FUN_nome`. Ghidra reuniu blocos não contíguos em
0x9573b8 e 0x95b790. As coberturas exatas permanecem nas respostas. A análise
de VFP frequentemente omite argumentos ou confunde valores retornados em s0–s3
com os parâmetros de entrada; por isso, fórmulas do sampler não foram aceitas
somente a partir de nomes de variáveis do pseudocódigo. A saída LLVM local
serve de apoio, não de prova de que todos os bytes de um intervalo são código.
O verificador privado `retain_provenance.py` comparou os 11 intervalos de
`read_bytes`, total de 1.304 bytes, com seus segmentos inicializados no ELF:
todos coincidiram. Isso valida identidade da fonte, não execução das equações.

## O que existe nos recursos selecionados

O inventário privado `root-motion/scene-track-inventory.json` usa o parser
congelado de [animation-findings.md](animation-findings.md). São cinco membros
LMT, 158 ações preenchidas e 161 tracks com bone ID 255:

| Entrada do índice | Ações | Tracks tipo 4 | Ações com translação não nula |
| --- | ---: | ---: | ---: |
| 9521, em062 | 17 | 17 | 1 |
| 9522, em062 | 9 | 9 | 5 |
| 9523, em062 | 10 | 10 | 2 |
| 54913, player | 19 | 19 | 9 |
| 92608, player | 103 | 103 | 69 |

Os três tracks restantes são tipo 3, codec 2, nos slots 75, 76 e 87 da entrada
92608. São referências aproximadamente identidade, com quaternion
`[0, 2.4492937e-16, 0, 1]`; não demonstram uma ação giratória. Nenhuma semântica
de gameplay foi atribuída aos números dos slots. Os flags de ação observados
são `0x800000`, campo distinto de `uModel::Motion.mAttr`.

O script privado `inventory.py` aceita `--lmt` repetido e `--out`. Preserva
hashes, referências, extremos, primeiro/último key armazenado, metadados de
loop e valores de término do header. A regra de referência e a conversão a
60 FPS são as do exportador anterior à correção descrita abaixo, não uma
medição de tempo do jogo. Esse inventário é evidência histórica de comparação;
não deve ser regenerado e apresentado como se tivesse a mesma interpretação.

O término do header não coincide necessariamente com o último key decodificado.
Por exemplo, entrada 54913/slot 3 tem 29 frames, loop -1, Z final do header
159.9036713 e Z final decodificado pelo exportador anterior 159.88659145. Os maiores desvios aparecem em
ações com loops positivos. Substituir um pelo outro perderia informação.

## Ligação e espaços

Defina `M = modelo + 0x240 + camada*0x130`. O resource LMT é escolhido em
0x9577ec e a ação é obtida por 0xcd7540 antes de 0x95aa90. Os tipos 0, 1 e 2
usam o mapa de bone ID em modelo+0x228 e os samplers de junta. Os tipos 3 e 4
vão, respectivamente, a `M+0xe8` e `M+0xd8`, mesmo quando o bone ID não produz
uma junta válida. O seletor efetivo é o **tipo**, não uma regra especial
inferida apenas do número 255.

Os nomes dos campos vêm da reflexão embarcada, não de convenções do glTF:

| Estrutura | Offset | Nome ou uso observado |
| --- | --- | --- |
| Modelo | +0x14 | `mDeltaTime` |
| Modelo | +0x40 / +0x50 / +0x60 | `mPos` / `mQuat` / `mScale` |
| Modelo | +0x238 | `mMotionCalcFlag` |
| Modelo | +0xbd0 / +0xbe0 / +0xbf0 | `mNullTransScale` / `mDeltaPos` / `mDeltaQuat` |
| M | +0x0c | `mAttr`, atributos de runtime |
| M | +0x48 | `mBlend` |
| M | +0x4c / +0x50 | Acumulador de frame / frame anterior copiado pelo avanço |
| M | +0x54 / +0x58 / +0x5c | `mFrameMax` / `mLoopFrame` / `mSpeed` |
| M | +0x78 / +0x88 / +0x98 | `mPrevQuat` / `mBaseQuat` / quaternion amostrado |
| M | +0xa8 / +0xb8 / +0xc8 | `mPrevTrans` / `mBaseTrans` / translação amostrada |
| M | +0xd8 / +0xe8 | Estado do sampler de translação / rotação de cena |

Reflexão: 0x959360, evidência
`ev_424541c2b01fe66bd4b11cab87ba198455f328767da7076b460f31dc48eebfcf`;
0x95fe30, `ev_d1a00c58731c6b6e6968b89dc689f6d10cbccc9680b6a9608aab2032c236e110`;
0x6d33a8, `ev_475ea503f38ac2ef74e78667782814bfa2b2230357bfbe3e0a3411fef52362ee`;
0x626f60, `ev_b5610070cf0714cd4776f1ba9fb9a6c3337304301f548b04f1eccf3be5f5dba3`.
O getter exposto como `mFrame`, em 0xd024c0, lê **M+0x50**. Seus bytes estão em
`ev_1e8ea16a1f0804e3ca5206a776e9e7f1de34eec1fae249ccb6d426843100b1ec`.
Não confundir o campo exposto com o acumulador M+0x4c.

O destino é a coordenada do modelo. A mesma reflexão expõe `mpParent` e
`mParentFlags`; o caminho estudado não demonstra que `mPos` seja sempre posição
mundial, nem resolve a composição com pais. O termo público
`absolute_translation` do RevilLib descreve um tipo de track; não prova
coordenadas mundiais absolutas de gameplay em MH4U.

## Aplicação, mistura e reset

0x95cab0 amostra translação e quaternion da camada e mantém valores anteriores.
No caminho em que `M.mAttr & 0x2020 == 0`, forma uma diferença de translação e
uma diferença de quaternion, considerando a orientação anterior e a do modelo.
O peso do track e `mBlend` participam da combinação entre camadas. O bit
`0x40000` seleciona um caminho diferente de combinação; o código não foi
reduzido a uma API genérica de mistura sem validar todos os ramos.

O ramo final usa os atributos da **primeira camada**, em modelo+0x24c:

| Condição | Efeito observado no modelo |
| --- | --- |
| `(attr & 0x2020) == 0` | Transforma o agregado com orientação, escala e `mNullTransScale`, soma a `mPos` e guarda o deslocamento em `mDeltaPos` |
| `(attr & 0x2020) != 0`, sem bit 0x2000 | Substitui `mPos` pelo agregado |
| Bit 0x2000 | Substitui `mPos` pelo agregado multiplicado por `mScale` |
| Bit 0x80 | Substitui `mQuat` pelo quaternion agregado |
| Sem bit 0x80 | Compõe o quaternion agregado com `mQuat` e normaliza |

Outros testes observados incluem `M.mAttr & 1`, que exclui a contribuição de
uma camada, e `&0x8100`, que espelha componentes nos caminhos examinados.
Esses valores não receberam nomes de estados do caçador. Os caminhos que
limpam deltas copiam templates globais; os valores desses templates depois dos
construtores não foram capturados em execução. Não se inferiu seu conteúdo a
partir da BSS sintética do ELF.

0x95aa90 inicializa os estados anteriores ao selecionar uma ação. Para
translação, o caminho normal sem `mAttr & 8` calcula `mBaseTrans` a partir da
diferença entre amostras em `mFrameMax` e `mLoopFrame`, como confirmado em
assembly 0x95ad40–0x95ad98. Existe ainda um ramo que copia o término do header
quando o loop de runtime é negativo; a inicialização normal desta mesma função
converte loop negativo do arquivo em zero. A existência desse ramo não prova
que ele seja alcançado para os arquivos de loop -1 da amostra.

## Avanço de tempo e repetição

A inicialização define `mFrameMax = float(action.frameCount - 1)` e
`mLoopFrame = max(action.loopFrame, 0)`. O avanço 0x9573b8 calcula
`passo = mSpeed * modelo.mDeltaTime`. Sem `mAttr & 0x10`, copia o acumulador
para M+0x50 e avança M+0x4c. Também atualiza estados e sequências de eventos.
Há tratamento distinto para passo positivo e negativo.

No caminho de avanço positivo:

- `mAttr & 4` seleciona parada no limite, em vez do caminho de repetição.
- No caminho de repetição, o intervalo é `mFrameMax - mLoopFrame`. Ao cruzar
  o limite, subtrai esse intervalo até retornar à faixa.
- Cada volta também subtrai `mBaseTrans` de `mPrevTrans` e compõe o estado
  anterior de rotação com a inversa da base. Isso preserva a continuidade do
  deslocamento apesar do retorno do frame.
- O caminho de passo negativo faz os ajustes correspondentes na direção
  inversa. Casos de intervalo não positivo e arredondamento possuem ramos
  próprios; não foram substituídos por uma operação de módulo genérica.

0x9573b8 chama 0x95b790 antes do avanço. Um ramo de 0x95b790 amostra frames
limitados por zero e `mFrameMax`, usando o acumulador e o passo acima, e chama
0x95cab0. A unidade de `mDeltaTime`, o escalonamento do loop principal e o valor
habitual de `mSpeed` não foram demonstrados. **60 FPS permanece convenção de
preview.** O limite N-1 observado é uma diferença concreta a auditar no
exportador, que atualmente preserva a regra de referência da fonte pública.

## Correção demonstrada no produtor: codecs 4/5 e referência

A extensão da investigação ao dispatcher **encontrou uma divergência**, não
somente uma incógnita. O produtor anterior usava `u/65535` e `u/255`, seguindo
a fonte pública, e inseria referência no frame zero quando `rawLoop < 1`.
O código desta ROM faz o seguinte:

| Codec | Stride de key | Delta para o próximo key | Sampler | Quantização de cada componente |
| --- | --- | --- | --- | --- |
| 4 | 8 | u16 em key+6, convertido para float | 0xae9910 | `(int(u16)-8) * float32_bits(0x37800801)` |
| 5 | 4 | u8 em key+3, convertido para float | 0xae92e4 | `(int(u8)-8) * float32_bits(0x3b888889)` |

Os multiplicadores são aproximadamente 1/65520 e 1/240, respectivamente;
os **bits** acima são a constante observada. Não se deve substituir a
representação observada por uma constante decimal arredondada arbitrariamente.
Após a quantização, os samplers calculam `bias[i] + scale[i]*quant[i]`, para
i=0,1,2. `scale` está em extremes+0/+4/+8 e `bias` em +16/+20/+24.
Não há clamp de quant: valores menores que 8 produzem números negativos.

Quando a fração é zero, retorna o key atual decodificado. Quando não é zero,
decodifica também o próximo key e combina `(1-t)*atual + t*próximo`. A sequência
é SUB inteiro, VCVT.F32.S32, VMUL.F32 e VMLA.F32; o lerp usa VMUL/VMLA e literal
1.0. A comparação numérica privada usa equações em precisão dupla com constantes
float32 exatas. Isso não simula o ambiente VFP nem estabelece equivalência bit
a bit; arredondamento e organização das operações precisam permanecer limites
explícitos do exportador nativo.

O despacho em 0xaea0e4 seleciona a tabela em 0xe315a4, stride 12. Os registros
4/5 têm os endereços acima e getters de delta em 0xde43b4/0xde43a4. O estado
comum inicializado em 0xae923c começa com cursor zero e ponteiro para o primeiro
key. **Somente codecs 1 e 2 retornam diretamente track+0x10**, ignorando o
buffer nesse caminho. Todos os demais seguem o cursor/buffer comum: não há
inserção de referência nem informação de `rawLoop` nesse estado.

Para um frame positivo, avança enquanto `keyStart + delta <= frame` e delta
não é zero. A fração enviada ao sampler é `(frame-keyStart)/delta`. Delta zero
retorna o key corrente com fração zero, incluindo o terminal. Assim, o primeiro
key começa no frame zero; um limite exato seleciona o próximo key. O retorno
temporal dentro do sampler foi preservado no assembly, mas os testes da
correção devem se concentrar no caminho crescente, nos limites e no terminal.
Buffer vazio em codec diferente de 1/2 deve continuar uma rejeição explícita
do conversor, não uma referência sintetizada.

| Evidência nova | Intervalo/conteúdo |
| --- | --- |
| `ev_02441f313eda39bd06c1494f9ebfa8bec85876427ca64be0dde2515738c9d660` | Dispatcher 0xaea0e4–0xaea268: referência 1/2, busca temporal, divisão da fração |
| `ev_7137a10978e33a9884d4000eef8ce0dd1b2087a7e283794e0806f9f0c21a3265` | Literais do dispatcher: zero, ponteiro da tabela, um |
| `ev_48961f4696c206e583bcd8a6a85e574dd40fa782f2ddf9a1737f100b6602c7d5` | Tabela, incluindo stride e destinos dos codecs 4/5 |
| `ev_0269438971bf5485fa0d3bcb6de218f10937baf292f1620567cbd1a99da0ec22` | Getters de delta 0xde43a4–0xde43c3 |
| `ev_9f30f4f3d29cfe67b5e7931b86ab1bea6a5424568cac03c2dda1d9a5692c214c` | Codec 4, 0xae9910–0xae9a5f, incluindo zero e multiplicador |
| `ev_dfbc336ac239e12306c696c4a0f7022e2d6d64553b55f1ae440b0fdbe70f84a2` | Codec 5, 0xae92e4–0xae9433, incluindo zero e multiplicador |
| `ev_6774b7a261266b57cd21d23889979aab593b20b98e68ed9c36cd075231cf2f0a`, `ev_ea72adfa892be57420f0e06fda2596f27b9b80ccecb71bda37e478faec13e254` | Literais 1.0 do lerp em 0xae9a60 e 0xae9434 |

Ghidra não reconheceu corpos de função nos quatro destinos de codecs/getters.
Por isso, a prova desses trechos é leitura de bytes REA mais LLVM ARMv6,
preservados em `root-motion/codec{4,5}-llvm.asm` e
`codec45-delta-llvm.asm`; não é pseudocódigo atribuído incorretamente.

`audit_translation.py` compara as duas equações sobre os cinco LMT, usando o
parser para layout e bounds: **225 tracks, 6.635 keys**, sendo 145 tracks de
codec 4 e 80 de codec 5. Todos os 225 têm delta terminal zero; nenhum delta
intermediário é zero. A regra antiga deslocava 199 desses tracks em um frame.
O maior desvio de componente entre as equações foi **1.08666849** unidades da
fonte, no player entrada 92608, slot 34, track 2, bone ID 0, frame armazenado 32.
É uma junta normal, não somente movimento de cena. Relatório completo e valores
empacotados: `research/private/root-motion/codec45-comparison.json`.

A correção foi encaminhada ao responsável por `animation_probe.py`, testes e
reexportações. Esta investigação não modificou o produtor. O tempo exportado
deve respeitar o domínio [0,N−1], preservando keys além desse intervalo nos
dados brutos para interpolar seu endpoint. Uma duração de preview continua
exigindo FPS explícito; corrigir frames não demonstra o FPS do jogo.

## Relação com gameplay e próximos limites

Foram seguidos quatro callers imediatos do avanço. O wrapper 0xb06f8c altera
atributos 0x10/1 conforme temporizadores, restaura posição antes da atualização
e depois combina a posição resultante com outros vetores de estado. Evidência:
`ev_08098f2a7b71908e43a6873a5a8ed0283bc50812b9b82d1f80d80728dd5eb9d2`.
Isso comprova que há política acima da camada de animação, mas não identifica
esse wrapper como o caçador ou determina qual política cada ação usa.

Para implementação fiel ainda faltam:

1. Identificar o tipo concreto desse wrapper e os setters de `mAttr` usados
   ao selecionar cada ação do player. O mesmo arquivo LMT não determina essas
   flags por si só.
2. Demonstrar a unidade de `mDeltaTime` e auditar as equações dos demais codecs
   de quaternion. O dispatcher comum já resolve a referência e o cursor;
   a auditoria numérica detalhada desta etapa cobre apenas codecs 4 e 5.
3. Seguir a composição com pais e as correções posteriores de colisão e
   controlador antes de chamar a translação de movimento mundial final.
4. Validar uma transição de ação, um loop com deslocamento e um reset contra
   observação do jogo; a derivação estática atual não é esse teste.

Um contrato nativo futuro deve preservar track de cena separado da rig,
metadados brutos de término/loop, tempo em frames e estado anterior por camada.
A aplicação ao ator deve ser explícita e possuir proveniência própria.
Exportar uma curva para inspeção é possível; chamar sua reprodução isolada de
locomoção original ainda não é sustentado. Nenhum track foi convertido em
animação inventada nesta etapa.
