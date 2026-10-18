# Jogador persistente com geometria nativa

`native_actor::Player` consome o driver `player_animation` com o caçador de sete peças da ROM. A cadeia agora alcança a geometria MOD: controlador → pedido com consulta live → frame original → transição → hierarquia → paletas → skinning CPU → `native_actor::Frame`. O estado inicial, os pedidos e o delta são explícitos. A composição não recupera, por si só, a criação completa de uPlayer nem a política de input de gameplay.

## Donos e interface

`Actor::Data` é o único dono imutável da receita, modelos MOD, LMTs, texturas, remapeamentos, globals, lista de arquivos e apresentação. `Actor::load` continua sendo o único loader. O inspetor anterior de uma camada mantém seu comportamento. `Player::from_rest` compartilha esses dados e cria um estado independente com duas camadas ativas e seis posições inativas.

A pose atual, canais, relógio, snapshots, controlador e hierarquia pertencem somente a `Player::state_`, um `player_animation::State`. A pose MOD de repouso em `Data` é um recurso inicial imutável, não uma segunda pose corrente. `Actor::Data::render` recebe uma view síncrona das matrizes do dono da hierarquia: aceita tanto o resultado legado quanto `player_animation::JointOutput`, sem extrair/copiar toda a pose para outra representação. O remapeamento temporário por peça necessário à paleta permanece no renderer compartilhado.

- `from_rest(Actor, PlayerInitial, Constants, PlayerBanks, Limits)` exige oito estados de camada, modelo TRS, snapshots, campos do controlador e flags explícitos. Usa as poses, matrizes e hierarquia do MOD já carregado. Não emite pedido de ação nem avança o relógio.
- `from_rest(Player, PlayerInitial)` reutiliza recursos, bancos, constantes e limites do dono existente. É uma factory de seed explícito; não guarda ou reaplica uma cópia da pose anterior.
- `apply(command, templates, area)` executa Common ou Wrapper sem avançar o frame.
- `step(PlayerStep)` avança a cadeia usando o único delta recebido.
- `tick(command, PlayerStep, area)` compõe pedido e passo.
- `capture()` renderiza o estado atual, sem avançar cursores, relógio ou contador.

`PlayerBanks` distingue `Unknown`, `Absent` e `MotionSource{sha256}`. A factory resolve o SHA256 completo contra os LMTs imutáveis da receita, preservando o token canônico de recurso. Digests truncados, estranhos ou ambíguos são erros. Tracks e eventos já ligados são resolvidos pela identidade original do recurso; uma associação de banco não reinterpreta esses vínculos. Resolvers chamam `motion_resource` e não leem LMT novamente.

O resultado é `PlayerOutput{Frame, CommandEffects?, FrameEffects?}` ou `PlayerError`. Razões do driver e do renderer permanecem tipadas e aninhadas. Toda a operação, inclusive geração de paletas e skinning, termina antes de publicar o novo estado. Uma falha de normal degenerada depois de controlador, frame e hierarquia não publica pose, cursor, pedido, snapshot ou contador parcial.

`Frame.tick` conta passos concluídos do consumidor, começando em zero; não é FPS nem o frame fonte. `Frame.source_frame`, máscaras e estado de fim resumem a camada zero. O estado completo de ambas as camadas e os efeitos do driver continuam acessíveis; esse resumo não apaga a camada um. A apresentação e os arquivos são compartilhados e sobrevivem à destruição do `Actor` usado pela factory.

## Perfil e capacidade

A factory verifica joints, snapshots, ordem e a soma dos bindings das oito camadas antes da cópia. Os dois bindings ativos e os snapshots devem ter a extensão exata do rig MOD. Defaults herdados: 256 joints/snapshots/itens de ordem e 2.048 bindings. Campos inativos não consumidos, inclusive bits não finitos, são preservados. Recursos de modelo, parent, templates, base de referência e área permanecem explícitos; valores desconhecidos não viram identidade ou ausência.

`PlayerStep` e `AreaContext` são emprestados somente durante a chamada. Templates e contextos externos não constam na lista de arquivos da receita: a lista identifica somente os dados realmente carregados pelo dono. O chamador que precisar de identidade de sessão deve incluir separadamente sua política e constantes. O renderer continua sendo a implementação CPU validada para o perfil VS18; não é execução PICA/GPU original, e não aplica normalização de pesos ou conversão glTF à geometria.

## Adaptador de inspeção

`PlayerPreview` é uma camada de política separada, usada somente por opt-in. Aceita a combinação de sete peças e 19 joints; oferece `from_actor`, `request(1|3, interpolation=4)`, `step`, `reset`, `capture`, `presentation` e `player() const`.

A seed de inspeção inicializa valores finitos: Q identidade, escala um, oito tabelas de bindings, snapshots explícitos e gates diagnósticos `global.field2b=2` e `pbw.field30=3`. As camadas começam com pending bit 2 e nenhuma ação ligada; esse bit não é mantido artificialmente durante reprodução. As consultas live depois da primeira preparação passam pelo consumidor original recuperado. Os templates são T zero, Q identidade e fallback inverso zero, parent explicitamente nulo, referências identidade; `step` fornece delta binary32 1. Isso é política diagnóstica, não alegação de que esses valores sejam o startup do jogo.

