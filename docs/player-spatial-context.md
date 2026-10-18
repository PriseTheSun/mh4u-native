# Consulta espacial antes do contexto do caçador

O caminho `b30fa0 → 2b9f6c` prepara uma consulta geométrica e trata uma lista de resultados; não é apenas uma busca de categoria pelo ID da área. Esta entrega recupera os argumentos, filtros, preparação e callback de resultados. A travessia completa de geometria ainda não foi executada. Não se substituiu essa travessia por um resultado falso nem se escolheu um selector de startup.

O alvo é o ELF privado SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Evidências, oráculo, traces e pins estão em `research/private/player-spatial-context-v1/`. Nenhuma alteração de runtime, GUI, pacote, ROM ou launcher pertence a este recorte.

## Encadeamento e argumentos

A ordem anterior `b6b37c → b30fa0 → b3aa84` permanece a da [investigação de contexto](player-gameplay-context.md). `b30fa0` recebe jogador e três saídas. Se `pw.byte6a9==1` e as três comparações binary32 de `pbw+10/+14/+18` com `pw+6b0/+6b4/+6b8` forem iguais, copia apenas os quatro bytes de `pbw+468` para a primeira saída e retorna1. Não atualiza as outras saídas nesse ramo. O ensaio confirma igualdade entre `+0/-0`; igualdade de infinito também foi observada separadamente, sem promover infinito ao perfil geométrico finito. NaN não satisfaz o cache.

No miss, a chamada é:

```
2b9f6c(manager=*(10574f4), player,
       vector0=pbw+10, vector1=pbw+6e8, area=pbw.byte9,
       out0, out1, out2, extraFlags0=0, extraFlags1=0)
```

O significado temporal de `vector1` não é inferido apenas por estar próximo de outros campos. A preparação usa sua coordenada Y. O caller original do recorte anterior passa `out1=pbw+470` e `out2=pbw+680`; isso vincula os bytes de resultado aos campos lidos depois por `b3aa84`.

`2b9f6c` chama `b43194(player)` e `b4620c(player)` e encaminha os dois resultados em posições diferentes para `2ba838`. Eles são combinados por OR com os argumentos extras, sem serem confundidos com atributos SBC:

- `b43194`: acrescenta4 se `manager(fcf45c)+18` tem o bit de `pbw.byte9`; acrescenta `20000` para `pbw370==2`; acrescenta `40000` se `d04924` for verdadeiro.
- `b4620c`: começa em2; `pbw478&8` acrescenta800; `d05694(player,0,0)` acrescenta10000; condições de `pw5a8`, `pwf0` e `pbw371/372` acrescentam80; `d051cc` e a exceção de ações120/121 acrescentam2000; `pbw370==2` acrescenta400000.

Esses dois produtores foram lidos estaticamente, com assembly e bytes completos. Não foram contados como executados pelo oráculo desta entrega. O pseudo de `b43194` mostra um cast assinado, mas o assembly usa `LDRB`; conserva-se o byte sem sinal e a semântica ARM do shift.

## Preparação original `2bad48`

`2ba838` constrói na stack um objeto de consulta Q, inicializa16registros e chama `2bad48`. A preparação completa foi executada com o helper real `2b9a04`; o argumento de manager não é lido nesse perfil. O template runtime `10e6370` é uma entrada explícita, não um BSS presumido zero.

Para `vector0=(x,y,z)` presente, a função produz:

| Campo de Q | Valor observado |
| --- | --- |
| `+10` | `(x, y+10000, z, 0)` |
| `+20` | `(x, y−10000, z, 0)` |
| `+38` | `(x, max(y,vector1.y)+10000, z, −1)` quando vector1 existe; usa y quando ausente |
| `+b0` | 100000 |
| `+b8` | y |
| `+bc` | y+h |
| `+c0` | y+(h+d) |
| `+c4` | zero |
| `+c8` | 2 se flags&1; zero caso contrário |
| `+4d0` | contagem byte zero |
| `+4d4` | −100000 |

`h` começa em0 se flags&2; senão50 se flags&8; senão100. Se vector1 existe, `dy=vector1.y−y`. Quando `h<=dy`, a função substitui h por `dy+h`; quando `vector1.y<=y`, d recebe dy, senão d é zero. Cada operação observada é binary32. Em particular `+c0` é calculado como `y+(h+d)` no assembly, apesar da associação diferente exibida pelo pseudocódigo.

