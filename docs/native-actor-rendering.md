# Reprodução nativa de MOD/LMT no laboratório

O marco 0.13.0 conecta a geometria da ROM ao renderizador sem converter malhas,
pesos, esqueletos ou curvas para glTF. Há duas receitas locais selecionadas:
em062 (12 juntas, 1.516 vértices, 1.564 triângulos, 28 ações) e caçador de sete
peças (19 juntas-base, 4.507 vértices, 5.043 triângulos, 44 ações).

```sh
python3 tools/launch_lab.py --model native-hunter --hide-primitive 3
python3 tools/launch_lab.py --model native-actor
```

A primitiva3 oculta é uma escolha de inspeção. A receita conserva todas as
primitivas selecionadas e permite alterná-las na interface; não recupera a
seleção original de aparência.

## Donos e composição

`native_actor::Actor` carrega uma receita `.mhnative` com caminhos locais
confinados e SHA-256 explícitos. Os donos imutáveis `model_resource` e
`motion_resource` fornecem os fatos parseados. A reprodução compõe pedido,
troca pendente, preparação dos frames, movimento do modelo, atualização dos
joints, relógio, transição, hierarquia, paleta e skinning. A sequência9573b8 é
composta em `model_animation_frame`;95b490 e95b940 seguem a ordem observada
no caller. Os eventos ficam disponíveis como máscaras, sem gameplay associado.
As peças usam a matriz B própria e a matriz W da junta-base correspondente.
O renderer recebe apenas os triângulos avaliados e materiais/imagens de
apresentação. O `Scene` de apresentação não contém nós, skins, curvas ou
vértices convertidos. O leitor glTF permanece para os outros recursos antigos.

As paletas reutilizam o resultado por peça/paleta dentro do frame. Os cursores
persistem com o estado completo da animação na mesma linha de tempo.
`evaluate` usa ticks inteiros a partir do tempo decorrido e executa um passo
com delta1 por tick. O tick0 prepara e mostra o frame0 antes do primeiro avanço
do relógio. Uma consulta repetida não avança o tempo; retorno no tempo ou mudança
do modo de loop reconstroem o estado desde a referência. `seek` sempre reconstrói
uma ação isolada, inclusive ao buscar um instante posterior. O slider usa essa API.
`change_action` executa o comando original sobre o estado anterior e depois o frame0
da nova ação; a reprodução seguinte conserva a transição. A seleção do menu usa
essa API, com interpolação solicitada de4 unidades da fonte (o wrapper acrescenta2).
A duração é política do caller, não decisão recuperada do controlador.
Cada chamada admite até4096 ticks adicionais; acima disso retorna erro antes
de publicar mudanças. O avanço em loop usa o intervalo do LMT, sem fmod do
visualizador. A opção de loop permite o comportamento da ação; ações cujo
header exige parada continuam parando. Uma falha de
amostragem, hierarquia, paleta ou vértice não publica estado parcial.

## Políticas explícitas e limites

- 60 ticks por segundo e delta1 por tick são políticas de inspeção. O
  algoritmo numérico do relógio original está conectado; o scheduler,
  bootstrap e gates do controlador ainda não foram reconstruídos.
- O seletor executa `motion_command::restart`, incluindo consulta do movimento
  anterior, cursores e snapshots de mistura. Quando a ação pertence a outro LMT,
  `motion_bank::replace` instala o recurso no banco0 e marca as camadas afetadas
  antes do comando. Essa escolha explícita de banco serve à inspeção; o carregador
  de gameplay e a política de dispositivos ainda não estão integrados.
- `Data` conserva todos os LMT imutáveis, incluindo os recursos de referências
  antigas. Bancos, canais e eventos usam identidades lógicas, sem ponteiros ARM.
  A receita `commands` aponta um snapshot SHA de24 bytes das tabelas
  em104e048..104e05f. A atualização de [integração na vila](native-world.md)
  usa a saída observada do inicializador original `d3f6fc`, substituindo os
  bytes anteriores do ELF. Não é uma captura dos globals durante uma partida.
