# Movimento e contato no Player

`Player::prepare_and_update_contact` prolonga a mesma atualização iniciada em
`b1407c` até antes de `b14700`. Depois da etapa do gerenciador, limpa a flag
original, executa a integração e a consulta de piso quando `PW6c0` solicita,
e calcula os indicadores de contato e o valor `Player.d2c`.

A posição usa a velocidade, a aceleração e o único delta do chamador, com cada
operação arredondada em binary32 na ordem original. O retorno da consulta
preserva o byte da classificação e a normal armazenada no SBC. Os atributos
vêm dos registros já analisados pelo leitor do recurso. Duas palavras que a
rotina original copia sem inicializá-las no extrator de atributos continuam
desconhecidas no contrato nativo;
não são preenchidas com zero.

Quando o despacho anterior também consulta o piso, a consulta de contato usa
as marcas e os contadores produzidos por ele. A cena emprestada permanece
intacta durante os cálculos. O módulo espacial valida e reúne as alterações
por slot; essa interpretação não é repetida no Player. As observações de cada
consulta ficam separadas do plano final de publicação.

O indicador `d2c` continua pertencendo ao mesmo estado usado pela ponte de
posição. O gerenciador, a referência, os campos do personagem e as camadas
usam os valores produzidos pelas etapas anteriores. A publicação reúne
personagem, posição, outro Player quando alcançado, gerenciador, coleção,
grade e fila, depois de preparar todos os quadros visuais necessários.

Os testes persistentes usam as sete partes e os 19 ossos do caçador. Verificam
duas consultas sucessivas, falha tardia seguida de nova tentativa e falhas de
alocação durante uma operação que combina contato, alteração de outro Player
e duas preparações visuais. A comparação com o executável original e os
relatórios de cada perfil ficam em `research/private/player-update-contact-v1/`.

A comparação contínua cobre 28 execuções originais: 26 chegam ao limite de
contato e duas param em caminhos ainda não admitidos. Foram comparadas 22
consultas, 12 resultados de colisão e oito publicações de contato. A regressão
da etapa anterior mantém 239 casos. O corpus separado dos auxiliares de
contato contém 262 execuções; ele não amplia a cobertura do chamador contínuo.
Nos testes persistentes, 255 pontos de falha de alocação da nova operação
preservam os donos originais e admitem nova tentativa.

O primeiro coletor de diagnóstico lia dois campos da grade em endereços
incorretos. A derivação final recupera a profundidade e as marcas a partir
dos checkpoints brutos preservados; o produtor revisado não é apresentado
como se tivesse gerado a execução anterior. A comparação contínua verifica
IDs e ordem dos pares, contagem de chamadas de face, atributos e grade. O
carrier temporário de uma chamada não é usado como identidade de face.

A validação pública final passou nas 168 suítes do macOS. Todos os alvos
Windows x64 foram compilados sem avisos; a execução nessa plataforma continua
não verificada. Os testes de componentes também passaram com ASan/UBSan.
A rodada completa detectou e permitiu corrigir uma regressão de classificação
de falta de memória: o planejador espacial agora distingue alocação ambiental
de limite explícito. As oito integrações de Player passaram após a correção,
com as expectativas originais preservadas. Relatórios anteriores permanecem
retidos junto dos relatórios finais.

A consulta admite o perfil direto de SBC já recuperado. Outros tipos de
objeto, transformações e grupos ainda não cobertos retornam erro explícito.
As referências são snapshots independentes; o contrato não deduz aliases
entre coordenadas do gerenciador e memória do personagem. O template usado
quando a referência é nula deve ser fornecido explicitamente.

Este ponto termina antes da ponte que atualiza o modelo e a pose. O
laboratório publicado continua na versão 0.17. O ciclo completo de jogo,
combate, missões, salvamento, tradução integral, controles físicos, execução
no Windows e cooperação pela Internet ainda fazem parte do trabalho pendente.
