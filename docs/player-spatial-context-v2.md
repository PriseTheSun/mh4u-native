# Interseção espacial original e vínculo ao SBC

O recorte fecha a produção do carrier de uma face SBC e a interseção vertical tipo21 até o callback original de atributos. **Não fecha a busca completa `18931c → 188d10`**, que escolhe objetos, grupos e pares. O experimento executa instruções ARM originais e não substitui essa busca por um retorno vazio ou por um primeiro hit.

A investigação anterior está preservada em [player-spatial-context.md](player-spatial-context.md). O novo material privado está em `research/private/player-spatial-context-v2/`. Alvo: ELF SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Não houve alteração de runtime, parser, GUI, pacote, ROM, ELF ou launcher.

## Fronteira executada

O ensaio constrói explicitamente os ponteiros de manager, objeto, recurso, carrier e auxiliar. Copia sem alteração os bytes do SBC `stage\v00\v00a01\collision\v00a01-a-g00`, entry93738 de `/loc/data/v00a01.arc`, SHA-256 `3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07`. O parser proprietário `collision_probe.parse_collision` valida spans, grupos e índices antes do ensaio.

O recurso tem um grupo,86pares,86faces,48vértices e2atributos. Todos os pares têm segunda face `ffff`; o recorte corresponde a esse ramo do original. No recurso observado, os86IDs de atributo do par coincidem com os da respectiva face. A coincidência não substitui o caminho de leitura: **`194388` usa o atributo do par em `pair+8`**.

A sequência executada é:

```
2bad48 → 2b9a04                    preparação da lista Q
194128                            instalação dos callbacks em C
195d24 → 186864                    preparação tipo21; callback opcional ausente
194388 → cd2f04                   montagem do carrier; filtro opcional ausente
       → 193884 → cd3148/cd3348   triângulo e gate pela normal
                 → cc94f0        virtual+b8 da forma tipo21
                    → 1eb400 → bdcf50
                 → 1867f4 → 2bb838 → cd337c/2b9474/cd2fec
```

As funções são chamadas desde suas entradas e retornam normalmente nos ramos declarados. `186864` e `cd2f04` executam seus próprios branches de callback ausente; não são interceptadas. Os hooks apenas observam e impõem limites. Nenhum hook escreve PC, registradores ou retornos para imitar helpers. O código e o SBC copiado são protegidos contra escrita.

O estado de entrada é um perfil de experimento, não uma reconstrução do startup: objeto0, grupo0, `carrier+1c=ffffffff`, `+18=0`, `byte+32=0`, `byte+48=0`, `+4c=0`, sem transformação, filtro `F+30=0`, callback opcional `A+174/+178=0`. A função de resultado é o callback original `2bb838`, com Q como contexto. O template runtime `10e6370=(0,0,0)` é um argumento explícito. RAM restante começa com poison, sem ser considerada inicializada pelo guard.

## Contrato numérico e campos

`194128(M,S,C,A,F)` grava `C+0=M`, `C+d4=A`, `C+d8=F`, `A+d4=F`. Para forma diferente de7 e slot `C+94=0`, instala em `C+c/+10` o par `(193884,0)`; também instala `19578c`, `195d24` e `191f74` nos slots seguintes. O oracle executa esse instalador; não fabrica o callback da face.

`195d24` no ramo tipo21 escreve `A+e8=A+e0` (ponteiro da forma ativa), `A+70=&A+40`, copia os quatro floats `S+8..17` para a forma apontada por `C+f8` e grava esse ponteiro em `C+100`. Copia `A+f0/+f4/+f8` para `A+100/+104/+108`, com `A+10c=0`. No perfil, a forma contém `(x,y,z,-1)` e o vetor de teste contém `(0,-1,0)`. O estado que fornece esses campos é explícito; a produção mais ampla por `188d10` não foi executada.

`194388(M,C,pairIndex)` usa:

