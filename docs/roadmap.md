# Escopo e critérios do porte

O projeto busca reimplementar o comportamento do jogo usando os recursos da
ROM do usuário. Os bytes de código ARM servem para investigação; o executável
nativo deverá conter a nova implementação. Uma build de visualizador ou uma
função decompilada não comprova equivalência de gameplay.

| Requisito | Estado deste marco | Critério para considerar concluído |
| --- | --- | --- |
| macOS nativo | Cena C++ arm64 com caçador controlável, câmera e colisão de diagnóstico; movimento por teclado observado | Fluxo completo de jogo, save e missões validado no Mac |
| Windows nativo | Executável x64 compilado; sem teste de execução | Mesmo fluxo validado em Windows e comparação entre plataformas |
| Recursos originais | Onze opções, duas com reprodução MOD/LMT direta, e cena combinada com piso/paredes; aparência, spawn e composição completa pendentes. Sete ações consumidas em C++ e comparadas a 2.590 ticks ARM isolados; pose, deslocamento e máscaras de eventos compostos no inspetor, ainda sem controlador original | Geometria, materiais, animações, cenários, áudio e interfaces necessários carregados e comparados |
| PT-BR | 42.196 fragmentos catalogados; catálogo nativo de 1.082 fragmentos com 925 traduções experimentais e 157 fallbacks em dez documentos. Renderização de fragmentos validada; comandos e layout dos menus pendentes | Catálogo completo revisado, fontes com acentos, layout e variáveis testados em execução |
| Seletor de texturas | Original/pack PNG por hash/sem textura; fallback e render dos modos verificados. Cliques automatizados deste marco não concluíram a alternância | Packs identificados por recurso, fallback, persistência e troca funcionando durante jogo |
| Joystick | Eixos de movimento/câmera, deadzone e remapeamento implementados; sem controle físico disponível | Dispositivos reais, hotplug, gameplay e persistência testados nas duas plataformas |
| Online | Duas janelas nativas compartilham movimento e colisão autoritativos em loopback; pose estática, sem combate | Dois PCs em redes distintas completando uma missão, com desconexão e reconexão verificadas |
| Compatibilidade com 3DS | Adiada pelo usuário | Fora do critério atual de conclusão |

## Sequência de implementação

A política de ações já compõe predicados, tempo e transição. A
[implementação nativa do estado1](player-state1-native.md) acrescenta quatro
entradas ligadas ao comando de animação e ao Player, com 384 casos ARM
comparados. As continuações dos IDs1,4,9 passaram 235 comparações ARM e a
composição com pose real; o [ID2](player-state1-id2-native.md) acrescenta252
comparações de custo, posição e contexto explícito. A consulta espacial sem
cache continua em construção.
Sua [interseção vertical nativa](spatial-triangle.md) já coincide com1.464
consultas preservadas e102 hits; a enumeração de objetos/grupos e a escolha
do resultado continuam separadas. A [consulta original sobre GRCO](player-spatial-context-v4.md)
passou223 casos, com122 hits e101 misses, usando a grade real e contexto
explícito. Ela fornece a próxima referência para busca e seleção nativas.
A [ponte PBW/modelo](player-position-bridge.md) passou150 execuções originais
com animação e publicação de juntas; sua versão nativa ainda está pendente.
Scheduler e bootstrap permanecem necessários antes
de ligar esse fluxo aos controles de gameplay.

1. Usar REA para identificar carregamento de recursos e seus consumidores.
   Registrar endereços, Evidence, limites da descompilação e campos desconhecidos.
   MOD230 esquelético, LMT67, SBC e LMD têm subconjuntos validados. Ampliar variantes
   antes de generalizar; índice completo e parsers preservam a proveniência.
