# Consulta espacial original sobre SBC e grade GRCO

Este recorte completa **223 consultas ARM originais**, com **122 hits e 101 misses**, sobre o SBC real `v00a01-core2`: 211 entradas em `1774e8` e 12 wrappers completos `2bb178`. A grade do recurso escolhe os candidatos, o callback original percorre faces e a rotina original retorna a classificação/altura. Não há enumeração de candidatos injetada pelo host, substituição de retorno de helper ou reescrita de PC.

A prova é de funções isoladas em contexto explícito, incluindo construtores reais dos componentes e inserção real em um container prealocado. **Não é execução do carregador de uma área, da inicialização inteira do manager ou do ID2 do Player sem cache.** Esta distinção permanece mesmo com os resultados positivos.

Artefatos: `research/private/player-spatial-context-v4/`. Fonte ELF SHA256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`; recurso SBC SHA256 `3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07`. O recurso tem 7.960 bytes, é a entrada93738 de `/loc/data/v00a01.arc`, membro `stage\v00\v00a01\collision\v00a01-a-g00`. Não foram alterados ROM, ELF, runtime, parser público, launcher ou entregas anteriores.

## Representação realmente consumida

O dono existente `collision_probe.py` fornece core: um grupo, 86 pares, 86 faces, 48 vértices e dois atributos. Os grupos começam no offset72, pares144, faces1004, vértices3756, atributos4524. O header distingue árvore global tipo0 de árvore por grupo tipo2. **A aceleração por grupo deste recurso é uma grade GRCO, não uma octree.**

O leitor original `db1454` e o loader `16014c` delimitam a extensão após o core:

| Offset de arquivo | Representação observada |
|---:|---|
|4588|Header GRCO de46 bytes: magic `6f637267`, versão2009111700, oito floats de caixa, `u16 nx`, `u16 nz`, `u8 index_kind`, `u8 compressed`|
|4634|30 células de8 bytes: `u16 count`, `u16 unknown`, `u32 start`|
|4874|`u32 index_count=249`|
|4878|249 índices `u16`, de0 a85; `index_kind=1`, `compressed=1`|
|5376|86 bounds serializados de28 bytes: XYZ mínimo, quatro bytes preservados, XYZ máximo|
|7784|176 bytes da árvore global tipo0, não interpretados nem consumidos no atalho de grupo único|

A grade mede5×6; a célula linear é `iz*nx+ix`. `start/count` selecionam um trecho do array de índices. Os índices são locais ao grupo; o consumidor `194388` soma as bases do grupo para resolver os registros. O `unknown_u16` das células vale zero nesta amostra, mas não foi removido nem recebeu semântica inventada. O parser investigativo `grid_resource.py` é fixado ao SHA desse SBC; não é um parser genérico de todas as variantes.

O loader lê, em ordem, cada árvore de grupo e seus bounds, e somente depois a árvore global. O count de bounds **é `u16(group+30)`**: `160490` executa `LDRH`, mesmo que o core preserve uma palavra maior nesse campo. Cada registro runtime tem stride40; as leituras `160518/160540/160550` consomem12+4+12 bytes. O loader zera os campos runtime `+1c/+20`; `+24` não é escrito por esse trecho. A confirmação LLVM está em `1603c0-loader-group-llvm.asm`; esta observação estática não amplia a execução para múltiplos grupos.

O construtor `349fa0`, o setup `345b9c` e o cleanup `349e6c` são executados completos. Com os ponteiros inicialmente nulos e `compressed=1`, não há allocator/free nesse caminho. Setup calcula em binary32 `cellX=(maxX-minX)/nx`, `cellZ=(maxZ-minZ)/nz`, seus recíprocos, e preserva a origem mínima. A consulta `346a5c` testa os limites XZ e converte `(coordinate-min)*reciprocal` por truncamento para índice inteiro, estreitado a16 bits com sinal. A caixa e os campos exatos resultantes estão em `arm-first/grid.json` e no trace.

O harness instala as relocações de buffers de células/índices/bounds conforme o leitor original. **Leitura de stream, alocação e loader inteiro não foram executados.** Tipo de índice0, grade não comprimida, árvore global e grupos múltiplos permanecem fora deste ensaio. Um inventário posterior de outros recursos pertence ao dono nativo que o produzir; não é contado como execução desta entrega.

## Associação do objeto e contexto declarado

A produção de `object+54` está fechada estaticamente: `19628c(object,resource)` libera o recurso anterior quando presente, escreve o recebido, adquire a referência e chama `196078` com `u16(resource+78)`. O caminho nulo chama `196078(object,0)` e pode remover o objeto dos containers. O único caller direto retornado pela consulta REA delimitada foi `170158`; isso não estabelece todos os callers indiretos ou o scheduler.

`196078` associa `object+58/+5c` ao armazenamento/quantidade de registros de grupo de64 bytes. Para cada registro, escreve `+14=ffffffff`, `+10=0`, `+c=1`, `+4=groupIndex`, `+8=objectOwner`; pode instalar uma transformação identidade se `object+60!=0`. Alocação, refcount, registro do objeto e lifetime completo **não foram executados**. Pseudocódigos063/067 e readbacks065/068 retêm essa derivação.

O ensaio reutiliza os quatro construtores integrais de componentes do manager da v3 (225 containers por preparação), depois executa `3e6c88` para inserir uma instância no grupo3. Capacidade, pool e armazenamento são entradas explícitas. Os demais containers ficam vazios porque seus construtores os produziram assim, não por um fallback de consulta.

A instância fornecida usa `object.byte5=1`, máscaras `+10=1/+14=8`, `+1c=0`, `byte33=0`, `+54=resource`, `+58=status`, `+5c=1`. O status tem `byte+c=1`, bytes`+1f/+20=0` e ponteiros`+24/+30=0`: é o perfil sem transformação. A vtable do manager é a original `e5b82c`; a associação do singleton e as máscaras `M+38=02000000`, `M+3c=M+40=0` são explícitas. Templates e palavra de ensaio também são explícitos, sem inferir BSS ou defaults do jogo.

## Caminho executado e contrato de retorno

```text
1774e8 → 1924b4 → 180c5c → 188850
 → 3eb178 → 3e73f0 → 18dc48 → 15af48
 → 1763c8 → 17e754 → 15c358
 → 349f4c → 346010 → 346a5c → 348780 → 348504
 → 183990 → 18a9f4 → 194388 → 17fec8
 → cópia16c570/171990 → classificação180c5c → retorno1774e8
