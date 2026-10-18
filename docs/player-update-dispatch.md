# Atualização do jogador com despacho de ações

`player_update_dispatch` compõe a preparação anterior com o despacho original
de ações. O ponto inicial é `b1407c`; o ponto final fica antes de `b1441c`,
onde o chamador passa a buscar a função virtual seguinte. Não corresponde
a uma atualização completa ou a um quadro de uma partida.

O primeiro `b66a20` sempre executa depois da preparação, inclusive para um
jogador remoto. Os próprios handlers conservam seus testes de controle local.
Após a primeira chamada, o chamador lê PBW376. Se esse byte for diferente de
zero, escreve `1` em PW01 e executa um segundo despacho com o estado, a ação
e o subestado resultantes da primeira chamada. Não existe um terceiro teste:
o PBW376 final da segunda chamada permanece como o handler o deixou.

Cada despacho limpa PBW376 antes de consultar o estado. O dono existente em
`player_state1` admite as ações 1, 2, 4 e 9 do estado 1 e o retorno vazio
original do estado 3. Outros estados e ações ainda retornam erros explícitos.
O estado 3 preserva os demais campos e não exige recursos de animação ou
contextos de helpers que não são consumidos nesse caminho.

A composição usa um único candidato. O segundo despacho recebe suas camadas
e seus campos atualizados; as consultas de objetos usam a coleção candidata
produzida pela preparação. Referência, delta, AD50, modo C8c e constantes
continuam com seus donos anteriores. Os novos argumentos fornecem as tabelas
de ações, recursos de animação, mapeamentos de área e contexto espacial.

Os efeitos dos dois despachos ficam separados e preservam a identidade do
estado, ação e subestado de entrada. PW01 não é lido para fazer a escrita
condicional; no caminho de uma única chamada, seu conhecimento anterior é
preservado. Nenhum relógio de animação avança implicitamente.

O perfil atualmente recuperado admite no máximo uma consulta espacial em
um resultado bem-sucedido. A consulta do ID2 pode causar uma transição para
outros handlers ainda não admitidos, ou retornar sem solicitar um segundo
despacho. A transição suportada ID2 para ID9 ocorre no outro braço. Essa
propriedade permite manter a cena emprestada durante o cálculo e publicar
seus contadores e marcas junto com o Player no final. Ampliar os handlers
exige reexaminar a leitura de alterações espaciais entre chamadas.

A comparação passou em 49 execuções originais completas até esse checkpoint,
com 53 despachos, quatro sequências duplas, 64 pedidos de animação e 343
consultas de atributos. Os checkpoints intermediários incluem o estado global,
inclusive a alteração do gerador aleatório pela política. As identidades de
animação preservadas por pedidos recusados são comparadas separadamente do
seletor final. Os valores esperados vêm das observações ARM; o resumo de
contagens do harness nativo serve apenas para verificar estabilidade entre builds.

Normal e ASan/UBSan/float-cast-overflow passaram na comparação, nas regressões
de 216 e 115 casos e em 102 processos com entradas adulteradas. Os testes do
Player persistente passaram em ambos os perfis, incluindo 53 falhas de
alocação até alcançar a renderização, com preservação dos três donos de estado
e nova tentativa na mesma instância. Hit, miss e grade sem pares têm cobertura
nativa de integração; não fazem parte dos 49 casos ARM desta etapa.

As evidências e os limites completos ficam em
`research/private/player-update-dispatch-v1/manifest.json`.
A suíte pública passou em 137/137 CTests no macOS; Windows x64 compilou sem
avisos, sem execução nesse sistema. O launcher publicado permanece na versão
0.17.0; esta extensão está na árvore de desenvolvimento. Essa fronteira
foi estendida pelos [ajustes após o despacho](player-update-post-dispatch.md),
que chegam ao ponto anterior a `b145dc`.
Bootstrap, scheduler, combate, missões, saves, PT-BR integral, joystick físico,
execução no Windows e coop pela Internet continuam pendentes.
