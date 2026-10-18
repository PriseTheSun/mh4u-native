# Atualização dos registros de interação

`item_pop_update::apply` atualiza o mesmo `Manager` que o construtor produz e
que a consulta de áreas lê. Reimplementa a travessia de `5ba580` e os caminhos
de retorno dos estados 0 a 6 e valores a partir de 7 de `5aa4cc`, no
perfil sem objetos auxiliares, transformação anexada ou callback. O contexto
contém os campos explícitos do relógio e o gate de categoria originais.
Nenhum delta é inferido do ritmo do laboratório.

O estado 0 desconta o atraso e pode entrar no estado 1 na mesma chamada.
O estado 1 atualiza a intensidade e o tempo restante, e inicia a desativação
quando o tempo acaba ou a contagem real chega a zero. O estado 2 desconta
seu próprio tempo, calcula a intensidade de saída e escolhe o próximo estado
conforme o tipo e as flags. O estado 6 zera a intensidade. Valores de estado
a partir de 7 retornam após o prefixo comum, conforme o executável.

O estado 3 pode renovar um registro secundário usando os dois canais do
estado aleatório canônico. A seleção compartilha a implementação de `5b928c`
com o construtor. Se a seleção não encontrar um recurso, conserva os sorteios
já consumidos e passa ao estado 4; a remoção ocorre numa chamada posterior.
O estado 4 remove o registro da lista, preservando a ordem dos writes original.
Como a travessia lê o próximo link depois da remoção, essa passagem para ali.
O estado 5 percorre as entradas primárias e restaura as contagens do último
registro compatível, incluindo a igualdade entre zeros com sinais diferentes.

O singleton do gerenciador e seus recursos têm identidades explícitas:
desconhecido, nulo conhecido ou objeto fornecido. Um gerenciador diferente é
um limite informado, não uma ausência. Esperar pelo tempo restante não exige
esses recursos, e o sorteio que adia uma renovação não os consulta. Campos
consumidos continuam obrigatórios; nenhum seed, delta ou singleton é inferido.
Objetos auxiliares, transformação, consulta espacial e callbacks específicos
continuam fora do perfil admitido.

Cada registro guarda a identidade conhecida da rotina virtual e a presença
ou ausência dos dois objetos auxiliares. Esses fatos ficam desconhecidos
em um registro criado apenas para consulta. A construção nova publica os
fatos correspondentes ao construtor original `5ab458`; eles não são efeitos
do reset nem do construtor da lista. O novo ensaio ARM executa os 128
construtores de registros antes de construir a coleção.

O resultado é publicado por inteiro após uma atualização bem-sucedida.
Um erro não altera a coleção recebida. Essa atomicidade é uma escolha da
API nativa: a rotina original pode ter alterado registros anteriores antes
de encontrar um problema. A travessia lê o próximo link depois da chamada
de atualização e verifica índices inválidos e ciclos.

`binary32_arithmetic` é o dono dos resultados escalares de soma, subtração
e divisão. Preserva zeros com sinal, infinitos, subnormais e a prioridade,
sinal e payload dos NaNs observados no perfil FPSCR 0. Exige arredondamento
para o mais próximo com empate para par e underflow gradual. A comparação
da intensidade usa os bits como inteiro com sinal, conforme o original;
substituí-la por um clamp comum de float mudaria os resultados.

A referência aritmética contém 12.816 observações de instruções isoladas
do executável, executadas em Unicorn com operandos explícitos. Isso não é
execução da função inteira nem medição em um console. A referência da
atualização, separadamente, executa o gerenciador, sua chamada virtual real
e a consulta completa. Nenhum desses mecanismos de pesquisa é incluído no
executável nativo entregue.

`Player::update_item_pop` recebe o gerenciador externo por referência e usa o
estado global já pertencente ao Player. Prepara o resultado e renderiza antes
de publicar as duas alterações, com operações de publicação que não lançam
exceções. Um erro conserva ambos os donos. A referência externa mantém sua
identidade e não exige recriar o personagem. Animação, pose e relógio são
preservados. A consulta continua usando `prepare_update`.
Ainda não há um scheduler que determine automaticamente a ordem numa partida.