```

`1924b4` instala os callbacks originais da tabela `e3b234`, incluindo `17fec8`. O filtro de caixa original é `cc93dc`. Na grade, `348780` visita **todos** os índices da célula, acumula o resultado e usa `348504`; não retorna simplesmente o primeiro candidato. `18a9f4` controla a repetição por query, aplica o filtro original `175318` e chama `194388` para resolver par/face/vértices/atributo.

A folha **`17fec8`** calcula inline o teste de orientação, interseção vertical com o plano e desigualdades baricêntricas em XZ; seleciona a distância mais próxima. Ela é diferente de **`cc94f0/1eb400/bdcf50`**, a folha executada por enumeração controlada na v2 e recuperada pelo módulo nativo anterior. Esta entrega não afirma que as duas entradas sejam substituíveis.

No carrier retornado, `+10/+14` identificam par/face, `+38/+3c/+40` apontam face/base de vértices/atributo, `+60..6c` contém o plano, `+70..7c` o ponto, `+80..8c` a direção vertical oposta e `+90` o parâmetro de distância. No perfil declarado, hits classificam como **129**, misses como0. **129 não é uma classificação padrão do jogo**: depende das máscaras explícitas. A máscara de filtro2 rejeita os atributos reais; `02000000` e `3fffffff` permitem a amostra.

As12 chamadas de `2bb178` executam o wrapper inteiro com flags2, anterior nulo, grupo3, máscara explícita e saída opcional de atributo nula. Retornam o byte de classificação e altura em`s0`; no miss, o próprio código devolve o Y de entrada. Flags/caminhos diferentes não são generalizados.

## Bytes opacos e leitura morta

O controle de inicialização é limitado por regiões, código/literais e bytes efetivamente escritos. A stack é invalidada e envenenada entre chamadas completas. Há duas exceções explícitas, verificadas no trace, em vez de uma afirmação falsa de que todos os bytes lidos foram inicializados:

1. O construtor `1987ec` deixa lanes do carrier sem escrever. Os loads em `16c5a8/b0/b8/c0/c8`, `180420` e `1719c8/d0/d8/e0/e8/1a18` transferem esses bytes diretamente para stores. O harness conserva o poison e propaga a marca de desconhecido ao destino. São22 bytes opacos no output: `+20..31` e `+44..47`. Eles não podem entrar em aritmética, controle ou formação de endereço; qualquer consumo fora dessas cópias falha no guard. Plano, ponto, distância e decisão permanecem definidos.
2. `18dc64` carrega `filter+10` antes de testar `object+1c`. No wrapper esse campo pode estar sem escrita. A admissão só ocorre com `object+1c==0`: `18dc8c→18dc98` pula a única comparação que usaria o valor (`18dc90`). O valor real envenenado é carregado e descartado, sem substituição. O negativo com `object+1c=1` e filtro desconhecido falha no próprio guard. Todas as12 leituras admitidas têm o branch de descarte corroborado pelo trace.

Poison00/A5/FF preservou decisão, altura e candidatos definidos. Os22 bytes opacos continuam opacos, mesmo quando a memória envenenada tem valor zero.

## Validação, custo e reprodução

O corpus inclui centroides das86 faces com origem acima/abaixo (172 casos),30 centros de células, variações de poison/máscaras, quatro posições exteriores e12 wrappers. O teste independente resolve baricêntricas em binary64 sobre as faces somente **para conferir os resultados**; não injeta candidatos ou alturas no ARM. O maior desvio de altura foi **0,00023211370210418636**, sob tolerância declarada0,002.

Seis negativos rejeitam ausência de buffers de células/índices/vértices, ponteiro de recurso, código da folha e o uso do filtro desconhecido. A auditoria posterior `audit_results.py` passou **5.414 verificações**, incluindo ordem exata dos candidatos versus as células, ausência de escrita no SBC, caminho original, cópias opacas, branch morto e invariância sob poison. Não reexecuta ARM.

O run autoritativo `arm-first` executou2.591.410 instruções, sendo944.109 nas consultas finais; levou12,111984s de wall time,13,218135s de CPU e pico780.009.472 bytes no processo Python. Unicorn2.1.4 executa funções isoladas, não um runtime entregue. `source-facts.json` confirma34.966 bytes mapeados contra72 readbacks REA completos e o ELF. A primeira consulta REA após import levou142,798s; CPU/memória da família REA e tokens do agente não foram medidos. Não há comparação de desempenho com o jogo.

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-spatial-context-v4/verify_grid.py --output /tmp/mh4u-spatial-grid-new
research/private/root-motion-runtime/venv/bin/python -B research/private/player-spatial-context-v4/audit_results.py
```

Use destino novo para execução; a pasta de saída deve não existir. Os protótipos `trial01..13` e três falhas de reconhecimento de função pelo Ghidra ficam retidos em `failure-notes.json`. As lacunas foram resolvidas fornecendo corpos/literais/contexto e modelando cópias opacas comprovadas; nenhum byte de código original foi corrigido.

A sessão `1792310616701` foi exportada em078 e fechada em079, com73 Evidence. O bridge terminou com código0; os três processos próprios estavam ausentes na conferência posterior. `lifecycle.json`, `rea-index.json` e `manifest.json` fixam limpeza, proveniência e dependências. Os readbacks071–075/pseudocódigos076–077 atendem à pesquisa paralela PBW/modelo; não contam como execução desta consulta.

A próxima fronteira é integrar a folha17fec8 e a grade ao dono nativo, conservando suas diferenças, e provar a população/contexto do cenário e o percurso completo do Player. Permanecem desconhecidos neste perfil: dinâmica/transformação, múltiplos grupos e árvore global, semântica física dos materiais, máscaras originais de cada caller, carregamento completo, cache do Player e scheduler. O retorno positivo prova a consulta original no recurso real sob as entradas declaradas, sem promover essas lacunas a defaults.