| Fonte | Consumo / saída |
| --- | --- |
| `M+88`, `carrier+8` | vetor de objetos e índice do objeto |
| `object+54` | objeto rCollision K |
| `carrier+34` | grupo atual G |
| `K+bc`, `carrier+c` | tabela de grupos e índice do grupo para localizar pares |
| `group.u32(+2c)` | primeiro par, stride10 |
| `G.u32(+34)` | primeira face, stride32 |
| `G.u32(+3c)` | primeiro vértice, stride16 |
| `K+c4` | base dos pares |
| `K+c8` | base das faces |
| `K+cc` | base dos vértices |
| `K+c0` | base dos atributos, stride32 |
| `pair.u16(+8)` | atributo: grava seu ponteiro em `carrier+40` |
| `pair.u16(+0)` | face: grava índice em `carrier+14` e ponteiro em `+38` |
| `pair.u16(+2)==ffff` | ramo de face única executado |
| `pairIndex` | gravado em `carrier+10` |
| base de vértices do grupo | gravada em `carrier+3c` |

Cada caso verifica os cinco campos produzidos `+10/+14/+38/+3c/+40` contra os offsets do parser. Isso fecha a associação **bytes SBC → par → face/vértices/atributo → resultado**, condicionada aos ponteiros de recurso/grupo de entrada. Não prova o loader nem o registro do objeto no manager.

`cd3148` lê os três índices u16 da face em `+c/+e/+10`, busca posições no array de vértices com stride16 e escreve três vetores com W zero. `193884` usa a normal fonte em `face+0/+4/+8`: se o produto escalar com `A+100` é positivo, rejeita. Caso contrário chama o slot virtual+b8 da forma. O readback da vtable `e5b248+b8` contém `cc94f0`; essa chamada virtual é executada no oracle.

`cc94f0` calcula o plano a partir dos três vértices. Normaliza o produto vetorial quando o comprimento binary32 tem representação assinada >=`34000000` (2⁻²³). Chama `1eb400`, que intersecta `(x,y,z)+t*(0,directionY,0)` com o plano. Rejeita se `abs(normal.y*directionY)` tem bits inferiores a `3727c5ac` (aproximadamente10⁻⁵), ou se `t<0`. O caminho finito observado não impõe máximo de t nessa folha. A aceitação dentro do triângulo usa três testes orientados no plano XZ, com igualdade aceita. As operações ARM/VFP são binary32; a referência independente usada no ensaio é uma solução baricêntrica em precisão dupla.

**Normal do hit e normal do atributo são campos distintos.** Para os hits descendentes com origem acima do triângulo, `1eb400` retorna no hit a normal oposta à calculada do triângulo. O ensaio compara essa normal com o produto vetorial independente (erro máximo `1,34647e-7`). Depois, `2b9474` obtém a normal fonte do SBC via `cd2fec`, sem normalizá-la, e grava-a no registro de atributos. O ensaio compara esse segundo vetor byte a byte com a face fonte. Não se deve trocar esses campos por uma única normal de render.

O callback `2bb838` insere alturas sem repetição exata e conserva os atributos conforme o contrato da v1. Todos os casos novos produziram no máximo um hit por posição, inclusive a enumeração declarada de86pares; portanto esta entrega não acrescenta prova nova da seleção final entre múltiplas superfícies. `2bb9b0` continua fora da execução.

## Busca anterior: fatos estáticos e próximo limite

A retenção confirma a cadeia `18931c → 194128 → 188d10 → 188850`. `188850` percorre famílias distintas de containers do manager e depois chama `183be0`; não é uma lista simples de todos os triângulos. Há gates de flags/máscaras, formas, callbacks, listas e árvores. A cadeia estática de uma família inclui:

```
188850 → 3eb178/3e73f0 → 18dc48
        → 15af48 → 1923f4 → 15d720 → 17e9a0/17e754
        → 15ec48 → 183990 → 18a9f4 → 194388
```

