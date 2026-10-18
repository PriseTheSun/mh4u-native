# Locomoção geométrica de diagnóstico

`src/locomotion.hpp/.cpp` implementa um disco horizontal apoiado num piso
contínuo, com sweep analítico e deslizamento contra paredes e bordas. O núcleo
é C++20 sem SDL, relógio, entrada de dispositivo ou estado global. Ele recebe
deslocamentos em unidades fonte por chamada. Os parâmetros e o comportamento
são escolhas explícitas do laboratório, **não física ou constantes recuperadas
de Monster Hunter 4 Ultimate**.

## Contrato aceito

- Piso estático com triângulos orientados para cima, normais Y maiores ou iguais
  a `slope_min_y`, projeções XZ sem sobreposição, sem T-junctions e sem arestas
  não manifold. Vértices XZ iguais precisam ter exatamente o mesmo Y.
- Arestas compartilhadas são identificadas pelas coordenadas exatas já
  avaliadas por `collision::World`; não há soldagem aproximada nem leitura
  alternativa de SBC/glTF. Componentes separados e buracos são permitidos.
- Paredes são pares de triângulos que cobrem retângulos verticais completos,
  com topo e base constantes. Cada retângulo precisa cobrir pelo menos o
  intervalo `[menor Y do piso − skin, maior Y do piso + height + skin]`.
  Paredes parciais, inclinadas ou com triangulação fora desse perfil são recusadas.
- O disco inteiro permanece na projeção do piso. Seu centro recebe Y da
  superfície por raycast usando o BVH existente. A altura verifica cobertura
  vertical das paredes; não transforma o disco em cápsula ou cilindro 3D.
- `max_step_height` precisa ser zero. Não há degraus descontínuos, gravidade,
  salto, teto, plataforma móvel, empurrão, aceleração ou colisão entre atores.
  Não há garantia de que uma malha visual de personagem não interpenetre um
  piso inclinado: o contrato é do centro apoiado e da projeção do disco.

Dados incompatíveis falham com `std::runtime_error` e motivo. Estados recebidos
por `step` precisam já estar apoiados e respeitar a folga; a função não
teleporta nem despenetra estados inválidos. Um chamador pode criar outro estado
explicitamente por `create_state`, que projeta XZ no piso e valida a folga.

## API e ownership

```cpp
mh4u::locomotion::Parameters parameters;
parameters.radius = 22;
parameters.height = 175;
parameters.slope_min_y = 0.8;
parameters.max_step_height = 0;

mh4u::locomotion::World world(
    mh4u::collision::load_static_gltf(floor_path),
    mh4u::collision::load_static_gltf(walls_path), parameters);
auto state = world.create_state(-61.14208710193634, 11963.744791666666);
auto result = world.step(state, {dx, dz, yaw_delta});
state = result.state;
```

`World` recebe e possui os dois `collision::World` imutáveis. `Parameters` também
é copiado; o chamador não pode alterar a geometria ou os parâmetros por alias
após a construção. A geometria permanece propriedade do carregador e avaliador
existentes; a locomoção deriva somente a topologia XZ e segmentos de barreira.

`State.position` é o centro apoiado em coordenadas fonte. `State.yaw` é um ângulo
em radianos normalizado para `[-pi,pi]`; não seleciona animações nem determina o
deslocamento. `Input{dx,dz,yaw_delta}` é explícito: velocidade, tempo, câmera e
mapeamento de joystick pertencem ao chamador. Um deslocamento acima de
`max_displacement` é rejeitado, sem truncamento silencioso.

`StepResult` informa deslocamento efetivamente aplicado, estado final, face de
suporte, contatos com identidade da geometria fonte, quantidade de segmentos
examinados e flags `blocked`/`contact_limit_reached`. O ID da parede refere-se
ao primeiro triângulo do par; o ID de uma borda refere-se à sua face de piso.
Os IDs conservam a ordem de `collision::World`, não a ordem do BVH. O yaw pode
mudar mesmo quando o deslocamento é bloqueado.

## Percurso e limites de trabalho