Somente banco zero recebe o LMT co_0 de SHA256 `c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8`. Os outros quinze permanecem desconhecidos. O arquivo entry54913 não é apelidado co_2. Os globals vêm da receita validada, que nesta amostra conserva o snapshot pós-inicializador d3f6fc de SHA256 `283bcb5502bd9e7c2daf2245fa36170f8f1f838accd1ed146ff3ba0f3e03b922`.

Factory e reset não fazem pedido ou passo implícito. `request` não avança tempo. O estado fonte 3 faz somente os stores auxiliares do controlador e não emite AnimationRequest: a seed vazia continua vazia, e uma ação já ligada permanece ligada. Ele não é rotulado como parada ou idle. Reset reconstrói a seed, renderiza e só então substitui o estado; não conserva um `Actor` legado ou uma pose salva. Um host pode oferecer pausa e passo unitário; um pacing de 60 Hz, se escolhido pelo visualizador, continua sendo política do host. O adaptador não faz seek independente, não mapeia joystick e não sincroniza comandos pela rede.

## Evidência e verificação

A prova dos componentes vem das investigações source-bound do mesmo ELF SHA256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`: `player-animation-runtime.md`, `motion-command-runtime.md`, `motion-clock-runtime.md`, `joint-output-runtime.md`, `model-resource-geometry.md`, `joint-palette-runtime.md` e `vertex-skinning.md`. Esta etapa reutiliza seus donos; **não executa um novo oracle ARM de todo o caçador**.

O teste integrado usa a receita real `assets/local/native-actor-v1/hunter-seven/hunter-seven.mhnative`: 19 joints, sete MODs, 20 primitivas selecionadas, 4.507 vértices fonte, 5.043 triângulos e nove imagens. Verifica matrizes mudando em passos consecutivos; pedido sobre animação já tocando com pelo menos duas queries e amostras reais; equivalência entre `tick` e `apply`+`step`; preservação de seis camadas inativas; captura sem passo; contexto de área emprestado; vida dos recursos após destruir o Actor; limites de extensão/capacidade; erros de banco; falhas tardias de hierarquia e skinning; reset e pedidos explícitos do preview.

As primeiras tentativas locais de teste omitiram o `prediction_rotation` finito na seed de inspeção. O ramo attrs10000 consumiu essa rotação e reportou quaternion de norma zero. O produtor de seed foi corrigido para identidade explícita; nenhuma tolerância ou fallback foi adicionado ao runtime. Tentativas e logs anteriores ficam preservados na pasta privada.

Os relatórios finais são `research/private/player-actor-v1/native-final/validation.json` e `native-strict/validation.json`: **1.573.865 checks, 21 passos observados e duas queries live em cada build**, normal e ASan/UBSan estrito. Oito processos negativos adicionais rejeitam constantes curtas/longas, SHA incorreto e outro perfil de ator. A validação exige `-fno-sanitize-recover=all`, stdout exato e stderr vazio, com 168 inputs fixados no normal e 164 no strict. A primeira validação completa foi corretamente rejeitada pelo guard de identidade porque o dono de input renomeou uma função durante a compilação; `native-fourth/validation-failed.json` preserva essa tentativa e os dois hashes alterados.

A regressão do inspetor anterior passou 72 clips, 864 frames, 72 transições e 151.562.680 checks. Um probe adicional comparou o código anterior preservado com o renderer compartilhado: 50 frames e 31.834.400 bytes serializados coincidiram, SHA256 `4b2f000d8cd465aa304b2a2e67e6c358f53709afa1dc78d189d5ebe3d87fcc5a`. Isso inclui metadados de frame, matrizes, posições, normais, UVs e identidade dos draws. SDL estático é fixado por SHA, mas não foi recompilado com sanitizers nesta etapa.

`render-measurements.json` registra três execuções equivalentes antes/depois. Nos dois repeats posteriores ao primeiro, o probe completo levou aproximadamente 0,175–0,176 s antes e 0,176–0,179 s depois; CPU 0,16 s e pico de RSS próximo de 112 MB em ambos. Não há alegação de ganho de desempenho. O processo nativo não cria filhos; o pico medido é desse processo. Caches do sistema não foram limpos, portanto o primeiro run não é denominado cold. Tokens da tarefa e custo do porte completo não foram medidos. O manifesto final fixa fontes/headers transitivos, recursos, backups, executáveis e relatórios.

Não está estabelecida aqui a equivalência da seed a um caçador criado pelo jogo, associação física de input, regras de combate, fiel aparência de todas as primitivas, scheduling original ou coop. A primitiva de silhueta do torso permanece no recurso; ocultá-la para inspeção requer seleção explícita do visualizador.