- A montagem dos ossos usa `joint_bootstrap`: T dos registros MOD, Q da matrizA,
  escala1, boneID/parent/mapping e contagem de filhos da fonte, com a ordem de
  profundidade original. Callback nulo e owner self são efeitos comprovados desse
  trecho. Bits anteriores de mapping98 não escritos recebem zero explicitamente
  da alocação gerenciada host. Modelo identidade, uma camada ativa e demais flags
  iniciais continuam políticas de inspeção; o construtor completo não está integrado.
  Compatibilidade entre peças exige erro máximo absoluto0,001 nas matrizes de
  referência; é tolerância de inspeção.
- A seleção RT/RTS segue95b790, com flags e gates próprios.95cab0 aplica os
  canais globais do modelo. A saída recebe as flags completas; callbacks
  especiais ainda não suportados retornam erro, sem mascarar flags. As72
  ações selecionadas passaram no perfil normal.
- O inspetor anterior de sete ações pareadas conserva seu caminho separado;
  não herda a prova da nova composição.
- O skinning segue a álgebra recuperada do VS18: pesos brutos, inclusive zero,
  sem normalizá-los; nenhuma segunda conversão Q. A precisão é binary32 CPU
  com operações separadas e normalização por sqrt/recíproco. Não foi executada
  nem provada equivalência bit a bit com PICA MAD/DP/RSQ.
- Materiais opacos, PNGs previamente decodificados, associação explícita de
  textura, V invertido e câmera/iluminação do laboratório permanecem políticas
  de apresentação. O contrato da câmera original está em
  [model-view-projection.md](model-view-projection.md), ainda sem integração.
- Receita≤64KiB; até16 MOD,16 LMT,64 imagens,256 primitivas selecionadas e4096
  ações. Imagens têm limite agregado de128MiB decodificados. A geometria
  selecionada por frame limita100mil vértices/200mil triângulos. O carregamento
  eager do MOD tem limites próprios por recurso; esses números não são um
  limite agregado de memória do Actor. Cada MOD/LMT tem limite de64MiB.

## Verificação

As fontes de componentes e seus limites estão em
[model-resource-geometry.md](model-resource-geometry.md),
[vertex-skinning.md](vertex-skinning.md) e
[joint-palette-runtime.md](joint-palette-runtime.md).

`mh4u-native-actor-tests` exercita72 ações em864 avaliações: início, fração,
fim, retorno, tempo além do fim, loop ligado/desligado, troca de clip, cópias
de Actor e tempo inválido. Compara bits de matrizes/vértices entre avaliação
nova e estado incremental, incluindo relógio, fim e máscaras de eventos;
verifica topologia, normais, cópias, limite de replay e publicação
transacional. Acrescenta72 transições entre ações, dentro e entre famílias LMT,
continuação após a troca, cópia do estado, rejeição sem publicação e busca isolada
posterior. Exige que pelo menos uma transição conserve uma pose diferente da ação
iniciada isoladamente. Isso comprova coerência de integração, não comportamento do
jogo inteiro. O smoke da CLI testa receita válida e19 rejeições de formato,
identidade, caminho e referências. Capturas visuais do caçador e do em062 são
retidas nos relatórios de integração. O marco0.11.0 permanece em
`research/private/scene-validation/native-skinning-v1/`; o relógio integrado
é validado em `research/private/scene-validation/native-clock-v1/`. A integração
atual está em `research/private/scene-validation/native-controller-v1/`.

A composição9573b8 foi comparada em2.586 frames ARM, com122 ações LMT,
19 joints de um MOD real, 1/3 camadas, eventos, loops e gates. Ver
[model-animation-frame.md](model-animation-frame.md),
[motion-application-runtime.md](motion-application-runtime.md) e
[motion-clock-runtime.md](motion-clock-runtime.md). Essas execuções usam
estado inicial explicitamente declarado; não provam o construtor do jogo.

Combate, missões, saves, coop jogável e execução Windows permanecem pendentes.
