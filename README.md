# MH4U Native — desenvolvimento local

Objetivo: reimplementar Monster Hunter 4 Ultimate em um motor próprio para macOS
e Windows, com PT-BR, seleção de texturas, controles configuráveis e coop online.
O executável entregue não usará runtime de 3DS. A compatibilidade com console
3DS foi adiada por decisão do usuário.

**Marco atual: laboratório nativo 0.17.0, com política de ações nativa e catálogo PT-BR ampliado.**

O catálogo atual contém 925 traduções em 1.082 fragmentos de dez documentos,
com 157 fallbacks preservados. O [intérprete de política](docs/player-policy-interpreter.md)
compõe tabela, predicados, tempo e transição, comparados em 341 execuções ARM
completas. Contextos e dispatches ainda não reconstruídos falham explicitamente.
O [Player persistente](docs/player-policy-actor.md) já recebe essa política por
uma operação transacional, com pose e relógio preservados.
Na árvore de desenvolvimento, as [ações do estado1](docs/player-state1-native.md)
já ligam quatro entradas e quatro continuações aos pedidos de animação e ao
Player persistente, comparadas em 384 e 487 execuções ARM completas,
respectivamente. O [ID2](docs/player-state1-id2-native.md) acrescenta custo,
posição e consulta de objetos em contexto explícito. A
[aquisição de altura do Player](docs/player-spatial-acquisition.md) já liga a
consulta sem cache ao handler e ao Player persistente. O
[início de atualização](docs/player-update-begin.md) copia a posição anterior,
limpa as flags da rotina original e invalida esse cache por uma operação
explícita. A [preparação de entrada](docs/player-input-preparation.md) acrescenta
agora o filtro local completo e o acumulador anterior a essa etapa. Preserva
limpezas parciais por byte e fornece operações explícitas no mesmo Player.
O [prefixo de atualização e a busca de áreas](docs/player-world-lookup.md)
compõem agora essas etapas e publicam PW5a8 em uma transação do Player.
A consulta percorre a coleção original e resolve sobreposições pela prioridade
do executável. A [construção dos registros](docs/item-pop-builder.md) agora inclui
a seleção ponderada original, referências IPL e o estado aleatório compartilhado.
A ativação dos registros ainda está pendente.
Scheduler e bootstrap continuam pendentes no runtime.
O módulo de [interseção vertical original](docs/spatial-triangle.md) passou
1.464 consultas preservadas, com102 hits idênticos byte a byte. É uma rota
distinta da função de face17fec8 usada na consulta espacial descrita abaixo.
A [ponte de posição](docs/player-position-bridge.md) agora tem implementação
nativa e operação transacional no Player, comparadas com 150 chamadas originais
PBW→modelo→PBW. A [orientação](docs/player-orientation.md) reproduz 69.076 casos
novos e 144 checkpoints retidos. O [leitor SBC e a consulta de faces](docs/spatial-face-query.md)
preservam os recursos e a normal original; a consulta passou em 1.444 chamadas ARM.
A [consulta espacial nativa](docs/spatial-query-native.md) agora compõe os filtros,
a grade GRCO, a seleção de contato e a função de altura, com estado persistente
de contadores e marcas. A referência inclui 312 consultas completas do código
original. A aquisição do Player acrescenta filtros, preparação do raio e cache,
com 88 chamadas originais completas e testes de integração persistente.
O estado inicial continua explícito; bootstrap e scheduler permanecem pendentes.
O launcher publicado permanece 0.17.0. A validação atual do
[prefixo de atualização](docs/player-world-lookup.md) passou em 120/120 CTests
no Mac e a compilação Windows x64 concluiu sem avisos; execução no Windows
continua sem validação.
Veja também o [catálogo v4](docs/localization-v4.md) e o
[ensaio da consulta espacial sobre a grade real](docs/player-spatial-context-v4.md),
com223 retornos completos,122 hits e101 misses no código original.

