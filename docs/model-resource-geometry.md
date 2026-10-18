# Geometria nativa do MOD230

`model_resource::Resource` passa a possuir os vértices e índices dos três layouts recuperados, além das tabelas de joints e paletas já existentes. A leitura ocorre uma vez no carregamento. `geometry(ordinal)` devolve em O(1) uma visão imutável ou o erro específico daquela primitiva. Os valores chegam ao consumidor sem transformação de posição, normalização de pesos/normais ou conversão de UV.

Este componente é um leitor de recursos, não um renderer ou uma reprodução da GPU. A correspondência foi comparada com os bytes de três MODs reais e com o extrator de strips existente. Não houve nova execução ARM, GPU ou jogo nesta entrega.

## Autoridade e representação

A fonte principal é [joint-shader-consumption.md](joint-shader-consumption.md), em especial o construtor de input layout `62983c`, o helper `628c20` e seus readbacks REA. O contrato privado `research/private/joint-shader-consumption-v1/contract-second/source-models.json` fixa os MODs, hashes, layouts e contagens. O ELF dessa investigação é `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`; esta implementação não o executou novamente.

O campo `primitive+0x14` é `input_layout_hash`. Não identifica um programa de vertex shader. A organização de offsets e a expansão de strips reutilizam os fatos de `tools/resources/extract_model.py`. O exportador `model_scene.py` também foi consultado, mas suas conversões de Q, pesos e normais não integram este leitor.

| Layout fonte | Hash | Stride | Influências |
| --- | --- | ---: | ---: |
| IAObj2wt | `d6784014` | 28 | 2 |
| IAObj4wt | `01b36016` | 36 | 4 |
| IAObj4wtT | `f606f017` | 36 | 4 |

O tipo consumido é `primitive[11] & 63 == 4`. Bits superiores desse byte são preservados no registro original e não mudam esse teste. Outros layouts, strides ou tipos geram `unsupported_layout` apenas para a geometria solicitada.

`Vertex` contém os seguintes fatos:

- posição XYZ e UV como words `uint32_t` com os bits float32 originais, incluindo zeros assinados e não finitos;
- quatro bytes de normal com sinal, sem divisão por 127;
- índices de paleta e pesos u8 originais, inclusive índices cujo peso é zero;
- `influence_count` igual a 2 ou 4; os slots restantes do array no layout de duas influências estão ausentes, não são bytes da fonte;
- `auxiliary` opcional com os quatro bytes assinados de `28..31`, presente somente nos layouts de stride36. O leitor não transforma esse campo em uma tangente inferida.

Não se aplica Q à posição. A matriz B usada pelo produtor da paleta já contém essa conversão. A soma dos pesos não é corrigida e índices inválidos para uma paleta não são escondidos por peso zero: cabe ao consumidor numérico validar cada matriz efetivamente acessada.

## API, identidade e tempo de vida

O dono é `src/model_resource.hpp/.cpp`. `Resource::load` e `decode` continuam exigindo SHA-256 esperado e mantêm um snapshot de bytes próprio. Cópias de `Resource` compartilham o mesmo armazenamento imutável e a mesma identidade; carregamentos distintos recebem identidades distintas. A identidade não é um endereço nem um hash truncado.

`Geometry` registra `resource_identity`, ordinal da primitiva, offsets fonte dos vértices e índices, `vertex_start`, layout/stride, spans de `Vertex`, índices u16 brutos e triângulos locais. Os índices brutos incluem os tokens `0xffff`. `restart_count` e `degenerate_triangles` descrevem a expansão. Uma visão permanece válida enquanto qualquer cópia daquele `Resource` estiver viva; a própria visão não prolonga esse tempo de vida. Chamadas repetidas não reextraem nem copiam os arrays.

`Skeleton::joint_records_parsed` acrescenta uma visão canônica de `JointRecord`: quatro bytes `fields` e cinco words `parameter_bits` por registro de 24 bytes. A visão bruta anterior continua disponível. Este dono não atribui semântica de parent/bone ID aos nomes genéricos desses campos. Quando o MOD declara zero joints, `skeleton()` continua ausente e não lê a seção ignorada pelo loader original.

Metadados, paletas e grupos carregados mantêm o contrato de [model-resource-runtime.md](model-resource-runtime.md). Uma primitiva sem geometria suportada continua com seus fatos de metadados disponíveis, e outras primitivas podem ter geometria válida. Erros estruturais dessas tabelas compartilhadas ainda podem invalidar o recurso completo.

## Offsets, strips e limites

O VBO é delimitado pelo offset de header `+0x38` e tamanho `+0x18`. A primitiva seleciona `VBO + vertex_start * stride + primitive.u32(+16)`, com `primitive.u16(+2)` vértices. A faixa declarada e a faixa selecionada são verificadas antes de qualquer leitura de vértice.

