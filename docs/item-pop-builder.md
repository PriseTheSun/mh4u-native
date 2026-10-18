# Construção dos registros de interação

`item_pop_builder::build_fresh` reimplementa a rotina completa `5ba7b4`,
incluindo `5b928c`, no perfil de uma coleção nova sem objetos auxiliares
anexados. Consome os recursos IPS/IPL do leitor nativo e os contextos explícitos
de categoria, estado aleatório, contador e vetores de reset. O resultado usa
os mesmos `Record` e `Manager` da consulta de áreas; não cria uma segunda
representação do estado. A projeção anterior `build_type1` foi removida.

Os 128 registros são reinicializados antes de consumir os recursos. Se um
recurso estiver ausente, retorna a coleção reinicializada na ordem original.
Referências primárias inexistentes não alocam; uma entrada secundária que
falha depois da alocação mantém a posição ocupada e fora da lista. As entradas
aceitas são inseridas no início, preservando os links anterior/próximo, o
contador e a ordem que a consulta usa para resolver empates.

A seleção secundária preserva o remapeamento original de categoria e o número
de avanços do RNG. Total de pesos zero consome um avanço; membro IPL ausente
após seleção consome dois. A comparação usa acumulado **maior ou igual** ao
resto: com resto zero, até a primeira escolha de peso zero pode ser selecionada.
O helper de RNG é compartilhado com a transição do jogador e o estado continua
no campo canônico `GlobalFields.field22`. Não há semente oculta ou relógio novo.

Os campos numéricos, IDs, links e referências pertencem aos registros e ao
gerenciador. A proveniência separada guarda apenas os índices, deslocamentos
e a identidade do recurso usado. Os buffers originais são imutáveis; nenhum
endereço ARM é convertido em ponteiro do PC. Categoria desconhecida, limites
de trabalho/memória e ordinal sem descritor após remapeamento geram erros
explícitos, sem publicar resultado parcial. Esse último caso não equivale a
um retorno nulo do original: seus getters não verificam esse limite.

O perfil não executa destruição de objetos auxiliares de uma coleção viva.
Vtables, ponteiros de objetos e padding não são estado do motor nativo. Todos
os campos numéricos e links escritos pelo reset estão representados uma só vez.
O estado inicial de prontidão permanece **zero**, conforme o original.

A referência ARM usa as rotinas originais completas, incluindo alocador,
reset, getters, remapeamento, RNG e divisão; não substitui chamadas por
retornos prontos. O par real 86102/86103 de `loc/data/m15.arc`, com semente 1
e modo 0 explícitos, produz 43 registros ligados: 23 primários e 20 secundários.
Os 40 avanços do RNG terminam na semente 32921. A execução ARM é uma ferramenta
de pesquisa; não faz parte do executável nativo entregue.

A auditoria encontrou a chamada indireta do gerenciador para a rotina que
atualiza a prontidão. Seu caminho mínimo foi delimitado em
`research/private/item-pop-builder-v2/readiness-frontier.md`, mas ainda não
foi implementado nem executado nesta etapa. A ligação à cena escolhida,
atualização dos objetos, sequência completa de carregamento, scheduler e
bootstrap continuam pendentes. O laboratório 0.17.0 permanece aberto;
esta biblioteca ainda não fornece combate, missões ou coop jogável.

Evidências e resultados estão em `research/private/item-pop-builder-v2/`.