O novo [modo de inspeção do Player](docs/player-preview.md) permite pedidos
manuais de estado, pausa e avanço por passo no caçador da vila. Usa o controlador,
a pose e o skinning nativos com os 19 ossos reais. A inicialização e o ritmo
são explícitos de diagnóstico; a política completa de entrada ainda está pendente.
O caçador de sete peças e o em062 agora usam geometria, pesos, hierarquia e
animações nativas, sem conversão para glTF. A reprodução agora compõe troca
pendente, movimento global do modelo, pose, relógio, eventos e transição
na ordem recuperada. As 72 ações selecionadas têm
[reprodução e seek verificados](docs/native-actor-rendering.md). O ritmo de
60 quadros/s, a montagem do personagem e a apresentação continuam políticas de inspeção.
O seletor de ações agora executa o comando original, preserva o estado da pose
e troca os bancos quando necessário. A linha do tempo faz uma busca isolada.
A inicialização de metadados dos ossos segue os registros MOD e a ordem recuperada
([escopo do bootstrap](docs/model-animation-bootstrap.md)); não é o bootstrap completo do jogo.
O [preset da cena](docs/native-world.md) agora usa esse mesmo caçador nativo,
e a identidade da sala inclui seus MOD, LMT, texturas e constantes de transição.
As constantes usam a saída verificada do inicializador original.
O caçador pode se deslocar no cenário pelo teclado, com câmera acompanhando,
apoio no piso e bloqueio nas paredes/bordas. Movimento e parâmetros são próprios
do laboratório; caminhada, combate e física originais continuam pendentes.
O programa C++ também oferece um seletor com ator de 12 juntas/28 ações, cenário v00a01, colisão,
espada, escudo, torso parcial, conjunto de cinco peças e candidato de sete peças
com rosto/cabelo, 19 juntas e 44 ações. A nona opção compõe cenário e acessório
pelas primeiras chaves do SDL, sem afirmar o estado inicial original. São recursos
extraídos da ROM fornecida. Não contém combate, missões, saves ou uma sessão
cooperativa jogável. Duas janelas nativas já compartilham posições e colisão
calculadas pelo servidor, somente em loopback, com pose estática.
Os eventos das ações são consultados no motor nativo e exibidos como valores
numéricos no inspetor; disparo de golpes, efeitos e dano ainda não foi implementado.
O código em desenvolvimento também contém os [classificadores originais de
entrada](docs/player-input-runtime.md), comparados com 131.980 execuções ARM.
Eles ainda não dirigem o caçador: a [transição de ações](docs/player-transition-state.md)
e a ligação aos dispositivos continuam em investigação.
A árvore de desenvolvimento acrescenta a [conversão numérica dos analógicos](docs/input-quantization.md),
a [orientação de referência](docs/player-reference.md) e um
[perfil nativo de transição de ação](docs/player-transition-runtime.md), comparados
a trechos ARM originais. São bibliotecas do controlador em construção; ainda
não selecionam movimentos no laboratório aberto.
Foram acrescentados [filtros e normalização](docs/input-filter-runtime.md), um
[perfil de seleção em duas fases](docs/player-selection-runtime.md) e o
[setter nativo de animação](docs/motion-request-runtime.md). A nova
[composição do controlador](docs/player-controller-step-runtime.md) liga seleção,
[comandos de animação](docs/motion-command-runtime.md) e transição, com recursos
e globals explícitos. A [preparação das ligações aos ossos](docs/motion-binding-runtime.md)
agora compõe a [inicialização completa](docs/motion-initialization-runtime.md),
com [leitura nativa dos LMT](docs/motion-resource-runtime.md) e
[amostragem dos canais](docs/motion-sampler-runtime.md) comparadas aos recursos
e ao ARM original. O [consumo nativo das camadas pendentes](docs/motion-pending-runtime.md)
agora executa a troca de animação e preserva os snapshots da transição,
compondo a [restauração dos ossos](docs/joint-pose-runtime.md). Foram comparados
1.664 casos ARM completos da troca e 931 da restauração. As
[duas rotas de aplicação da pose aos ossos](docs/joint-update-runtime.md)
e a [consulta de movimento do modelo](docs/motion-query-runtime.md) também
estão em C++, incluindo várias layers, espelho e mistura aditiva. A
[mistura dos snapshots de transição](docs/motion-transition-runtime.md)
e a [construção hierárquica das matrizes](docs/joint-output-runtime.md)
também foram reproduzidas no perfil normal de saída. O [leitor nativo de
tabelas MOD](docs/model-resource-runtime.md) fornece as matrizes e paletas
originais à [preparação nativa para desenho](docs/joint-palette-runtime.md).
Ela calcula as matrizes e produz um plano de escritas e bindings, lendo os
ossos diretamente de seu dono. A [composição do frame](docs/model-animation-frame.md)
foi comparada em 2.586 execuções ARM completas, incluindo o
[relógio](docs/motion-clock-runtime.md) e a
[aplicação do movimento ao modelo](docs/motion-application-runtime.md).
O wrapper de pedido agora atende o seletor do visualizador; a política de
entrada e seleção de estado do jogador ainda não controla essa interface.
O [driver persistente do jogador](docs/player-animation-runtime.md) compõe
controlador, relógio, transição e matrizes com duas camadas ativas e oito posições
de armazenamento. A cadeia do controlador foi comparada em 501 casos ARM completos;
seus 32 quadros persistentes adicionais usam hierarquia sintética. O novo
[consumidor do Player](docs/player-actor.md) liga esse driver ao MOD real e ao
visualizador, em perfil de inspeção separado. Há também três rotas condicionais de
[seleção do estado zero](docs/player-action-policy.md), verificadas em 162 casos
ARM. A [consulta por área](docs/player-selection-area.md) acrescenta 328 casos
ARM e contexto emprestado. Os [39 predicados de entrada](docs/player-policy-predicate.md)
foram comparados em 5.555 casos ARM. A [cadeia de inicialização](docs/player-gameplay-context.md)
ainda depende de contexto e readiness não fechados; essas rotas não definem a
animação inicial nem uma política completa de controle.
O novo [consumidor CPU de skinning](docs/vertex-skinning.md) já alimenta o renderer
a partir da [geometria MOD bruta](docs/model-resource-geometry.md). O contrato original da paleta
está documentado em [joints e desenho](docs/joint-render-binding.md). A seleção
condicional do shader, seus índices/pesos e o upload de uniforms estão descritos
em [consumo da paleta](docs/joint-shader-consumption.md). A
[câmera/projeção original](docs/model-view-projection.md) foi investigada; a
GPU ainda usa a câmera e a iluminação do visualizador.
A [ordem pad → sPlayer → uPlayer](docs/player-input-scheduler-quest.md)
foi recuperada estaticamente, sob os gates originais. Isso ainda não dirige
o caçador no laboratório.
O marco também acrescenta a [consulta nativa da tabela de eventos](docs/motion-event-dispatch.md)
e corrige a ordem dos participantes quando clientes terminam a entrada na sala
em ordem diferente da conexão TCP. Os relatórios do marco anterior ficam em
`research/private/scene-validation/native-world-v2/`: os 84 testes passaram no
macOS, e os 292 arquivos do pacote foram verificados por SHA-256. A compilação
Windows permanece sem teste de execução nessa plataforma.

