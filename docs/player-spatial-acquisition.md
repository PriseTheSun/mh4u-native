# Aquisição de altura no Player nativo

O Player agora pode consultar a colisão quando sua altura ainda não está em
cache. `player_spatial_acquisition` implementa `b40b88` e compõe os filtros de
`2ba230` com a consulta de altura nativa. `player_state1` e o Player persistente
consomem o mesmo componente. O trecho anterior que comparava apenas a altura
em cache foi removido.

A referência executa 88 chamadas completas do ARM original, incluindo contato,
ausência de contato, filtros, comparação com valor unsigned e reutilização do
cache. São contextos de pesquisa explícitos, com recursos SBC inteiros da ROM.
O código original roda somente no ensaio de referência; o componente entregue
é C++, sem runtime de 3DS. São 75 consultas e 13 chamadas com cache; 66 contatos,
3 gravações de cache, 925 candidatos, 201 chamadas de face e 67 atualizações
de grade. O comparador passou 217.145 verificações por perfil normal e com
ASan/UBSan/float-cast-overflow, mais 46 controles de processo. Inclui 12 erros
tipados e 3 controles de campos não consumidos.

A preparação gira `(0,300,60)` usando o ângulo do caçador e soma esse vetor às
posições atual e anterior. A função de rotação `aacf0c` passou para uma operação
compartilhada no dono numérico já usado pelo avanço do Player. A tabela e a
interpretação dos coeficientes continuam únicas. A referência cobre 68.254
casos, incluindo todos os 65.536 ângulos de 16 bits do vetor desta consulta.
Os resultados não finitos ficam fora do perfil nativo finito e produzem erro
explícito.

O estado do Player produz a máscara de atributos. Uma máscara externa de áreas
e o controle local produzem as flags da consulta. Esses valores permanecem
separados e não recebem nomes de material ou significado de gameplay inferido.
Os campos adicionais de posição anterior e PW5a8 têm um único dono em
`player_fields`. A área e o ângulo conservam seus donos existentes.

Status e contato não são intercambiáveis: `2ba230` preserva o resultado da
consulta e `2bb178` grava seus oito bits baixos. Apenas status exatamente 1
grava a altura e incrementa o byte de cache. Status 129 compara a altura sem
publicar cache; status 0 retorna falso. Uma chamada que já encontra cache não
consulta o cenário nem incrementa novamente esse byte.

Na comparação comum, a altura precisa ser menor que Y atual mais o offset.
O parâmetro alternativo é um inteiro unsigned, convertido para float, e usa a
comparação oposta: maior que esse limite. A altura lida do cache deixa de ser
consumida nesse segundo caminho. O componente respeita esses campos mortos,
sem exigir dados de cenários ou posições anteriores no caminho com cache.

As alterações do cache e da grade retornam no mesmo resultado. O estado
original permanece intacto em qualquer erro. A integração persistente primeiro
conclui o handler e a renderização; então publica os campos do Player e devolve
as atualizações da grade ao dono do cenário. O chamador aplica essas
atualizações somente após o sucesso da operação completa. A consulta conserva
a propriedade do recurso SBC, sem guardar um ponteiro ao contexto emprestado.

O teste do Player usa um contato real do recurso 86059, compara a altura
observada no ARM, verifica os dois resultados de transição e repete a chamada
com cache sem fornecer cenário. Outro caso falha na transição após a consulta
bem-sucedida e verifica que custo, posição, cache e grade não foram publicados.
Essa falha é uma guarda nativa de um dispatch ainda não suportado, não uma
execução do corpo original desse dispatch.

O perfil de geometria mantém os limites da [consulta espacial](spatial-query-native.md):
instâncias diretas, um grupo, GRCO comprimido, pares de uma face e status sem
transformação. O carregamento completo do mundo, a inicialização e o scheduler
do jogo permanecem pendentes. O reset explícito do cache foi implementado
depois, na [rotina de início de atualização](player-update-begin.md). Esta etapa
não estabelece combate, missões, saves ou coop pela Internet. O launcher
publicado 0.17.0 permanece aberto; não houve publicação de pacote novo.

A integração passou nos 112 testes CTest do Mac em 47,03 s. O build completo
incremental, incluindo os testes, levou 61,20 s; a compilação cruzada para
Windows levou 16,82 s. Os builds rodaram em paralelo e não emitiram avisos do
compilador. Não houve execução no Windows. Os relatórios preservam CPU dos
processos filhos; pico de memória da família de processos não foi medido.
Os 252 casos ID2 anteriores também passaram após a extração da rotação e a
remoção da comparação de cache duplicada. Esses testes são diferentes das 88
novas chamadas originais de aquisição.

A análise do próximo passo localizou `b3b3a0`: ele copia a posição atual para
os campos anteriores e invalida o cache no início de `b1407c`, antes dos
possíveis dois dispatches de ações. A implementação posterior fica no
[componente dedicado](player-update-begin.md). Não se deve zerar o cache a cada dispatch isolado nem
inferir uma taxa de atualização a partir desse ponto de chamada.

Evidências: `research/private/player-spatial-acquisition-v1/manifest.json`,
`original-contract.md`, o corpus original e o comparador nativo dessa pasta;
rotação em `research/private/player-spatial-offset-native-v1/manifest.json`.