2. Construir uma cena com um caçador animado, câmera, terreno e colisão.
   A integração de diagnóstico está disponível no marco 0.7, com importação,
   render, entrada e locomoção separados. Falta recuperar o controlador original,
   integrar a seleção de ações e comparar poses e tempos com uma referência
   observável antes de afirmar fidelidade. A [composição de sete ações](action-motion-viewer.md)
   já funciona no inspetor. A [cadeia de seleção](player-action-controller.md) e os
   [eventos LMT](native-motion-events.md) já permitem consultas nativas; despacho de gameplay pendente.
   Os [bancos de movimento](player-motion-banks.md) têm recortes estáticos conferidos.
   Os [classificadores nativos de entrada](player-input-runtime.md) foram comparados
   com 131.980 execuções ARM isoladas. A [produção dessas entradas](player-input-production.md)
   agora inclui [conversão numérica executada](input-quantization.md), e a
   [orientação de referência](player-reference.md) reproduz o cálculo usado
   pelo classificador relativo. Um [perfil nativo da transição](player-transition-runtime.md)
   preserva efeitos e gates delimitados. A política completa, aquisição de
   dispositivos e seleção automática de ações continuam em investigação.
   Os [filtros modos 0/1](input-filter-runtime.md) já alimentam a quantização
   nos ensaios. A [seleção em duas fases](player-selection-runtime.md) conserva
   a leitura de estado após o pedido, e o [setter de animação](motion-request-runtime.md)
   reproduz a inicialização de frames/eventos. A [composição do controlador](player-controller-step-runtime.md)
   já executa seleção → [comando](motion-command-runtime.md) → transição no
   perfil validado. A [preparação do binder](motion-binding-runtime.md) liga
   tracks aos joints e compõe a [inicialização completa](motion-initialization-runtime.md).
   O [dono dos LMT](motion-resource-runtime.md) fornece os mesmos dados ao setter,
   binder e [sampler nativo](motion-sampler-runtime.md), sem nova interpretação
   pelos consumidores. O [consumo das camadas pendentes](motion-pending-runtime.md)
   e a [restauração dos joints](joint-pose-runtime.md) estão implementados e
   comparados a 1.664 e 931 casos ARM, respectivamente. As [duas folhas de
   atualização dos ossos](joint-update-runtime.md) e a [consulta de movimento
   do modelo](motion-query-runtime.md) também estão em C++, comparadas a 862
   casos e 5.002 consultas ARM em domínios explícitos. A [mistura dos
   snapshots](motion-transition-runtime.md) e a [construção das matrizes
   hierárquicas](joint-output-runtime.md) também estão em C++; sua sequência
   foi comparada com 401 cenários ARM, incluindo duas rejeições por norma
   zero. O [leitor nativo de MOD](model-resource-runtime.md) e a
   [preparação das paletas](joint-palette-runtime.md) agora fornecem matrizes
   e planos de escritas/binding em C++, preservando o dono das matrizes dos ossos.
   O [contrato original](joint-render-binding.md) inclui dois recursos reais;
   a [seleção condicional do VS e seu consumo de índices/pesos](joint-shader-consumption.md)
   estão documentados, com o loop ARM de upload validado isoladamente.
   Os vértices raw e as paletas já alimentam o renderer CPU. Faltam a câmera
   original e equivalência da precisão gráfica/GPU, incluindo o padding
   de paletas ímpares.
   Faltam também os callbacks especiais e o bootstrap; a composição numérica
   do prefixo com o relógio já foi implementada. Permanecem
   os wrappers do caçador e a sequência completa.
   A [ordem condicional do scheduler](player-input-scheduler-quest.md)
   agora fecha pad → gerente de áreas/aQuest → sPlayer → uPlayer por análise
   estática; uma passagem inteira e o grupo ativo do caçador continuam pendentes.
   A [primeira consulta da tabela de eventos](motion-event-dispatch.md)
   também foi reproduzida, sem disparar efeitos de gameplay.
   Veja também [a cena](controllable-scene.md).
3. Reproduzir um conjunto fechado de movimentos, um monstro e uma missão.
   Cobrir estados, hitboxes, dano, itens e condições de conclusão. Implementar
   save versionado, sem sobrescrever saves externos.
4. Integrar o catálogo de tradução no motor e packs de textura por identidade
   de recurso. Testar fontes, quebras, placeholders e fallback.
5. Implementar coop do novo motor. A direção proposta é simulação autoritativa
   no host/servidor, com clientes enviando intenções, já implementada para movimento no marco 0.8. Definir autoridade sobre
   combate, inventário e missão antes de sincronizar estados. Validar framing,
   limites, versão de protocolo, perda de conexão e atrasos. A fundação de duas
   instâncias locais com janelas está verificada em [network-viewer.md](network-viewer.md);
   a versão 0.10.1 corrige a ordenação dos participantes na entrada da sala,
   com regressão reproduzida antes e depois nos dois protocolos;
   faltam integração jogável, LAN e duas redes externas. Escolher hospedagem ou
   relay quando existir uma sessão jogável a transportar.
6. Empacotar e validar o mesmo conteúdo em macOS e Windows; testar desempenho,
   controles físicos, instalação, saves e retomada de sessões.

Online próprio é independente da disponibilidade de servidores oficiais ou de
Pretendo. Um lobby que conecta não basta para afirmar que o multiplayer do
jogo funciona. Não se promete prazo de conclusão a partir deste
subconjunto: a quantidade de comportamento ainda não reconstruído é grande.

A reprodução direta de MOD/LMT está integrada no marco0.12.0: veja
[native-actor-rendering.md](native-actor-rendering.md). Isso conecta modelos
reais ao render com preparação, movimento global, relógio, eventos e
transição na ordem recuperada. Faltam bootstrap, gates e ritmo do scheduler,
controlador jogável e sincronização das ações. A recuperação da
[projeção original](model-view-projection.md) não substitui a câmera de inspeção
no executável atual.

Marco0.13.0: o seletor nativo agora executa transições e troca de bancos sobre
o estado canônico; os ossos recebem metadados e ordem recuperados da fonte.
A política de input→seleção e o bootstrap completo do jogador seguem pendentes.
