# Preparação e seleção de ações do jogador

`Player::prepare_to_admission` reúne o prefixo de atualização e a seleção
seguinte em uma operação nativa. O limite observado é a instrução anterior a
`b142d0`, dentro de `b1407c`. A função completa de atualização e o scheduler
continuam pendentes.

A operação recebe os mesmos fatos explícitos do prefixo, a ligação conhecida
da chamada virtual, a coleção de objetos e os recursos consultados pelo
original. O filtro, a consulta do mundo, a busca de objetos, a política de
ações e as transições usam os donos já existentes. O delta e o contexto AD24
têm uma única origem por chamada. As constantes pertencem ao Player.

Os 32 slots da coleção apontam para identidades de objetos. Cada objeto aponta
para uma identidade de dados. Assim, dois slots podem referir o mesmo objeto,
e objetos distintos podem compartilhar os mesmos dados. Essa distinção afeta
o comportamento: a consulta `9368c4` pode marcar D349 e retornar falso. A marca
deve permanecer visível para a consulta seguinte do mesmo dado.

`68da18` procura o primeiro objeto que atende aos filtros e ao teste de
distância. O raio vem da consulta real à tabela de recursos, incluindo o
possível override; a API não aceita um raio pronto no lugar dessa consulta.
Os índices e as referências desconhecidas ou ausentes só são examinados
quando o caminho original os consome. Campos desconhecidos não viram zeros.

As comparações mantêm os detalhes do executável: PW224 e PW238 são inteiros
de 16 bits com sinal, PW31c é uma palavra completa e o teste de PW324 deixa
valores NaN seguirem para a busca. O teste final de distância rejeita um
resultado não ordenado. Multiplicações e somas seguem o perfil binary32
separado do VFP2, sem contração em uma operação fundida.

A coleção candidata conserva alterações mesmo quando uma busca termina sem
selecionar objeto. Um erro nativo, por sua vez, não publica nenhuma alteração.
O Player prepara a saída e renderiza antes de publicar seu estado e a coleção
externa. As operações finais de publicação não lançam exceções. Não há avanço
automático do relógio da animação.

Antes de copiar as camadas, a API verifica o orçamento de armazenamento pelo
mesmo validador dos comandos de animação: oito camadas e 2.048 vínculos de
ossos no total por padrão. O chamador pode fornecer outro orçamento explícito.
Esses valores são limites de trabalho do host; não são máximos do formato da
ROM. Excesso de orçamento ou falha de alocação retorna erro de capacidade.

O perfil de `b17f78` cobre retornos sem ação e a transição direta `(6,19,12)`,
além da política executada quando ele retorna zero. Os ramos de PW490/PW9a8,
a consulta adicional condicionada a estado/ação e os ramos que precisam de
`b22748` ainda não foram integrados. Quando alcançados, retornam um erro
tipado. A seleção positiva de `68da18` executa a limpeza de alvo e a transição
`(2,25,0)`, usando a identidade do objeto escolhido.

As entradas de validação são estados de diagnóstico explícitos. Elas não
comprovam a construção inicial desses objetos durante uma partida. A
projeção nativa também não reproduz acessos de memória sem efeito semântico:
PW1ea e D797e são carregados pelo ARM nos caminhos observados, mas seus valores
não são usados pelo perfil integrado. Não há execução de instruções ARM no
produto; o executável original é usado apenas na pesquisa comparativa.

As evidências, comparações e limites desta etapa estão em
`research/private/player-update-admission-v1/`. O laboratório publicado
permanece aberto durante os testes. Este componente ainda não constitui
combate, missões, saves, tradução completa ou coop jogável pela Internet.

## Validação

A referência executou **115 casos** desde a entrada original `b1407c` até o
ponto anterior a `b142d0`: 94 chamadas da política e 25 transições. Todas as
rotinas auxiliares alcançadas usaram as instruções originais. O chamador ainda
tinha seu quadro de pilha aberto no ponto de parada; isso não é o retorno da
função completa. Os casos usam estados de diagnóstico, sem alegar um estado
inicial obtido pelo bootstrap do jogo.

A comparação conserva as 16 regiões de memória examinadas, as identidades e
os aliases dos objetos, além das 8.585 escritas observadas. A execução original
inclui 19 controles de dependências: 15 rejeições de acesso e quatro caminhos
que dispensam o dado omitido. As falhas de acesso da pesquisa não representam
um retorno normal do jogo.

O comparador nativo passou nos perfis normal e sanitizado, com os 115 casos,
24 controles de erro tipado e nove controles de consumo condicional dos dados.
Os 62 processos de entradas malformadas rejeitaram os arquivos sem diagnóstico
dos sanitizadores. Os corpora anteriores de início de atualização (96 casos)
e prefixo (54 casos) continuaram passando após a extração do leitor de campos
compartilhado. O comparador verifica resultados e efeitos selecionados; não
alega reproduzir cada acesso de memória do ARM. Os argumentos de `b66f0c` não
foram capturados e somente sua presença no traço é comparada.

O teste do Player persistente passou em modo normal e com ASan/UBSan. Exerceu
17 falhas de alocação até alcançar a primeira falha real de renderização nessa
rota, sempre seguida de nova tentativa no mesmo Player. A suíte pública passou
em **130/130 CTests no Mac**. A compilação Windows x64 concluiu sem avisos;
execução no Windows ainda não foi validada.

O perfil estrito usa ASan, UBSan e float-cast-overflow, sem recuperação.
Bibliotecas do sistema e SDL não foram instrumentadas; não há alegação de
verificação de vazamentos. O manifesto consolidado preserva fontes, binários,
relatórios, tentativas exploratórias e suas limitações em
`research/private/player-update-admission-v1/manifest.json`.
