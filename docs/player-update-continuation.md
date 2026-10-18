# Atualização do jogador até o primeiro despacho

`Player::prepare_to_dispatch` reúne a preparação de entrada, a seleção de ações
e as consultas seguintes de atributos em uma operação nativa. A execução
comparada começa em `b1407c` e para imediatamente antes da chamada a `b66a20`
em `b143f8`. Esse ponto ainda está dentro da atualização; não é um quadro
completo do jogo.

A composição reutiliza os donos do prefixo, da política e das transições.
`player_parameter_query` implementa os seletores 0 a 6 de `2f8e04`, lendo os
descritores e as tabelas que o código original consulta. Os sete descritores
são examinados na ordem original. As escritas em PBW9404 também são
preservadas, inclusive quando descritores distintos usam o mesmo índice.
As APIs recebem recursos e campos explícitos; não aceitam resultados prontos
para substituir essas consultas.

`player_update_statistics` reproduz as duas consultas condicionadas ao controle
local, as conversões e os limites aplicados pelo chamador e as cinco consultas
incondicionais de `b0c86c`. Jogadores remotos ainda passam pelas conversões,
pelos limites e pelas cinco consultas. Os bônus usam a busca real pela lista
de jogadores ou a tabela do modo 2, conforme o contexto original. O jogador
atual tem uma identidade própria nessa lista, de modo que a consulta usa o
estado candidato atualizado.

As operações numéricas preservam a ordem dos operandos e o arredondamento
binary32 do ARM. Os limites finais de `b0c86c` comparam os bits como inteiros;
por isso não foram substituídos por uma função genérica de limite de floats.
Os temporizadores usam as condições de comparação do original, incluindo o
tratamento de valores NaN. As conversões para inteiro saturam e arredondam
em direção a zero. O relógio consultado pelo bônus é um valor explícito do
contexto, sem aquisição implícita do relógio do computador.

O perfil inclui os retornos iniciais reais das duas rotinas seguintes:

- `b2f09c`: estado 8 ou jogador local com PW1bc positivo e ordenado.
- `b42108`: PWa15 zero; ou jogador local com PW296 menor ou igual a PW294;
  ou jogador local com PBW6f9 zero após esse teste.

Os demais caminhos dessas duas rotinas retornam erro tipado quando
alcançados. A divisão por zero do bônus de tempo também permanece um erro
explícito, pois o código original entra em uma rotina de exceção fora deste
perfil. Depois dos retornos admitidos, a composição copia e limpa PW4f8,
limpa PBW93ee/PBW93e9 e transfere o bit correspondente de PWf0 para PWf8.

Recursos externos são vistas imutáveis durante a chamada. Dados ausentes,
dados desconhecidos e referências fora da extensão são condições distintas.
Um campo só precisa estar conhecido quando o caminho o interpreta; uma
cópia simples pode preservar conhecimento parcial. O orçamento da busca pela
lista de jogadores é um limite de trabalho do host, não uma capacidade
inferida do formato original.

Na estrutura original, o quinto slot da lista ocupa a mesma palavra do
contador. A API conserva contadores maiores que quatro quando a busca
retorna em um slot anterior, mas rejeita explicitamente uma busca que chega
a esse ponto de sobreposição. Não aceita um quinto ponteiro independente
do contador nem atribui uma capacidade máxima ao formato.

O Player produz um único estado candidato e renderiza antes de publicá-lo
junto com a coleção externa. Falhas tardias não publicam a preparação
anterior, nem avançam o relógio da animação. A integração usa a composição
pura, sem encadear operações públicas que publicariam estados intermediários.

As referências usam estados de diagnóstico explícitos e recursos delimitados.
Não comprovam o bootstrap, os carregadores ou a cadência do scheduler em uma
partida. O ARM é executado somente na pesquisa comparativa; não faz parte
do produto nativo. O laboratório publicado permanece aberto, com a versão
0.17.0. Combate, missões, saves, PT-BR completo, joystick físico e coop pela
Internet ainda não estão concluídos.

As evidências e os relatórios desta etapa estão em
`research/private/player-update-continuation-v1/`. A comparação passou em
216 execuções originais, com 1.504 consultas, 670 escritas de flags e 432
checkpoints intermediários. O comparador nativo realizou 5.545.515 verificações
e 32.472 verificações de projeção do movimento por perfil, normal e com
ASan/UBSan/float-cast-overflow estritos. Os 76 processos com entradas inválidas
foram rejeitados como esperado; as referências anteriores de 115, 96 e 54
casos também passaram nos dois perfis.

Os 31 controles nativos de conhecimento parcial e limites são uma cobertura
separada. Modo 2, jogador externo na lista e sobreposição do quinto slot não
foram novas execuções ARM nesta etapa. O comparador preserva e confere as
arenas originais, mas não afirma equivalência de todo o fluxo de instruções,
registradores ou FPSCR.

A integração persistente passou em 409.650 verificações por perfil, incluindo
17 falhas de alocação, a primeira falha dentro da renderização e repetição
bem-sucedida no mesmo Player. A suíte pública passou em 134/134 CTests no Mac.
As compilações Mac e Windows x64 terminaram sem avisos. O executável Windows
foi compilado, mas não executado; o pacote visual publicado não foi substituído.