A integração 0.15.0 está registrada em
`research/private/scene-validation/player-preview-v1/`: 88 testes cobertos no
macOS (87 passaram na execução completa; o novo teste passou após corrigir uma
expectativa de amostragem e reexecutá-lo), compilação Windows x64 e 296 arquivos
do pacote verificados por SHA-256. O perfil público do Player passou 18 casos;
pausa, passo, pedido de estado e reinício foram verificados na janela real.
Os componentes têm auditoria de 1.273 pins, com snapshots históricos preservados.
O porte completo e o coop pela internet continuam em desenvolvimento.

## Abrir no Mac

Abra `Abrir laboratorio.command` ou execute, na raiz deste projeto:

```sh
python3 tools/launch_lab.py
python3 tools/launch_lab.py --model native-hunter --hide-primitive 3
python3 tools/launch_lab.py --model native-actor
python3 tools/launch_lab.py --world --hide-primitive 3
python3 tools/launch_lab.py --action 0 --hide-primitive 3
python3 tools/launch_lab.py --model shield
python3 tools/launch_lab.py --model sword
python3 tools/launch_lab.py --model stage
python3 tools/launch_lab.py --model collision
python3 tools/launch_lab.py --model player
python3 tools/launch_lab.py --model assembly
python3 tools/launch_lab.py --model appearance
python3 tools/launch_lab.py --model composition
```

