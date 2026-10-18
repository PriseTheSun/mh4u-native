# Atualização dos registros de interação

`item_pop_update::apply` atualiza o mesmo `Manager` que o construtor produz e
que a consulta de áreas lê. Reimplementa a travessia de `5ba580` e os caminhos
de retorno dos estados 0, 1, 2, 6 e valores a partir de 7 de `5aa4cc`, no
perfil sem objetos auxiliares, transformação anexada ou callback. O contexto
contém os campos explícitos do relógio e o gate de categoria originais.
Nenhum delta é inferido do ritmo do laboratório.

O estado 0 desconta o atraso e pode entrar no estado 1 na mesma chamada.
O estado 1 atualiza a intensidade e o tempo restante, e inicia a desativação
quando o tempo acaba ou a contagem real chega a zero. O estado 2 desconta
seu próprio tempo, calcula a intensidade de saída e escolhe o próximo estado
conforme o tipo e as flags. O estado 6 zera a intensidade. Valores de estado
a partir de 7 retornam após o prefixo comum, conforme o executável.

Os estados 3, 4 e 5 ainda exigem as rotinas de renovação e remoção. Se forem
alcançados numa chamada posterior, o motor informa esse limite; não mantém
o registro artificialmente ativo. A rotina também informa quando encontra
objetos auxiliares, transformação, consulta espacial ou callback ainda não
implementados. Os campos são exigidos apenas quando o caminho original os lê.
Por exemplo, o gate do relógio pode retornar sem ler a lista ou o delta.

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

A ligação ao Player usa a operação já existente `prepare_update` e uma
coleção externa, explicitamente atualizada entre consultas. O estado aleatório
produzido pelo construtor entra inteiro na inicialização do Player. A operação
da coleção e a operação do Player têm transações separadas; ainda não há um
scheduler que determine automaticamente sua ordem numa partida.

A associação desses recursos à cena, o bootstrap completo, a inicialização
do relógio e a máquina inteira de atualização continuam pendentes. Esse
componente não estabelece combate, missões, saves ou coop jogável. O launcher
publicado permanece no laboratório 0.17.0, aberto durante a validação.

As evidências locais ficam em `research/private/item-pop-update-v1/` e
`research/private/binary32-arithmetic-v1/`.
