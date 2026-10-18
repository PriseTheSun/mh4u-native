# Ajustes após o despacho de ações

`player_update_post_dispatch` prolonga a atualização nativa desde `b1407c`
até o ponto anterior a `b145dc`. Reutiliza a preparação e os despachos de
ações já recuperados, ajusta as duas camadas de animação e consulta as regras
de ações. Não representa uma atualização completa do jogo.

A chamada virtual `+10c` exige a identificação explícita da classe examinada.
Seu alvo `b25b68` contém apenas um retorno. O fator de velocidade começa em
1 ou no valor PW10 para o estado 1; PW0d igual a 1 força zero e, caso contrário,
PW0c igual a 1 multiplica o fator por 0,1. No ramo que força zero, o valor
numérico carregado de PW10 é sobrescrito antes de qualquer uso observável;
o modelo nativo permite que esse campo permaneça desconhecido nesse ramo.
Isso é equivalência de efeitos numéricos, não equivalência de todas as leituras.

O ajuste principal escreve a velocidade e, quando a interpolação restante
é positiva e a nova velocidade não é zero, recalcula os dois tempos usando
as divisões e a multiplicação na ordem original. A segunda camada recebe
somente sua velocidade. PWc0 recebe o mesmo valor da primeira camada.
O dono numérico conserva zeros com sinal, infinitos e NaNs, inclusive a ordem
dos operandos; não substitui resultados excepcionais por valores padrão.
Não avança o relógio nem recalcula a pose implicitamente.

A consulta `b2a75c` percorre registros de 22 bytes até um marcador final.
O limite de 48 registros pertence ao intérprete anterior, não a essa consulta.
O dono da tabela fornece os campos tipados, preserva o carrier e seu hash,
e aceita um orçamento explícito de bytes para carriers maiores. A consulta
pode terminar ao encontrar uma ação antes de precisar do restante do carrier;
se alcançar um campo ausente, retorna erro. Um limite de cobertura nunca
se transforma em uma resposta negativa. As duas comparações de tempo têm
comportamentos diferentes com NaN, reproduzidos conforme as instruções ARM.

A consulta de elegibilidade `b345b8` reutiliza o dono da lista de estados e
conserva os testes condicionais para os seletores 37–47. O chamador escolhe
37, 42 ou zero; sua seleção de 42 e zero passa por instruções fora do bloco
contíguo principal, incluídas nas evidências. Campos e listas desconhecidos
só são exigidos nos caminhos que precisam deles.

O predicado `b3444c` tem agora um único dono em `player_transition`. Tanto o
cálculo de atributos quanto a elegibilidade o reutilizam, preservando a ordem
original dos testes; seletores fora do intervalo não provocam deslocamentos
de bits inválidos.

`Player::prepare_and_adjust` publica jogador, coleção e alterações da grade
espacial depois de toda a operação e da preparação da renderização. Compartilha
a implementação de publicação com `prepare_and_dispatch`; não mantém uma
segunda cópia persistente dos campos ou das animações. Uma falha tardia deve
preservar todos esses donos e permitir nova tentativa na mesma instância.

A comparação passou em 107 execuções originais desde `b1407c`, com 111
despachos, além de oito cenários que começam diretamente na fase nova e 446
chamadas isoladas dos ajustes de velocidade. Esses cenários isolados não provam
que a preparação completa alcance os mesmos estados. Os checkpoints incluem
os resultados após a velocidade e após a consulta da tabela.

Normal e ASan/UBSan/float-cast-overflow passaram com 3.243.369 verificações e
615.861 verificações de projeção por perfil. As regressões de 49, 216, 115, 54
e 96 casos também passaram. Os 76 processos com dados adulterados terminaram
com rejeição normal, sem abortos dos sanitizadores. Os testes do Player
persistente confirmaram 53 falhas de alocação até a renderização, com nova
tentativa na mesma instância e preservação de jogador, coleção e grade.

A suíte pública passou em 141/141 CTests no Mac. Windows x64 compilou sem
avisos, com execução nesse sistema ainda não verificada. As identidades,
medições e limitações completas estão em
`research/private/player-update-post-dispatch-v1/manifest.json`.

A fronteira seguinte começa em `b145dc`, antes de `b22164`. O mapa das chamadas
restantes não equivale à implementação nem à execução dessas funções.
Bootstrap, scheduler, combate, missões, saves, PT-BR integral, joystick físico,
execução no Windows e coop pela Internet continuam pendentes. O launcher
publicado permanece 0.17.0; esta extensão pertence à árvore de desenvolvimento.
