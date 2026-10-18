# Oráculo completo de consumo das animações pendentes

O novo ensaio executa `9577ec` até seu retorno original, incluindo o getter
`cd7540`, `95aa90`, `aea0e4` e seus decoders, ou o tailcall `95c7dc` quando
o ramo exige restauração. São **1.664 execuções ARM**, com as **122 ações
presentes** dos LMT de entradas 54913 e 92608. O comparador nativo passou
259.170 checks e comparou **17.448.568 bytes** das arenas de saída por execução,
sem diferenças, tanto normal quanto com ASan/UBSan no macOS ARM64.
Este documento registra o oráculo; a implementação e a validação
nativa final pertencem a `motion_pending` e aos relatórios em
`research/private/motion-pending-native-v1/`.

O ensaio anterior de 561 casos, descrito em [motion-pending.md](motion-pending.md),
continua preservado. Ele cobria ações ausentes. O conjunto novo acrescenta a
composição completa com recursos presentes; não substitui seus resultados nem
promove o ensaio a uma execução do controlador do jogo.

## Fontes e execução

O produtor é `research/private/motion-pending-runtime-v1/verify_arm.py`.
Ele usa o parser existente `tools/resources/animation_probe.py` para fatos de
relocação. Não calcula resultados de animação em Python. A cópia de cada LMT
em memória recebe as relocações de ponteiros consumidas por esta cadeia;
os arquivos de origem permanecem inalterados. Os ponteiros de dados de eventos
que a cadeia não lê não recebem uma interpretação adicional.

| Fonte | SHA-256 |
| --- | --- |
| ELF ARM com BSS | `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435` |
| LMT entry 54913 | `dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a` |
| LMT entry 92608 | `c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8` |
| Fixture final | `0048bd4f08d087312c59f7c96ff02339612d085375816d7c79801b22f6dbe67b` |
| Relatório ARM final | `8b656f8b8bb65446278039b74b306bd3d8b68a2c9b38af232484472a2bfde395` |

O relatório `arm-final/arm-validation.json` retém os intervalos de código e
literais, seus offsets no ELF e hashes, dependências, relocações, contagens e
PCs executados. A evidência REA foi reutilizada dos recortes de pending,
initialization e sampler; nenhuma nova sessão foi aberta para este ensaio.
As ligações com os consumidores originais estão em
[motion-pending.md](motion-pending.md),
[motion-initialization-runtime.md](motion-initialization-runtime.md) e
[motion-sampler-runtime.md](motion-sampler-runtime.md).

Unicorn 2.1.4 executou ARM11MPCORE, little endian, FPSCR inicial 0. O harness
permite somente instruções dos corpos recuperados, leituras inicializadas e
escritas nas arenas de estado/stack declaradas. Literais não são executáveis
no guard. Não há stubs, alterações de instruções, redirecionamento de PC,
retorno de helper fabricado ou substituição dos resultados do sampler.
As páginas LMT são protegidas para leitura e seu conteúdo é confrontado após
cada caso. O modelo inteiro de 8192 bytes é retido, incluindo o complemento
dos campos interpretados. ABI e registradores preservados são conferidos.

Foram 4.098.615 instruções, 1.034.097 leituras e 702.756 escritas. O maior caso
executou 4.984 instruções. No host macOS, o produtor mediu 6,915 s de parede,
7,725 s de CPU e 216.367.104 bytes de RSS máximo do processo. São medidas desta
execução local com fontes disponíveis; cache frio/quente e custo total de
agentes não foram medidos.

## Cobertura observada

| Observação | Quantidade |
| --- | ---: |
| Chamadas reais do getter | 1.512 |
| Chamadas reais de 95aa90 | 2.544 |
| Dessas chamadas, ação presente | 1.506 |
| Chamadas reais de aea0e4 | 7.572 |
| Tailcalls de 95c7dc | 24 |
| Casos com primeiro ativo diferente do primeiro pendente | 1.020 |
| Casos com mais de um inicializador, em ordem descendente | 945 |
| Layers cujas8 palavras de máscara permaneceram idênticas | 3.865 |
| Layers com bit 100 anterior preservado | 515 |

As 122 ações são exercitadas com 9 combinações de atributos cada. Existem
layouts com 1–3 layers, pendentes intercaladas com layers não pendentes e
selector `ffff`. A colocação dos recursos nos bancos 0–15 é explícita e
sintética. O índice mais baixo ativo não é usado como substituto do índice
pendente. O getter resolve também bancos ausentes e slots realmente vazios,
sem confundir essas rotas com ações presentes.

