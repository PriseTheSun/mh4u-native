# Oráculo completo da consulta de movimento do modelo

O ensaio executa `958884` completo, o helper de frame `d12f8c` e os samplers
originais, sem stubs. O snapshot autoritativo **arm-complete** contém
**860 sequências e 5.002 consultas**. Dessas, **4.998** têm resultados nativos
comparados bit a bit; **4** produzem quaternion de norma zero no original e
são rejeitadas explicitamente pelo perfil numérico nativo. Nenhum resultado
finito é fabricado para essas quatro consultas.

O comparador verifica o modelo inteiro de 8192 bytes, inclusive o complemento
não interpretado, os canais/cursors, identidades lógicas, presença das saídas,
contagens e os 32 bytes de saída. São **41.103.552 bytes comparados** por
execução nativa, com 171.074 checks. As variantes normal e ASan/UBSan
passaram no macOS ARM64; o relatório original e a repetição justificada sobre
arm-complete estão em `research/private/motion-query-native-v1/`
(`native-validation.json` e `cursor-coverage-validation.json`). A implementação `motion_query` pertence
ao dono do runtime; este recorte entrega o produtor independente, a fixture,
o adaptador de testes e a evidência.

## Origem e cobertura

O ponto de partida está em [joint-update.md](joint-update.md). Os 76 cenários
históricos de consulta são preservados e suas configurações são reexecutadas
no novo harness; os 154 resultados antigos permanecem intactos. O novo conjunto
contém:

| Sequências | Quantidade | Perfil |
| --- | ---: | --- |
| Históricas | 76 | Referências constantes ou canais ausentes |
| LMT reais | 366 | 122 ações × 3 layouts, 11 consultas sequenciais cada |
| Sintéticas | 416 | Mistura, espelho, gates, frames e cópias raw |
| Domínio numérico rejeitado | 2 | Mistura normal/aditiva de norma zero, com e sem saídas |

As sequências reais recebem os canais resultantes de uma execução completa
do `9577ec` original, incluindo getter, inicializador e sampler. Essa
preparação usa estados, mapas e bancos explicitamente sintéticos. Seu modelo
posterior serve somente como entrada da consulta; a saída esperada de cada
consulta vem de `958884`, nunca do runtime C++ nem de curvas Python em double.
`arm-complete/preparation.json` retém os hashes, intervalos de fonte e contagens da preparação.

| Observação ARM | Quantidade |
| --- | ---: |
| Chamadas de aea0e4 | 17.878 |
| Chamadas de d12f8c | 2.114 |
| Consultas que mudam cursor | 1.347 |
| Consultas com ambas as saídas ausentes | 1.341 |
| Dessas, com mudança de cursor | 189 |
| Consultas contendo mistura normal | 497 |
| Consultas contendo mistura aditiva | 3.148 |
| Consultas com layer aceita espelhada | 1.975 |

As consultas nativas bem-sucedidas publicam 17.862 amostras; as outras 16
chamadas ARM pertencem às quatro consultas rejeitadas.

Essas categorias podem se sobrepor. O exemplo sequência 79, query 4,
entry 54913/slot 3 demonstra mudança de cursor com ambas as saídas nulas.
O snapshot anterior **arm-final** tinha 975 consultas sem saídas, mas nenhuma
mudava cursor: os pontos escolhidos permaneciam no mesmo intervalo ou já
estavam no fim. Essa limitação foi detectada pela auditoria. **arm-complete**
altera somente a seleção de saída da consulta a 37% do domínio nas sequências
reais; mantém os mesmos frames, estados resultantes, chamadas e 5.002 consultas.
Os snapshots anteriores e seus relatórios são preservados.

Mistura normal, mistura aditiva, attr100, attr800, layers inativas/pending,
primeira layer após inativas, referências não unitárias, pesos fora de 0..1,
ausência de canais e consultas para frente/para trás estão cobertas. Os casos
raw incluem ±0, subnormais, infinitos, NaNs e valores junto de 2²³. Para esses
valores especiais, codecs de referência/ausência evitam atribuir uma
interpretação numérica inválida aos buffers comprimidos.

## Bytes originais e helper de frame

ELF fixado: SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Os corpos de 958884 e dos samplers reutilizam a retenção REA anterior. O novo
helper foi lido na sessão 1792291312711:

| Evidência | Conteúdo |
| --- | --- |
| 003-read_bytes | 156 bytes, `ev_ae0a77ce7ab5ae0267f9f76438ae575b46b5814864e20eb7ab91c2e74951283a` |
| 011-procedure_pseudo_code | Pseudocódigo do helper |
| 012-procedure_assembly | `ev_903ddd239688d016f4d32adaca357a387b5d22aa8cf5aab91fc668d32b057d37` |

O corpo está em `[d12f8c,d13018)` e os literais em `[d13018,d13028)`.
Não há chamadas a outros helpers. Os 156 bytes retornados foram comparados
integralmente com o ELF, e a evidência foi copiada para
`research/private/model-motion-query-v1/helper-evidence/`.

