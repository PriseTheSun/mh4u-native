# Atualização do modelo após o contato

`Player::prepare_and_update_model` prolonga a atualização iniciada em `b1407c`
até antes de `b14724`. O mesmo candidato passa pelas etapas de contato e pelas
três chamadas originais `b07c08`, `b06f8c` e `b07bb8`. A ponte existente faz a
orientação, os temporizadores, o avanço da animação e a publicação das matrizes.

A posição usada pelo modelo vem do contato já calculado. O delta continua
pertencendo à admissão e os templates pertencem ao contexto de despacho.
`PlayerModelContext` fornece apenas recursos e callbacks adicionais; não aceita
outro relógio ou outro conjunto de templates. As duas camadas ativas entram na
atualização do modelo; os seis slots inativos permanecem preservados.

As declarações compartilhadas de frame, limites e efeitos têm um único dono
em `player_animation_types.hpp`. A ponte não depende mais do módulo que compõe
a atualização completa. O contexto anterior da ponte e o novo contexto de
recursos usam o mesmo corpo numérico. O ramo `d64 & 8` retorna antes de consumir
recursos do frame; `d64 & 2` suprime somente a publicação das matrizes.

A preparação das duas imagens, quando há outro Player envolvido, termina antes
de publicar qualquer alteração. Personagem, pose, posição, gerenciador,
coleção, grade e fila compartilham essa fronteira. O contador de frames avança
uma vez quando a ponte executa a animação; ele não indica conclusão do scheduler
original nem avança a animação do outro Player.

Os testes persistentes usam o caçador com sete partes e 19 ossos. Cobrem delta
inteiro e fracionário, as duas saídas condicionais, duas consultas de piso
seguidas de uma falha tardia, e falhas de alocação com nova tentativa nos mesmos
objetos. No perfil normal e com ASan/UBSan, os 291 pontos de alocação da operação
selecionada preservam o estado anterior quando falham. Essa cobertura não
representa todos os erros possíveis do sistema.

A coleta contínua contém oito execuções originais completas e um prefixo
que termina antes do modelo. O comparador confirma sete saídas nativas completas
e esse prefixo. No oitavo caso, o peso de interpolação causa estouro numérico:
o ARM conclui com zeros assinados e o contrato aritmético nativo retorna
`numeric_overflow`. Esse caso continua preservado como diferença conhecida,
sem alterar seus dados para produzir uma aprovação. Entre as sete saídas
nativas aceitas, seis avançam o frame e cinco publicam as matrizes; os gates
explicam a diferença. A preparação original e o comparador têm revisões
independentes. A regressão da ponte preserva os 150
casos anteriores. Um teste adicional com recursos reais verifica duas
inicializações, quatro amostras do modelo, 20 amostras das juntas e 19 juntas
publicadas pelo método persistente; ele não estabelece cobertura completa de
eventos.

A validação pública final passou nas 172 suítes do macOS. Todos os alvos
Windows x64 foram compilados sem avisos; não houve execução no Windows.

Os relatórios desta etapa ficam em `research/private/player-update-model-v1/`.
A execução contínua do original usa preparação diagnóstica explícita de modelo,
camadas, juntas e recursos. Ela não demonstra a construção original de um
personagem a partir de um save. Os caminhos ativos `b06b38` e `d00628` ainda
retornam erro explícito no código nativo.

O laboratório publicado continua na versão 0.17, aberto para acompanhamento.
Esta API de desenvolvimento ainda não completa o ciclo do jogo. Combate,
missões, salvamento, tradução integral, teste de joystick físico, execução no
Windows e cooperação pela Internet continuam pendentes.
