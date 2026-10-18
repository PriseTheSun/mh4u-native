# Consulta espacial: containers construídos e retorno original

Este recorte atravessa a fronteira anterior `1774e8 → 1924b4 → 180c5c → 188850` com código ARM original. Há **15 consultas completas em componentes de manager vazios construídos pelo próprio código, 18 consultas completas em listas não vazias e 48 chamadas completas do wrapper `2bb178`**. Todas retornaram sem colisão nos estados declarados. Quatro outros ensaios alcançaram o callback `18dc48` e pararam por memória executável não fornecida: não são hits nem misses.

A inicialização dos componentes do manager está demonstrada. A população desses componentes pelo cenário carregado não está. Nenhum resultado desta entrega autoriza transformar “contexto desconhecido” em “sem colisão”, nem habilitar o caminho sem cache do Player usando listas vazias inventadas.

Os artefatos estão em `research/private/player-spatial-context-v3/`. O ELF fixado permanece `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Nenhum módulo nativo, launcher, parser ou entrega anterior foi alterado.

## Produção observada dos containers

O prefixo de `19c944`, corroborado pelo pseudocódigo completo, chama os seguintes construtores sobre o mesmo objeto `M`. Esse prefixo foi analisado estaticamente; o construtor inteiro contém outras inicializações, alocações e registro no manager e **não foi executado** neste ensaio.

| Componente | Entrada original | Containers consultados depois | Estado produzido e consumido |
|---|---:|---|---|
| `M+78` | `170f04` | Cinco arrays de 32 containers, bases `M+98`, `+a98`, `+1498`, `+1e98`, `+2898`, stride `50` | Base `19c3b0` escreve `M+7c=0`; cada contador ativo `container+2c=0` |
| `M+3298` | `19c184` | Dois arrays de 32, bases `M+32f8` e `+3cf8`, stride `50` | Byte `M+329c=1`; contadores ativos zero |
| `M+46f8` | `1721c4` | Um container em `M+4748` | Byte `M+46fc=1`; contador ativo zero |
| `M+4798` | `1744b8` | Lista adicional lida por `183be0 → cd0068` | Contador `M+47a0=0` |

Números de offsets e endereços nesta tabela são hexadecimais; contagens 32 são decimais. Os quatro construtores foram executados completos, com seus helpers reais, incluindo o array constructor Thumb `1001f4`, `3eb1cc → 157aac`, e a inicialização com `LDREX/STREX` em `10bd74 → 10bd3c`. O trace confirma **225 entradas reais em `3eb1cc` por ensaio**. Nenhuma chamada de allocator foi interceptada ou substituída.

`3eb1cc` inicializa as duas listas embutidas. No container, `+2c/+30/+38` são quantidade/capacidade/array de ponteiros ativos; `+40/+44/+4c` são os correspondentes da lista reutilizável. Os bytes`+34/+48` são escritos como zero. Alguns outros bytes permanecem sem escrita, inclusive componentes não consumidos do volume; o harness mantém poison e rejeita leituras não inicializadas. Não há zero-fill global de `M`.

REA nova: sessão `1792309425902`, pseudocódigos 012–015, readbacks 017–045. A cadeia `19c944` está no readback 025/pseudocódigo 013. O corpo completo de `19c184` está no 038. Todos os bytes mapeados para os ensaios têm cobertura REA comparada ao ELF em `source-facts.json`.

## Inserção real, com armazenamento explícito

O método `3e6c88(container,bbox,object)` é executado inteiro no perfil com elementos disponíveis na lista reutilizável e capacidade suficiente na lista ativa. O armazenamento inicial é um **pool de ensaio explícito**; não se afirma que o allocator ou o carregador do cenário o produziram.

A rotina consome o último ponteiro reutilizável, copia XYZ mínimo para `node+10/+14/+18`, XYZ máximo para `+20/+24/+28`, escreve zero em `+4/+c/+1c/+2c`, guarda a identidade recebida em `+8`, reduz `container+40`, acrescenta o nó a `*(container+38)` e incrementa `container+2c`. `node+0` permanece não escrito nesse ramo. O array ativo e os nós retornados são produzidos pela execução original, não preenchidos pelo harness como resultado esperado.

Os ensaios usam um ou três nós na família `M+98`, grupo 3. Os valores de caixa vêm do grupo 0 do SBC `v00a01-core2`, com o SHA256 verificado:

`3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07`

A associação desses valores aos nós é declarada pelo ensaio. Isso não prova a ligação original `rCollision → objeto → container`. O parser existente valida o SBC; nenhuma árvore ou face é sintetizada como colisão.

## Busca e fronteira seguinte

O filtro é construído por `198418` com os mesmos argumentos instalados em `2bb178`, incluindo seus oito argumentos de stack. `198154` aplica tipo/máscara/grupo e `171860` constrói o carrier. `1774e8` constrói o contexto, `1924b4` instala a tabela de callbacks original e `180c5c` cria a forma tipo 21 antes de chamar `188850`.

Nesta rota, o segundo argumento de `188850` é nulo. Portanto, ela não exige o AABB parcialmente escrito apontado como lacuna na rota diferente de `188d10` na entrega v2. A lacuna anterior não foi “corrigida” por inicializar esses bytes arbitrariamente; esta é outra entrada original.

O caminho para a primeira família é:

```text
1774e8 → 1924b4 → 180c5c → 188850
       → 3eb178 → 3e73f0 → 18dc48 → 15af48…