Cada segmento é expandido por `radius + skin`. O raio do centro é intersectado
com os dois lados paralelos e os círculos das extremidades, considerando todo
o intervalo do deslocamento. Isso detecta obstáculos finos e buracos mesmo
quando o destino teria apoio; não depende de dividir um `dt` em amostras.
O avanço termina uma `skin` antes do primeiro contato. A componente restante
que entra na normal de contato é retirada e o restante é varrido novamente,
permitindo deslizamento. Empates usam a ordem estável dos segmentos.

Após `max_contacts`, qualquer movimento restante é descartado e o resultado
expõe `contact_limit_reached=true`. Quinas podem, portanto, parar o ator antes
do deslocamento solicitado; não há tentativa ilimitada de resolver contatos.
O apoio final e a distância a todas as barreiras são novamente conferidos.
A tolerância absoluta usada para verificações de fronteira é `1e-9` unidade.

Limites explícitos deste perfil:

| Item | Limite |
| --- | --- |
| Faces de piso | 1 a 2.048 |
| Faces de parede | 0 a 2.048 |
| Coordenadas absolutas | até 1.000.000 unidades |
| Comprimento projetado de aresta | maior que `1e-6` unidade |
| `skin` | `1e-6` a 1 unidade |
| Raio | maior que `8 * skin`, até 10.000 unidades |
| Altura | maior que zero, até 10.000 unidades |
| `slope_min_y` | 0,01 a 1 |
| Deslocamento por chamada | até `max_displacement`, máximo configurável 1.000.000 |
| Contatos por chamada | 1 a 16; padrão 8 |

A construção verifica pares de faces de piso em O(F²), limitada pelo teto de
faces. As tabelas temporárias são lineares na geometria. Cada iteração de
movimento examina os segmentos de barreira; não há crescimento de fila ou
alocação proporcional à distância. O resultado reserva 16 contatos em array.
O orçamento prévio do `collision::World` continua responsável pelo armazenamento
da geometria e do BVH antes da construção da locomoção.

## Verificação executada

`src/locomotion_tests.cpp` cobre:

- Sweep rápido através de parede, deslizamento, círculo da extremidade,
  movimento para longe da parede e concordância de um passo longo com passos
  curtos numa parede reta.
- Quina de duas paredes e interrupção por orçamento de contatos.
- Borda de piso e buraco entre origem e destino apoiados; acompanhamento da
  altura de um plano inclinado; preservação de identidade da fonte.
- Rejeições de NaN, movimento excessivo, estado fora do piso/folga, altura sem
  cobertura, degraus, parede incompleta/inclinada, faces duplicadas ou
  sobrepostas, overhang, T-junctions, inclinação e quantidade de faces.

O binário de teste foi compilado no macOS arm64 com C++20, `-O3`,
`-Wall -Wextra -Wpedantic`, sem avisos, ligado às bibliotecas existentes do
projeto. A execução real leu:

- `assets/local/collision-probe/v00a01-core2/scene.gltf`: 86 faces e oito bordas.
- `assets/local/stage-composition-probe/v00a01-walls/scene.gltf`: 90 faces,
  reconhecidas como 45 retângulos verticais.

Raio 22, altura 175 e `slope_min_y=0.8` passaram na geometria. O centroide
selecionado em `docs/stage-composition.md` retornou apoio na face 30 e Y
`−17.2122446696`. Esse ponto é um início geométrico de diagnóstico, não o spawn
original. Foram bloqueados os 64 sweeps radiais de 4.000 unidades; 1.000 passos
contínuos adicionais concluíram com apoio e folga válidos. Nessa execução,
foram examinados 72.557 segmentos em aproximadamente 2,84 ms de movimento;
carga e construção levaram aproximadamente 4,03 ms. É uma amostra local quente,
sem medição de CPU/memória ou comparação de versões; não é benchmark geral.

Logs, hashes e resumo estão em `research/private/locomotion-runtime/`.
Não foi testado controle de personagem ou gameplay original nesta etapa.
Nenhuma janela persistente, recurso congelado, pacote ou manifesto foi alterado.