Na ausência de vector0, x/y/z vêm do template, vector1 é ignorado e a segunda coordenada de `Q+38` é10000, independente do Y do template. O ensaio usa template `(11,12,13)` justamente para distinguir esse caso de um zero conveniente.

As unidades e os nomes físicos dessas formas não estão provados. Os números10000/100000 são literais do jogo, não recomendações de escala para o renderer. O perfil de chamada `b30fa0` não produz os bits1/2/8 por `b43194`; os testes desses bits cobrem a API mais ampla de preparação, não implicam que todos ocorram nessa chamada do caçador.

`2b9a04` inicializa o registro de48bytes: seis bytes zero, u32 em+8/+c/+10 zero, vetor em+18 `(0,1,0)` e floats+24/+28/+2c zero. Não escreve os bytes+6/+7 e+14..17. A preparação de Q preserva padding e demais campos não escritos; o teste verifica isso com padrões00/a5/ff, em vez de transformar esses bytes em defaults.

## Busca ainda não fechada

Depois da preparação, `2ba838` configura um filtro por `198154`, usando os flags de `b4620c | 03000000` e a máscara ARM `1<<area`. A busca segue:

```
2ba838 → 189440 → 18931c
                    → 1868c8 (objeto auxiliar)
                    → 194128 (despacho/contexto)
                    → 188d10 (busca)
                    → 1869cc (liberação auxiliar)
```

O callback `2bb838` é passado por dados em `e062ec+4`; o contexto é Q. Há vtables e tabelas de callbacks distintas nesse caminho. Os bytes estáticos de tabelas potencialmente inicializadas em runtime não foram usados como garantia de readiness.

A próxima fronteira executável é construir o manager real esperado por `188d10`, selecionar seu caminho para a forma tipo21 de `Q+30`, fechar os consumidores geométricos e produzir o carrier do resultado. A preparação e os callbacks abaixo não estabelecem a interseção. O retorno de ausência de resultado da busca não foi forjado nem testado com um helper substituído.

## Callback original `2bb838` e decodificação

O Ghidra não reconheceu `2bb838` como procedimento. A leitura pública REA052 retém o corpo completo de376bytes, incluindo os dois literais; a análise e execução usam os bytes ARM e o LLVM com VFP2, sem criar função no provider ou editar o alvo.

O callback lê a altura do resultado em `point+4`. No subconjunto finito executado:

1. Obtém `f=cd337c(carrier,1)`, que no perfil executado lê `attribute[2]`.
2. Se `f&1046`, atualiza `Q+4d4` com o máximo anterior/altura. Se também `f&1004`, retorna sem inserir.
3. Descarta uma altura exatamente igual a qualquer entrada existente.
4. Insere por altura crescente, movendo os registros com o helper original `14753c`.
5. Marca o registro presente, copia a altura e chama `2b9474` para preencher seus atributos; incrementa o byte de contagem.

O ensaio cobre inserção crescente, decrescente, duplicada e até15resultados. Há um clamp do índice de inserção para15, mas o byte de contagem ainda é incrementado pelo código; o comportamento no limite16/overflow e o limite imposto pelo manager não são generalizados aqui.

O perfil de carrier executado é explícito: `carrier+1c=ffffffff`, `+34!=0`, `+40` aponta um registro de atributos, `+38` aponta um vetor e `+4c=0` indica ausência de matriz. Não se afirma que esse seja o carrier produzido pelo cenário real. Nesse perfil, `cd337c(carrier,i)` lê `attribute[1+i]`; `cd2fec` copia o vetor sem normalizá-lo.

`2b9474` decodifica `w=attribute[1]` no registro de saída:

| Saída | Fonte |
| --- | --- |
| byte0 | low8(w) |
| byte1 | (w>>8)&7 |
| byte2 | (w>>11)&1 |
| byte3 | (w>>16)&15 |
| byte4 | (w>>20)&15 |
| byte5 | w>>24 |
| u32+8 | attribute[2] |
| u32+c | attribute[4] |
| u32+10 | attribute[0] |
| vetor+18 | vetor retornado por cd2fec |
| float+24 | zero |