O strip começa em `header.u32(+0x3c) + 2 * primitive.u32(+24)` e tem `primitive.u32(+28) + 1` índices. Todos esses cálculos usam intermediários largos e bounds contra o arquivo fixado. Cada índice não sentinel deve pertencer ao intervalo de vértices selecionado, mesmo antes de formar o primeiro triângulo. A expansão subtrai `vertex_start`, alterna a ordem a cada triângulo candidato, inclui os degenerados nessa alternância e reinicia janela/paridade em `0xffff`. Triângulos com índices repetidos são contados e omitidos; não há teste ou reparo de área geométrica.

O campo global de contagem de índices do header não foi usado como tamanho físico do IBO. Nos três testemunhos, multiplicá-lo por dois leva a um fim dois bytes além do arquivo; as faixas selecionadas por primitiva permanecem dentro dele. O significado integral desse campo continua desconhecido. O leitor não fabrica esses dois bytes nem lê padding implícito.

Os limites padrão cumulativos são 1.048.576 vértices, 4.194.304 índices e 2.097.152 triângulos, além dos limites existentes de bytes/tabelas. São políticas configuráveis de memória e trabalho, não máximos atribuídos ao jogo. A ordem de admissão é a ordem das primitivas no arquivo. Os limites de vértices/índices e a faixa do strip são verificados antes do scan; o limite de triângulos é conferido antes de alocar os arrays. Vértices/índices admitidos continuam debitados se um erro posterior no strip rejeitar a primitiva, limitando também trabalho descartado. Falhas anteriores de layout/extent não gastam esse orçamento. Falhas de alocação/capacidade são erros tipados.

Geometria vazia ou strips que ainda não formam um triângulo são fatos permitidos, desde que os índices existentes sejam válidos. Não são tratados como um desenho visível por este componente.

## Validação executada

O produtor independente `research/private/model-geometry-native-v1/source_geometry.py` usa leituras inteiras e o dono de bounds/strips de `extract_model.py`; não importa código C++, não converte floats e não usa o exportador glTF como resultado esperado. O conjunto canônico está em `source-with-joints/`:

| Recurso | Joints | Primitivas | Vértices |
| --- | ---: | ---: | ---: |
| em062 | 12 | 2 | 1.876 |
| body001 | 19 | 4 | 1.280 |
| one133 | 1 | 2 | 503 |

No total foram comparados 32 registros de joint, 8 primitivas, 3.659 vértices e 4.210 triângulos. A fixture `geometry.bin` tem SHA-256 `e9c6004adb2ad450feb2aaadcf5b60a123312b89c5dbd9f3546517c87da3d23b`; a comparação fonte executou **104.857 checks** por variante.

`src/model_geometry_tests.cpp` também executou **115 checks de fronteira**: identidade, compartilhamento/tempo de vida, imutabilidade após alteração dos bytes do chamador, não finitos brutos, signed bytes, peso zero com índice inválido preservado, auxiliares, reinício/paridade/degenerados, layouts não suportados isolados, offsets e contagens extremos, índices fora do span, orçamentos cumulativos e débito de trabalho rejeitado.

As regressões anteriores executaram **310 checks** de limites e **5.752 checks** de fatos de dois MODs e 31 casos de agrupamento ARM previamente retidos. Reutilizar essas fixtures não representa uma nova execução ARM. Nove mutações da fixture rodaram em cada variante: digest, magic, truncamento, trailing bytes, posição, normal, índice de peso zero, triângulo e parâmetro de joint — **18 processos negativos**.

Os dois perfis foram executados em macOS arm64: normal `-O2` e ASan/UBSan `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all`. Builds usam `-Wall -Wextra -Wpedantic -Werror`. O validador exige retorno zero, stderr vazio e contagens exatas para sucessos; os negativos exigem erro e diagnóstico esperado. Fonte/header, fixture e inputs são fixados antes/depois. Comandos, saídas, binários e tempos de parede estão em `native-final/validation.json`. CPU, pico de memória da família e tokens não foram medidos.

Para repetir somente este recorte, com um diretório novo:

```sh
python3 research/private/model-geometry-native-v1/validate_native.py --output research/private/model-geometry-native-v1/new-validation
```

O executável de teste de geometria sem argumentos executa as fronteiras; com argumentos aceita `geometry.bin SHA em062.mod body001.mod one133.mod`. Esta etapa não executou a suíte geral nem Windows. Esses checks pertencem à integração separada do projeto.

## Limites da entrega

Somente os três layouts/tipo listados estão implementados. O leitor não reproduz aquisição GPU, precisão PICA, skinning, matrizes de câmera/projeção, materiais, visibilidade, clock de animação ou gameplay. A triangulação corresponde ao extrator de recursos existente e aos testemunhos selecionados; não é uma nova prova de submissão de primitivas ao hardware. Recursos e relatórios derivados permanecem privados. Backups dos donos e manifests anteriores estão em `research/private/model-geometry-native-v1/before/`; nenhuma evidência histórica foi sobrescrita.