No pacote atual, `Abrir controlador nativo.command` (Mac) ou
`Abrir controlador nativo.bat` (Windows) abre o Player na vila. **Espaço** pausa,
**ponto** avança um passo, **1/3** pede estado e **Home** volta à referência.
O pedido 3 preserva a animação já ligada; não é um comando de parada.
O atalho `Abrir cena controlavel.command` abre a cena com o inspetor anterior. Use
**WASD** para mover o caçador, mouse para orbitar e roda para zoom. A direção
acompanha a câmera. O analógico esquerdo está configurado para movimento e o
direito para câmera, ainda sem teste em joystick físico. A colisão cobre o piso
e as paredes selecionadas; móveis e outros atores não participam. A pose é
independente do deslocamento e pode parecer deslizar. Veja a
[integração da cena](docs/controllable-scene.md) e o
[contrato de locomoção](docs/locomotion-runtime.md).

Para o teste compartilhado no mesmo computador, abra `Iniciar servidor local`
e depois `Abrir jogador 1` e `Abrir jogador 2`. Cada janela controla um caçador;
o servidor calcula posições e colisão. Acesso pela internet, ações sincronizadas
e combate cooperativo ainda estão pendentes. Veja [a integração de rede](docs/network-viewer.md).

Os atalhos `Abrir acao pareada 0..6` inspecionam as sete ações com pose e
deslocamento nativo juntos. Espaço pausa, ponto avança um tick e Home reinicia
e pausa. Este modo ainda é separado da cena de rede; lê as máscaras de eventos,
mas não dispara efeitos de gameplay ou combate. Veja [a composição das ações](docs/action-motion-viewer.md).

Arraste para orbitar; use a roda para zoom. O painel permite escolher a textura
original, alternativa ou nenhuma, configurar a câmera e mapear os controles de
inspeção. O ator abre com animação, seletor de clip, pausa e controle de tempo.
O algoritmo do relógio é nativo; o ritmo de 60 ticks/s continua sendo uma
convenção de inspeção. O seletor de ações reinicia o estado; o loop respeita
a configuração do LMT. O movimento global ainda não controla a cena de rede.
O seletor permite trocar recursos sem fechar a janela; em falha, mantém a cena
anterior. O controle **Partes da geometria** permite isolar primitivas sem
alterar os recursos. Uma delas forma a silhueta preta observada no torso;
sua seleção no jogo original continua em investigação. O candidato com rosto e
cabelo reúne peças compatíveis; seleção, cores e visibilidade originais ainda
não foram reproduzidas. Veja a [investigação de aparência](docs/player-appearance.md) e o
[contrato do catálogo](docs/resource-catalog.md).
Para fornecer uma textura alternativa ao modelo estático:

```sh
python3 tools/launch_lab.py --model shield --texture-alternative /caminho/textura.ppm
```

Cenas glTF aceitam [packs PNG vinculados ao hash da imagem original](docs/texture-packs.md)
com `--texture-pack /caminho/pack.tsv`. Imagens sem substituição conservam a textura
original. Não há pack HD incluído. O painel **Textos do jogo** inspeciona 1.082
fragmentos de dez documentos, incluindo forja, missões e ajuda, alternando 925 traduções experimentais
e o inglês original. O layout e os comandos desses menus continuam pendentes;
veja o [catálogo ampliado](docs/localization-v4.md).

