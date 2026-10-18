# Consulta de áreas de interação do Player

A busca `5b8f14 → 5ba61c` está reescrita em C++ nativo. Percorre a lista
original de registros, aplica os filtros de estado, máscara, área e posição,
e escolhe o resultado pela tabela de prioridades do executável. Empates
favorecem o último registro elegível na lista. A busca não recebe um resultado
pronto nem usa a malha SBC como substituta dessa coleção.

O volume usa um intervalo vertical com limite inferior inclusivo e superior
exclusivo, e distância radial inclusiva no plano XZ. As operações seguem
binary32 com arredondamento ao mais próximo e produtos separados das somas.
NaNs, infinitos, overflow e subnormais conservam as decisões observadas no
ARM; não são todos tratados como entradas inválidas. Flags de exceção, traps,
aliasing entre saídas e registros, e mutação concorrente ficam fora do perfil.

O construtor e o alocador originais estabelecem 128 posições nesta coleção.
O código nativo representa os links por índices e distingue link desconhecido
de lista vazia. Ciclos e índices fora dessa coleção produzem erros explícitos;
o código original da busca não tinha essas proteções.

No acerto, a busca copia os vetores e o status sem interpretar novamente os
bits. A referência ao recurso mantém identidade e deslocamento no buffer
carregado; endereços ARM preservados nas provas não viram ponteiros do PC.
Na ausência de resultado, o wrapper não publica os vetores. O chamador
limpa o campo canônico PW5a8, conservando a distinção entre os dois contratos.
O byte de área PBW9 é lido pelo chamador mesmo quando a lista está vazia.

`Player::prepare_update(context)` agora compõe o trecho inicial de `b1407c`
até a instrução anterior a `b14104`: condição de controle local, filtro,
acumulador, início de atualização e consulta. Todos os campos candidatos
são publicados juntos após renderização bem-sucedida. Uma falha conserva
o estado anterior. O relógio, a pose e o contador de passos não avançam.
As operações individuais continuam disponíveis, com suas transações separadas.
O restante de `b1407c` e o scheduler ainda não estão implementados.

A coleção é produzida a partir de `rItemPopSet` e `rItemPopList`. Os arquivos
reais selecionados de `loc/data/m15.arc` têm os identificadores 86102 e 86103.
São fontes verificadas, sem ligação comprovada à escolha da variante de uma
cena ou à malha SBC. A construção inicial deixa os registros inativos.
Ativação, atualização e ordem de execução precisam ser reconstruídas; não
há marcação automática de prontidão no motor nativo.

O leitor nativo preserva os segmentos carregados pelos formatos IPS/IPL 2003,
os bytes do arquivo, os deslocamentos e as tabelas de posições e escolhas.
Lacunas não carregadas não viram zeros conhecidos. A primeira etapa possuía
uma projeção `build_type1`; ela foi substituída pela
[construção completa no perfil de coleção nova](item-pop-builder.md), que
inclui as entradas secundárias, remapeamento e seleção ponderada. Os números
e o manifesto abaixo preservam a validação histórica da etapa inicial.

As evidências e os relatórios desta etapa ficam em
`research/private/player-world-lookup-v1/`. A execução isolada do ARM é uma
referência de análise e não faz parte do motor entregue. O launcher publicado
permanece 0.17.0, aberto durante a validação; esta etapa ainda não fornece
uma partida ou coop jogável.

A busca foi comparada em **326 chamadas ARM completas**, incluindo listas com
128 registros, prioridades, empates, limites geométricos e valores IEEE
especiais: 189 acertos, 834 visitas e 90.315 verificações por perfil normal e
sanitizado. Houve 76 testes negativos de processo. Os 6.800 bytes produzidos
pela busca são separados dos bytes opacos preservados nos buffers de teste.

O prefixo foi comparado em **54 execuções originais até b14104**, com 216
checkpoints, 30 acertos e 64.570 verificações por perfil. Todos os helpers
dentro desse trecho executaram seu código original, sem retornos substitutos.
A parada de observação deixa o frame do chamador aberto: não é um retorno
completo de b1407c. Os 8.788 bytes de gravações observadas são distinguidos
nos relatórios dos 19.464.192 bytes comparados de arenas, que também contêm
regiões preservadas. Foram 62 testes negativos de processo.

A integração no Player persistente passou em 136.445 verificações por perfil,
com oito negativos. Verificou preservação da pose e do relógio, referências
copiadas, limpeza de PW5a8 em miss, falhas nos quatro estágios sem publicação
e retry após falha tardia. Os sanitizadores instrumentaram 57 unidades do
projeto; SDL, bibliotecas do sistema e verificação de vazamentos ficaram fora
dessa cobertura. A falha de render foi exercitada pela operação individual
de filtro; os erros do prefixo combinado foram exercitados antes do render.

O leitor IPS/IPL passou em 8.030 verificações por perfil, 35 fronteiras e
16 negativos de processo. Comparou os 4.336 bytes carregados da dupla real,
26 registros primários, 58 posições, 40 escolhas e 32 descritores IPL. Essa
prova vem dos bytes e contratos dos loaders: não houve execução dos loaders
ou do construtor inteiro no ARM. O manifesto próprio está em
`research/private/item-pop-resources-v1/manifest.json`.

<!-- project validation -->
A suíte completa passou em **120/120 CTests no Mac**, em 43,09 s. O build
incremental final com testes levou 46,51 s; a recompilação Windows, 3,44 s.
Ambos terminaram sem avisos após remover uma dependência direta redundante
na ligação do teste do prefixo. Os primeiros builds e o aviso foram preservados.
Os tempos finais são de uma repetição com cache e não medem o desempenho do
jogo. Há CPU dos filhos nos relatórios; pico de memória da família de processos
e custo de tokens não foram medidos. O binário Windows não foi executado.

O manifesto consolidado é
`research/private/player-world-lookup-v1/manifest.json`. A sessão REA
1792319274646 foi exportada e fechada, com 76 Evidence; a auditoria confirmou
a remoção dos processos e do workspace temporário pertencentes à sessão.