A associação desses recursos à cena, o bootstrap completo, a inicialização
do relógio e a máquina inteira de atualização continuam pendentes. Esse
componente não estabelece combate, missões, saves ou coop jogável. O launcher
publicado permanece no laboratório 0.17.0, aberto durante a validação.

As evidências locais ficam em `research/private/item-pop-lifecycle-v1/`,
`research/private/item-pop-update-v1/` e `research/private/binary32-arithmetic-v1/`.

<!-- previous milestone validation -->
Os números abaixo pertencem ao marco anterior de ativação/desativação.
A sequência original preservada contém **97 construções, 102 atualizações
completas e 375 consultas**, com 271 resultados encontrados. O primeiro caso
histórico declara a ligação virtual e ausência de auxiliares como premissas;
os outros 96 executam os 128 construtores de registros. O gerenciador
chama 4.300 vezes a rotina real dos registros. Foram preservados 14.156 stores
de atualização e 15 controles de caminhos não admitidos. Esses números são
do corpus de diagnóstico, não de uma partida.

A comparação nativa reproduziu os estados completos, os bytes preservados e
os resultados dessas consultas. A integração usa 43 pares de consultas nas
posições produzidas pelos recursos reais: o mesmo Player passa de nenhum
resultado ativo para um resultado encontrado após a atualização, preservando
seu estado aleatório, animação e pose. O estado inicial do Player continua
explícito de diagnóstico.

A suíte pública passou em **125/125 CTests no Mac**, sem avisos. Depois, o teste
do Player recebeu 258 verificações adicionais de preservação interna e foi
recompilado e executado novamente com sucesso. O runtime, os helpers e os
demais testes mantiveram exatamente os mesmos hashes; o alvo Windows desse
teste também foi recompilado. O build
incremental com testes levou 67.76 s; a compilação Windows x64,
21.21 s, antes desse reforço isolado. O binário Windows não foi executado. Os tempos usam
cache e não medem desempenho do jogo. Perfis normal e sanitizado, controles
negativos e a regressão dos testes extraídos estão nos relatórios do manifesto
consolidado `research/private/item-pop-update-v1/manifest.json`.

<!-- lifecycle validation -->
O marco de renovação/remoção acrescenta **57 casos de referência**, com
100 atualizações completas, 2.276 chamadas de registros e 236 consultas,
182 delas com resultado. Executa os construtores e as rotinas originais com
recursos reais e entradas sintéticas identificadas. As 14 chamadas de remoção
incluem 12 remoções ativas e dois retornos sem remoção. Foram observados
25 sorteios do canal0, 13 do canal1, 14 renovações primárias e dez tentativas
secundárias, cinco aceitas e cinco sem recurso selecionado.

A comparação nativa passou nos perfis normal e sanitizado, preservando os
corpora anteriores de 97 casos de atualização e 33 de construção. O leitor
verifica todos os novos campos tipados de posição e escolha contra os bytes
originais. Seis Players persistentes exercitam 12 atualizações e 12 consultas,
com controles de rollback para dependências desconhecidas, gerenciador
incorreto, falha após os sorteios e falha de renderização.

A suíte pública passou em **127/127 CTests no Mac** e a compilação Windows x64
concluiu sem avisos. O Windows não foi executado. O perfil sanitizado inclui
ASan, UBSan e float-cast-overflow, sem recuperação; bibliotecas de sistema e
SDL não foram instrumentadas e o verificador de vazamentos ficou desativado.
Os relatórios, hashes, snapshots e limitações estão no manifesto consolidado
`research/private/item-pop-lifecycle-v1/manifest.json`.

A implementação cobre os caminhos admitidos do perfil, incluindo agora os
estados3/4/5. Os tipos que exigem spawn, consulta espacial e callbacks ainda
não reconstruídos continuam retornando um limite explícito. Isso não é uma
sessão jogável nem uma validação do ciclo completo do jogo.