Essa cadeia é uma derivação do código retido, não uma execução completa. Há outras famílias/branches. `3e73f0`, `15af48`, `15ec48` e vários callbacks foram inspecionados apenas no trecho necessário; os arquivos com sufixo `prefix` não garantem corpo completo. Não se afirma ausência de geometria quando uma dessas estruturas não foi fornecida.

No branch tipo21 de `188d10`, a forma AABB local encaminhada a `188850` tem inicialização parcial visível; não foi demonstrado que todos os XYZ usados depois por sua virtual+20 tenham sido escritos nesse branch. Antes de um ensaio integral será necessário rastrear essa preparação e os produtores dos containers. O próximo recorte concreto é a família `M+98+80*group`, seu container/árvore e `18dc48`, mantendo a identidade do objeto/grupo até o carrier. O experimento desta entrega enumera pares explicitamente apenas para conferir a folha, sem apresentar isso como a busca original.

Continuam desconhecidos: readiness do manager, construção das árvores e bound efetivo da lista de resultados; pares com duas faces; filtro dinâmico; transformações e formas alternativas; seleção final entre hits; vínculo de máscaras a piso/parede/material; unidades físicas; scheduler e estado inicial real do caçador.

## Verificação, custo e proveniência

O relatório autoritativo é `arm-final/validation.json`: **274 casos positivos,5 controles negativos,726.429 instruções originais,102 hits**. São258 consultas isoladas (acima/abaixo/fora para cada um dos86pares),14 enumerações declaradas em ordem direta/inversa e2 variações de poison00/ff, além do padrão a5. O maior erro de altura contra a referência independente é `0,0002321137021` em unidades fonte, dentro da tolerância declarada `0,002`.

Os negativos retiram o helper de interseção, buffers de pares/vértices/atributos e ativam uma forma alternativa fora do perfil. Todos interrompem nos guards. Não retornam um miss silencioso. Leituras exigem bytes inicializados ou literais retidos; execução e escritas são limitadas aos intervalos declarados. Os índices de entrada são validados pelo parser/harness; não se afirma que o jogo faça bounds checks nesses arrays.

O primeiro ensaio `arm-first` passou e permanece preservado. `arm-final` acrescentou a comparação independente da normal, proteção explícita do SBC contra escrita e remoção de ranges de cache não usados. Nenhuma falha de execução original ocorreu nesse recorte. A auditoria de artefatos confirma os39intervalos declarados integralmente cobertos por readbacks públicos REA; os24readbacks novos somam10.732bytes, todos idênticos ao ELF. Inclui as consultas cruzadas: cauda de `6b398c` (007), `b2a7ec` (010–012) e `d0506c` (013–015), cuja execução pertence ao oracle do intérprete, não à contagem espacial.

REA/Ghidra12.1.4, perfil focused, heap3GiB, sessão1792304841481. A importação/resolução fria levou152,620s; o ensaio final levou2,404s, excluindo REA. CPU, pico de memória da família e tokens não foram medidos. `source-facts.json` retém IDs de Evidence, operações, readbacks e cobertura; `traces.json.gz` contém PCs, leituras, escritas, inputs e saídas; nenhum byte de jogo faz parte de uma saída pública.

A sessão foi exportada em065 (`rea-evidence.json`,63Evidence), fechada em066 com snapshot, seguida de saída0 do bridge e inspeção sem processos REA/Ghidra remanescentes. A sessão anterior já estava encerrada; não houve importação duplicada nem manipulação do launcher.

Reprodução em diretório novo:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-spatial-context-v2/verify_spatial_v2.py --output research/private/player-spatial-context-v2/recheck
python3 research/private/player-spatial-context-v2/retain_facts.py
```

O segundo comando audita o snapshot `arm-final` existente. Os pins finais e a limpeza estão em `manifest.json` e `lifecycle.json`. Esta evidência permite implementar futuramente a folha de consulta e a montagem do carrier; não autoriza chamar um raycast genérico de política espacial original completa.