As configurações são gravadas apenas ao clicar em salvar, na pasta de
preferências `MH4UNative/ResourceLab`, com backup da configuração anterior.
O suporte a gamepad está implementado, mas não foi testado com controle físico.
O significado do canal alpha original é desconhecido; transparência fica
desligada por padrão e o controle é identificado como experimental.

## Reproduzir as ferramentas e a compilação

O bootstrap foi preparado e executado em macOS Apple Silicon. Requer Python,
Git, Node/npm e as ferramentas de compilação Apple já disponíveis no host.
Instala Ghidra, JDK, CMake, LLVM-MinGW e dependências em `.tools/` e `vendor/`.
Versões e hashes ficam em `tools/dependencies.lock.json`; REA usa um lock npm.
O código de localização requer Python 3.10 ou mais recente.
Nenhum perfil de shell ou configuração global é alterado.

```sh
python3 tools/bootstrap_macos.py
python3 tools/setup_analysis.py
python3 tools/build.py macos
python3 tools/build.py windows-x64
python3 tools/package_lab.py
```

O pacote macOS é arm64. O Windows é x64, compilado com LLVM-MinGW; não houve
execução em Windows. As importações do PE foram inspecionadas e são bibliotecas
do Windows/UCRT. A build Windows não depende de DLLs do SDL ou do compilador
instaladas separadamente. É necessário um driver com suporte ao OpenGL usado
pelo programa. Os pacotes são de desenvolvimento local, sem assinatura de
distribuição. O manifesto de cada pacote registra hashes de seus arquivos.

## Importar os recursos e trabalhar na tradução

Veja [o pipeline de recursos](docs/resource-formats.md) para recriar a inspeção,
o inventário e os recursos a partir da ROM local. Os originais permanecem
inalterados. Conteúdo extraído e código descompactado ficam nas pastas
ignoradas `assets/local/` e `research/private/`.

O [índice pesquisável](docs/resource-index.md) cobre 112.427 membros em
10.592 ARC desta ROM. A [conversão esquelética](docs/skeletal-models.md) preserva
a hierarquia, os pesos e a pose de referência do modelo observado.
[Animação](docs/animation-findings.md) registra codecs, amostras e limites.
As curvas de rotação foram corrigidas a partir dos consumidores ARM e verificadas
em 225.131 consultas nativas, com diferença máxima de 0,0001031° frente às
equações derivadas. Isso não valida ritmo, eventos ou deslocamento do personagem
no jogo original. Veja os [codecs](docs/quaternion-codecs.md) e o
[sampler nativo](docs/animation-runtime.md).
O [cenário](docs/stage-findings.md), o [caçador parcial](docs/player-findings.md),
a [montagem de cinco peças](docs/player-assembly.md)
e as [consultas nativas de colisão](docs/collision-runtime.md) possuem evidências
separadas. As consultas geométricas ainda não implementam a física do jogo.
A [composição da vila](docs/stage-composition.md) acrescenta o acessório `a00`
na primeira posição documentada. Spawn, visibilidade e tempo do cenário não
foram recuperados. A [aplicação de root motion](docs/root-motion-runtime.md)
tem biblioteca C++ com aplicação, pré-update, previsão e avanço contínuo
comparados a funções ARM isoladas. O [binding de sete ações reais](docs/action-motion-binding.md)
passou por 2.590 ticks comparados e preserva estados/flags entre ticks. O
[sampler nativo](docs/model-motion-sampler.md) preserva ausências e quaternions
brutos; os [defaults comprovados](docs/root-motion-defaults.md) são aplicados
somente no binding. Esse consumidor continua separado do movimento do viewer
e do controlador de gameplay.