O helper remove a fração para frames finitos não negativos; preserva −0 e
retorna os bits originais para expoente≥23, infinito e NaN. O código contém
uma soma com literal 1e30 para decisão/efeitos de FPSCR. A API nativa preserva
o resultado relevante à consulta e não publica flags de exceção de hardware.
Os traces retêm frame antes do helper e frame passado ao sampler.
**Esses frames são observações ARM, não uma comparação direta de trace
nativo:** a API nativa expõe saídas, cursores e contagens. A equivalência
verificada é dessas saídas e estados observáveis, inclusive em sequências
reais comprimidas que usam attr800.

O harness usa Unicorn 2.1.4, ARM11MPCORE, little endian e FPSCR inicial 0.
Executa somente os intervalos permitidos, rejeita leituras não inicializadas,
escritas fora de outputs/cursores/stack e retorno fora do orçamento. Confere
SP, registradores preservados, bytes das fontes e templates imutáveis.
Não modifica instruções, PC, resultados de helper ou valores de amostra.
Páginas LMT são somente leitura; a cópia relocada é gerada pelo parser dono
existente e não altera os arquivos originais.

O ensaio final mediu 1.706.030 instruções, 610.205 leituras e 265.566 escritas;
máximo de 1.674 instruções por consulta. O produtor completo, incluindo a
preparação, levou 9,233 s de parede e 9,958 s de CPU, com RSS máximo de 235.732.992
bytes no macOS. São medidas locais com arquivos já disponíveis; cache
frio/quente, memória da família REA e custo total de agentes não foram medidos.

## Contrato da fixture e do adaptador

Produtor: `research/private/model-motion-query-v1/verify_arm.py`.
Consumidor: `src/motion_query_resource_tests.cpp`.
Fixture: `research/private/model-motion-query-v1/arm-complete/oracle.bin`, SHA-256
`699799261bf45dd60445e7fa603f9e9c42390d55368527ab2083c6b035ddb603`.

`MH4UMQ01` é um contêiner de teste little endian, não um formato distribuível
de animação. Cabeçalho: magic 8, quantidade de sequências u32, quantidade de
fontes u32. Cada fonte fixa entry, base ARM, tamanho e SHA-256. Cada sequência
tem tamanho prefixado e contém seed, quantidade de layers/queries/descriptors,
templates T3/Q4, modelo inicial 8192 e descritores de referência sintéticos
explicitamente declarados quando aplicável. Cada query contém frame raw,
seleção de saída, classificação de domínio, quantidade de trace, outputs 32
anteriores/posteriores, modelo posterior 8192 e triplas do trace ARM.

O adaptador exige o SHA externo antes de interpretar a fixture e limita bytes,
sequências, records, layers e queries antes das alocações correspondentes.
`motion_resource::Resource` continua sendo o único parser nativo de LMT.
`action_info` e `binding_action` fazem a ponte de endereços sintéticos;
`sampler_track` fornece as views de bytes. As referências sintéticas da
fixture são somente codecs 1/2 declarados, sem criar outro decoder comprimido.

```text
motion-query-resource-tests oracle.bin oracleSHA \
  54913 entry-54913.lmt sourceSHA54913 \
  92608 entry-92608.lmt sourceSHA92608
```

O estado nativo de uma consulta bem-sucedida alimenta a seguinte. O teste
reconstrói o modelo completo e compara seus bytes. Também compara a identidade
completa de `TrackReference`, inclusive bank, porque um endereço sintético
isolado não representa esse campo, e mantém a identidade do dono de key_data.
Outputs não selecionados devem conservar os bytes anteriores. A ordem
T→Q e layer crescente é confrontada com a ordem de canais do trace original.

Um negativo tardio usa uma consulta T comprimida cujo sucesso já demonstrou
avanço do cursor; somente a resolução Q seguinte é forçada a falhar. O teste
confirma a ordem T→Q, o erro com componente/identidade e a ausência de estado
parcial publicado. Há ainda desconhecimento de track/template e limite de
layers. As quatro consultas degeneradas verificam o erro de norma zero,
incluindo duas com **ambas as saídas ausentes**. Seus NaNs originais ficam
retidos; não entram na contagem de 4.998 resultados bit-exatos.

`artifact-validation-complete.json` registra seis fixtures deliberadamente
inválidas: digest, truncamento, contagem, tamanho de record, identidade de
fonte e alteração somente da saída esperada com digest de teste recalculado.
O último negativo verifica o comparador além da proteção por hash.

## Limites

São funções originais completas dentro de um perfil com arenas disjuntas e
propriedade explícita. Nomes, bancos, mapa, templates e estado anterior não
são inferidos como configuração real do jogo. A consulta recebe 0–4 layers
neste conjunto; limites extremos de contagem ARM não foram executados.

A consulta não aplica root motion, não avança clocks e não calcula pose dos
joints. Nenhuma regra de gameplay, FPS, scheduler, renderização ou execução
Windows é afirmada por este oráculo. Misturas nativas exigem operands e
intermediários finitos, arredondamento nearest-even e gradual underflow;
cópias raw preservam os valores não consumidos aritmeticamente. Efeitos de
callbacks de resolução e flags FPSCR não são transações da API.