Há 288 casos adicionais para os predicados de bits: interpolation abaixo,
igual e acima de `34000000`, infinitos e NaN; peso com zeros de ambos os
sinais, subnormais, valores negativos/positivos, infinito e NaN; remaining
com zeros, subnormais, 1 e NaN. O primeiro ativo é diferente do pendente
nesses casos. A ação ausente isola os gates, sem enviar os valores especiais
à aritmética do inicializador. A observação confirma:

- interpolation usa comparação **signed dos bits** com `34000000`;
- o peso compara numericamente com zero: positivo subnormal e +Inf passam,
  NaN e ±0 não passam;
- remaining diferente de ±0 mantém/escreve bit 100, incluindo NaN;
- o bit 100 não recebe limpeza automática no caminho oposto.

Os 24 casos de restauração cobrem templates e matrizes, incluindo N=0.
O audit compara a rotação, translação e escala de B com **J anterior** ao
tailcall, separadamente do J restaurado. Os canais retêm suas palavras de
frame/cursor durante o reset parcial; apenas os campos que o código original
limpa ou o binder posterior escreve são alterados.

## Fixture e consumidor de teste

`arm-final/oracle.bin` é um contêiner de teste little endian, versão
`MH4UPN01`. Não é um formato novo de recurso do jogo nem de distribuição.
Começa com magic 8, count u32 e source_count u32. Cada fonte contém entry u32,
base ARM u32, tamanho u32 e SHA256 bruto 32. Cada caso tem tamanho u32 e:

1. seed, N, C e quantidade de itens do trace, todos u32;
2. 16 índices de banco u32: zero ausente; 1/2 identificam a fonte fixada;
3. 68 bytes dos snapshots globais, mapa 256 e N matrizes de64 bytes;
4. estado anterior: modelo 8192, J de N×168, B de N×56 e C arrays A de N×52;
5. estado posterior com os mesmos tamanhos;
6. trace de triplas u32: getter 1, initializer 2, sampler 3 ou reset 4.

Os snapshots globais seguem a ordem baseT 10e62c0, T 10e6370, Q 10e6560,
fallback 10e6570 e S 10e6380. Todos são entradas declaradas, não leituras
inventadas da BSS. As arenas J/B/A são disjuntas; sua identidade não é
derivada apenas da igualdade dos valores.

`src/motion_pending_resource_tests.cpp` recebe o digest externo da fixture
antes de interpretar os records. `motion_resource::Resource` é o único
parser nativo de LMT. Seus `action_info`, `binding_action`, `event_groups`
e `sampler_track` produzem a ponte entre endereços sintéticos e identidades
vivas. O teste não decodifica outro header ou track de LMT. O adaptador
reconstrói todas as arenas a partir da saída nativa e do complemento original
preservado, depois compara cada byte com a execução ARM.

```text
motion-pending-resource-tests oracle.bin oracleSHA \
  54913 entry-54913.lmt sourceSHA54913 \
  92608 entry-92608.lmt sourceSHA92608
```

Além da comparação, há cinco rejeições compostas: ação não resolvida,
identidade divergente, sampler não resolvido, template ausente e extensão
de snapshots insuficiente. Os estados de entrada permanecem iguais.
`artifact-validation.json` retém seis negativos do contêiner: digest
alterado, truncamento com digest de teste recalculado, contagem excessiva,
record excessivo, identidade de fonte divergente e alteração deliberada
somente da saída esperada. O último confirma que o comparador detecta
divergência real, além de verificar hashes.

## Limites

Este é um ensaio de funções originais completas **dentro do perfil descrito**.
As contagens observadas são N=0/1/3/8 e C=0–3; contagens extremas do domínio
ARM não foram executadas. O mapa de bones, os transforms anteriores, matrizes,
snapshots globais e bancos são entradas sintéticas explícitas, não estado de
gameplay capturado. O perfil tem arenas válidas e disjuntas; aliases ou
ponteiros inválidos do programa original não recebem semântica nativa.

O binder amostra canais do modelo; ele não avalia uma pose completa dos joints.
Os codecs de joints presentes nos LMT são ligados e preservados, mas não
ganham cobertura numérica adicional por este ensaio. Não houve avanço de
clock, execução de scheduler, dispatch de gameplay ou rótulo de movimento
como andar/correr. FPSCR final e flags de exceção de hardware não fazem parte
do estado publicado pela API nativa. O teste não demonstra execução Windows,
console, renderização ou conclusão do porte.