Logo, quando essa saída é copiada a `pbw+470`, `pbw473` provém dos bits16..19 de w e `pbw475` de seus bits24..31. Esse é um vínculo estático ao consumidor contextual anterior, sem atribuir nomes como piso, parede ou dano.

O helper `14753c` copia os campos definidos e força float+24 a zero; não é um memcpy de48bytes. O callback executado mantém essa semântica também nos deslocamentos da lista.

Dois casos adicionais alimentam o carrier com os registros de32bytes do SBC real `v00a01-a-g00` (entry93738, SHA `3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07`). Eles são obtidos pelo parser proprietário `collision_probe.parse_collision`, após verificar o hash. As alturas de interseção continuam sendo inputs sintéticos. Esses casos provam o consumo desses bytes no perfil declarado, não o vínculo do loader SBC ao producer do carrier nem uma colisão contra os86triângulos reais.

A seleção de um resultado quando há vários ocorre depois, em `2bb9b0`: consulta `69d568`, `6c2d18`, `6c0eb0` e bits10/2000 do atributo. Seu corpo e literais estão retidos, mas ele não foi executado nesta entrega; a presença de alturas ordenadas não permite escolher sempre a primeira nem a última.

## Verificação e proveniência

`arm-final-v2/validation.json` é autoritativo: **177casos positivos,8controles negativos,126.592instruções originais**, Unicorn2.1.4/ARM11MPCore. São142preparações,3cache-hits e32sequências de callbacks. Guards restringem execução, leituras inicializadas/literais, escritas e orçamento. Nenhum hook entrega retorno ou modifica código/registradores para substituir helper.

Os controles retiram helper, template, campo do cache, ponteiro de atributo e três condições de hit do cache. Todos falham pelo guard; os misses não são reportados como sucesso com saída vazia. O cálculo independente de campos e a conferência de ordenação/preservação de bytes acompanham o trace completo comprimido.

`arm-callback/failure.json` preserva uma falha de expectativa: o primeiro teste leu os words mostrados pelo LLVM como se fossem bytes invertidos. Os literais reais são1046/1004. A correção foi feita apenas na equação independente de expectativa; o código ARM não mudou. `arm-initial`, `arm-complete` e `arm-final` são etapas anteriores preservadas. A revisão final-v2 mantém os mesmos resultados e representa NaN/infinito do ensaio de cache por bits binary32 em JSON válido.

REA sessão1792303785054, Ghidra12.1.4, perfil focused, heap3GiB. Primeira resolução/importação levou148,084s. O ensaio final levou aproximadamente0,619s, excluindo importação. CPU, pico de memória da família e tokens não foram medidos. A sessão anterior1792301774362 foi confirmada fechada antes da nova importação. A sessão nova é exportada e fechada no lifecycle privado; nenhum launcher é manipulado.

O índice `source-facts.json` verifica todos os readbacks novos byte a byte contra o ELF. Também retém as consultas focadas atendidas para o intérprete da tabela do caçador: `b09da8/b064fc`, `b08770`, `b05758/d024b0`, `b3ca88`, `6b398c`, `b6b120`, `d05ac0`, `b242e8` e `d055c0`. A execução desses helpers pertence às investigações de seus donos; não é somada ao oráculo espacial. `b064fc` e `6b398c` têm corpos Ghidra incompletos por jump tables, e os spans fonte completos foram retidos para derivação pelo LLVM.

Reprodução em diretório novo:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-spatial-context-v1/verify_spatial.py --output research/private/player-spatial-context-v1/recheck
python3 research/private/player-spatial-context-v1/retain_facts.py
```

A integração futura ainda precisa da busca completa, do producer do carrier, da seleção entre hits e da ordem de readiness. Este recorte não autoriza substituir o contexto original por raycast genérico nem habilitar uma política de caminhada baseada em máscaras semânticas presumidas.

A sessão foi exportada em090 e fechada em091, seguida de saída0 do bridge; a inspeção de processos não encontrou REA/Ghidra remanescentes. O bundle retém88Evidence únicas. Os25readbacks novos somam5.192bytes, mais dois readbacks reutilizados de b30fa0. Todos os16intervalos de código/literais do ensaio estão integralmente cobertos por essas leituras públicas. A primeira auditoria havia incluído apenas a leitura de cauda141; a falha e a correção pela inclusão do prefixo140 permanecem registradas.
