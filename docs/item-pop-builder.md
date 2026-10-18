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

A [atualização dos registros](item-pop-update.md) acrescenta agora a ativação
por essa chamada indireta, com relógio explícito e fatos do construtor de cada
registro. O marco original desta construção, congelado em v2, não executava
essa etapa; sua nota de prontidão permanece como evidência histórica.
A ligação à cena escolhida, a sequência completa de carregamento, scheduler
e bootstrap continuam pendentes. O laboratório 0.17.0 permanece aberto;
esta biblioteca ainda não fornece combate, missões ou coop jogável.

Evidências e resultados estão em `research/private/item-pop-builder-v2/`.


A comparação final passou em **33 execuções completas do construtor original**,
com 874 alocações, 734 registros ligados e 744 avanços de RNG. Os perfis normal
e sanitizado passaram em 2.672.486 verificações cada, incluindo a projeção de
532.686 bytes de campos nativos, preservação dos bytes opacos e proveniência
das 874 alocações. Esses totais são do corpus de diagnóstico, não de uma partida.
Foram oito erros tipados, duas verificações de retorno por recurso ausente e
78 negativos de processo. Os recursos conservaram seus bytes e fatos parseados.

O helper compartilhado foi comparado a todas as 65.536 sementes possíveis;
o remapeamento, a 2.048 casos originais. Os seis testes de helpers e consumidores passaram em
94.082 verificações por perfil e 16 negativos de processo. Os sanitizadores
usaram ASan, UBSan e float-cast-overflow, sem recuperação; vazamentos e
bibliotecas do sistema não fazem parte dessa cobertura. A auditoria independente
corrigiu a ordem de validação quando List está ausente e não deixou achados
materiais abertos.

<!-- project validation -->
A suíte pública passou em **122/122 CTests no Mac**. O build
incremental com testes levou 97.46 s; a compilação Windows x64,
37.60 s. Ambos concluíram sem avisos. O binário Windows não
foi executado. Esses tempos usam cache e não medem desempenho do jogo.
O manifesto consolidado é `research/private/item-pop-builder-v2/manifest.json`.