O [inventário LMD](docs/localization-inventory.md) cobre 42.196 fragmentos em
285 conteúdos únicos aceitos; oito conteúdos únicos continuam rejeitados.
Os rascunhos `localization/pt-BR/title.seed.json`, `charmake.seed.json` e
`common-buttons.seed.json` mudam **160 fragmentos**, vinculados aos hashes
originais. O pacote inclui os overrides separados em `PT-BR-experimental/`,
com catálogos, proveniência e relatórios. O motor carrega um catálogo próprio
de 1.082 fragmentos, com 157 fallbacks preservados; tradução completa e menus jogáveis continuam
pendentes. O override local da referência exibiu
os cabeçalhos “Menu principal” e “Opções de jogo” e 14 fragmentos da criação
de personagem, incluindo instruções e acentos, sem corte visível nas capturas.
Os botões “Voltar” e “Confirmar” também foram observados na criação de personagem.
Os demais textos/layouts ainda não foram validados. Os rótulos gráficos dos
botões do menu principal permanecem em inglês.
Detalhes: [localização](docs/localization-findings.md).

A [execução de referência](docs/reference-runtime.md) usa Azahar separado para
comparar a ROM original e testar recursos derivados. O emulador não faz parte
do motor nem do pacote nativo. A ROM original permanece intacta.

## Rede e colisão

`mh4u-network-lab` verifica um manifesto de recursos e executa um servidor com
até quatro participantes. O teste passou com clientes nativos separados,
entradas inválidas e encerramento por sinal. O endereço é loopback; Internet,
LAN, autenticação e sincronização de combate ainda não foram implementados.
O modo `--world` acrescenta movimento autoritativo e snapshots de até quatro
atores. Veja [o protocolo de movimento](docs/network-world.md),
[a cena conectada](docs/network-viewer.md) e [o protocolo inicial](docs/network-foundation.md).

`mh4u-collision-query` consulta a malha estática por raycast/segmentcast, com
BVH própria e identidade dos triângulos preservada. São ferramentas de
desenvolvimento incluídas no pacote, além da interface gráfica.

## Investigação com REA

REA 6.5.0 usa Ghidra 12.1.4 com JDK 21 e decompilador compilado para arm64.
`tools/rea_session.mjs` é um cliente do MCP público que mantém uma única sessão
e preserva respostas completas, catálogo e logs sob `research/private/`.

O contêiner NCSD não é diretamente analisável pelo REA. A imagem ARM extraída
é envolvida em um ELF **sintético e somente para análise**, com segmentos
derivados do NCCH. Isso não transforma o código ARM em um jogo para PC.
Metadados sintetizados e limites ficam no arquivo de proveniência. O intervalo
BSS foi derivado do loop de inicialização e corroborado pelo exheader; o gerador
recusa divergências antes de mapear esse intervalo. O REA/Ghidra realiza análise
estática com o perfil `ARM:LE:32:v6`. O harness separado de root motion usa
Unicorn apenas na investigação, executando funções isoladas com estado sintético
e interceptações declaradas; ele não integra o executável entregue. Veja
[as evidências REA](docs/rea-findings.md) e [a derivação BSS](docs/rea-bss.md).

Para gerar a imagem de análise a partir de uma inspeção local:

```sh
python3 tools/make_analysis_elf.py research/private/rom-inspection research/private/analise.elf
```

Para abrir a sessão, execute `node tools/rea_session.mjs` e envie uma chamada
MCP por linha. Consulte o catálogo salvo pela própria sessão antes de escolher
argumentos. Use `exit` para fechar a análise e o servidor. A configuração local
contém os caminhos das ferramentas; não é necessário configurar o app Codex.

## Limites e próxima etapa

As builds e o renderer são uma primeira base técnica. O porte completo exige
reconstruir os formatos restantes, ampliar a cobertura de animação e implementar
física original, combate, IA, missões,
progressão, save, menus e rede. Os requisitos originais continuam abertos;
veja [marcos e critérios de validação](docs/roadmap.md).

Dependências: SDL sob licença zlib, Dear ImGui e cgltf sob MIT, REA sob MIT e ferramentas
sob suas licenças originais. As licenças do código ligado aos executáveis são
copiadas aos pacotes locais. Referências públicas de engenharia reversa e sua
proveniência estão documentadas nas páginas de formatos.