```

Com `param5=-1`, o ramo tipo 21 de `3e73f0` passa por `3e8fc4`. O trecho `3e9144..3e9180` testa apenas `query.x >= node.minX` e `query.z <= node.maxZ`. **Não é um teste de contenção AABB completo.** `x<minX` ou `z>maxZ` rejeitam o nó; pontos em `x>maxX` ou `z<minZ` ainda podem chamar o callback. O contador de visitas do trace prova que todos os três nós foram considerados nos casos de rejeição; não há atalho de “primeiro resultado”.

Quatro probes verificam a fronteira em pontos: mínimoX/máximoZ, além de máximoX, abaixo de mínimoZ e centro. Todos executam a chamada indireta real em `3e91b0` e falham no fetch de `18dc48`. O callback não está mapeado no perfil executável porque a representação do objeto/árvore não foi fornecida. Não há substituição de retorno, reescrita de PC, callback Python ou resultado de interseção vindo do host.

A análise estática de `18dc48` (REA 047/048) delimita o próximo contrato. Ele lê o filtro em `C+d8` e exige:

- `object.byte5 != 0`;
- identidade em `object+1c` compatível com a exclusão do filtro, e objeto diferente da exclusão opcional em `filter+44` quando `filter+40==1`;
- interseção entre `object+10` e `filter+4`;
- interseção entre `object+14` e `filter+8`;
- ponteiro `object+54` não nulo, então chama `15af48` com a forma original e callback `1763c8`.

Não se atribuem nomes de gameplay a esses campos. Em particular, `filter+10` não é inicializado pelo construtor usado; quando `object+1c` não é zero, sua origem precisa ser provada ou passada explicitamente, não presumida. A retenção v2 já segue estaticamente `15af48 → 1923f4 → 15d720 → 17e9a0/17e754`; a enumeração e a árvore reais ainda precisam ser ligadas ao objeto. A execução v2 de triângulos enumerados manualmente continua separada desta busca.

## Retornos completos e controles

| Ensaio autoritativo | Cobertura | Resultado |
|---|---|---|
| `empty-complete` | 15 consultas; poison 00/A5/FF; cinco máscaras de área, incluindo bit31 e todos os bits | Retorno0; manager preservado; três controles negativos rejeitados |
| `nonempty-complete` | 18 consultas; 1/3 nós; três posições de rejeição; poison 00/A5/FF | Retorno0; todos os nós visitados; dois controles negativos rejeitados; quatro fronteiras guardadas |
| `wrapper-complete` | 48 chamadas completas `2bb178`; poison 00/A5/FF; flags 0/2/4/8; anterior ausente ou três valores de Y | Consulta original 0, cleanup `17193c`, byte de classificação 0 e retorno float igual aoY de entrada 100 |

Os 81 ensaios completos executaram 729.078 instruções originais, incluindo a construção. A auditoria independente dos arquivos salvos passou 1.034 verificações, sem reexecutar ARM nem calcular expectativas pelo código da consulta. As arenas são limitadas, somente bytes escritos/fornecidos podem ser lidos, e stack é invalidada e envenenada entre chamadas completas. Código/literais são cópias exatas do ELF; templates globais são entradas explícitas. O vetor`10e6370=(0,0,0)` e palavra`104df34=13579bdf` não são apresentados como defaults do jogo.

As falhas iniciais estão preservadas: argumento de stack do filtro não fornecido, duas lacunas de literais na declaração do harness e endereço de observação incorreto do loop. Elas foram corrigidas na representação do ensaio; nenhum byte original ou resultado original foi corrigido. `failure-notes.json` distingue esses protótipos dos três resultados autoritativos.

Unicorn 2.1.4 executa essas funções isoladamente em ARM11/VFP, não o jogo inteiro nem um runtime incluído no executável entregue. Os runs autoritativos levaram aproximadamente 0,65/0,69/1,35s de wall time; CPU 0,72/0,76/1,48s; picos do processo Python 109/129/299MB decimais. A primeira consulta REA após importação levou 152,34s. Tempo/CPU/memória da família REA completa e tokens do agente não foram medidos. Não há comparação de desempenho com a execução do jogo.

## Reexecução e limites

Use destinos novos; os scripts recusam sobrescrever pastas existentes:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-spatial-context-v3/verify_query.py --output /tmp/mh4u-spatial-empty-new
research/private/root-motion-runtime/venv/bin/python -B research/private/player-spatial-context-v3/verify_nonempty.py --output /tmp/mh4u-spatial-list-new
research/private/root-motion-runtime/venv/bin/python -B research/private/player-spatial-context-v3/verify_wrapper.py --output /tmp/mh4u-spatial-wrapper-new
```

A próxima prova necessária é a produção de `object+54` e sua ligação ao rCollision/árvores efetivamente inseridos na família consultada. Esta entrega não demonstra carregamento de uma área, colisão positiva, altura do terreno, atualização do cache do Player, retorno completo de ID2 sem cache, material físico, walkability, física ou scheduler. O resultado negativo aqui é restrito aos componentes vazios construídos ou aos nós explicitamente rejeitados pelo código.

A sessão REA foi exportada em 051 e fechada em 052, com 49 Evidence. `lifecycle.json` registra a saída do bridge e ausência dos processos próprios. `manifest.json` fixa fontes, dependências, documentos, readbacks, scripts e relatórios.
