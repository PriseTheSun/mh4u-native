# Consumo contínuo de sete ações

`mh4u-action-motion` combina o sampler nativo com o pré-update, a aplicação,
a previsão e o avanço de uma camada. Usa os recursos selecionados da ROM;
nenhum código ARM, Ghidra ou Unicorn é necessário para executar a ferramenta.
A CLI devolve uma sequência numérica. O mesmo Player também alimenta o
inspetor gráfico de ações, separado da cena de locomoção e rede.

O dono das curvas é `model_motion::Package`. O consumidor `action_motion::Player`
recebe sua identidade e uma política diagnóstica explícita. As sete ações
preservam a associação entre LMT, slot e índice do clip glTF. O pacote v2
verifica também os 13 arquivos associados, incluindo os buffers externos.

## Inicialização e tempo

A CLI escolhe posição zero, quaternion identidade e escalas unitárias. Usa
uma camada, flags iniciais `4/1`, gate de modelo `0x20000000`, máscaras iniciais zero,
frame inicial selecionável e um incremento declarado em frames da fonte por
tick. Esses valores definem o experimento; não recuperam o controlador do jogo.

O final é `frame_count−1`; loop bruto `−1` vira zero no ramo normal do binder.
A base é `T(end)−T(loop)` e `inverse(Q(loop))*Q(end)`, sem normalizar o produto.
A amostra anterior vem do frame inicial. Componentes ausentes continuam
ausentes no sampler e só recebem T=zero/Q=identidade no binding, conforme
[o ramo original comprovado](root-motion-defaults.md).

Reset de posição usa o template zero; reset de rotação usa a identidade em
`0x10e6560`. O quaternion zero em `0x10e6570` pertence ao fallback de inversa,
fora do perfil suportado. Não se troca um pelo outro. Valores binary64 do
sampler são estreitados uma vez para float32 antes de entrar na API de movimento.

Cada tick pede os frames necessários ao dono da lógica de movimento, amostra
apenas esses frames e carrega integralmente o estado devolvido para o tick
seguinte. Não reescreve flags entre ticks. `joint_source_frame` corresponde ao
frame efetivamente amostrado; o acumulador já avançado é outro campo.

```sh
build/mh4u-action-motion \
  --package assets/local/model-motion-native-v2/model-motion.bin \
  --paired-root assets/local/paired-player-actions-v1 \
  --action 0 --ticks 60 --step 1 --start 0 --repeat
```

No pacote distribuído localmente, ambos os caminhos de dados ficam em
`assets/paired-motion/`. A CLI limita cada pedido a 100.000 ticks e o incremento
absoluto a um intervalo da ação. As APIs subjacentes também limitam loops e
recusam entradas fora do perfil. Não há unidade de segundos ou FPS implícita.

## Comparação composta

O oracle executou 2.590 ticks originais ARM em 14 sequências: loop e parada
para cada uma das sete ações. Somente o sampler é interceptado, recebendo os
valores reais pelo decodificador proprietário já auditado. Memória, instruções
e chamadas continuam sob os guards do harness. Isso é execução isolada com
estado diagnóstico; não é uma sessão do jogo original.

O CLI nativo produziu 2.604 linhas incluindo estados iniciais. Frames, flags
e contagens de loop foram exatos; erro vetorial máximo `1,1920929e−7` e erro
de quaternion zero nos casos comparados. Passaram 14 rejeições, incluindo
pacote alterado e buffer glTF externo alterado. O maior estreitamento de
amostra binary64 para float32 foi `6,0542368e−5`, registrado separadamente.

Oracle: `research/private/root-motion-runtime/real-curves-sequences-v1/manifest.json`,
SHA-256 `f61be3310aa1f4ffc616fb517829972b7353a11eaf495bbe96577f65a402356f`.
Comparação: `real-curves-native-comparison-v2.json` no diretório pai.
O CTest `real-action-motion-sequences` repete a comparação por processo sem
executar ARM e só é registrado quando os recursos locais estão disponíveis.

Seleção de ações, despacho de eventos, transições, mistura de camadas, hitboxes, combate
e integração com colisão permanecem pendentes. Os nomes de ações não são
inferidos a partir da aparência de uma pose.

## Consultas de eventos no Player

A opção `--events-package assets/local/motion-events-native-v1/motion-events.bin`
ativa as tabelas locais. O namespace `motion_events` é o único dono do formato;
o Player verifica entry/slot/hash do LMT e frame count antes de associar a ação.
Estados rejeitados ou desconhecidos falham; ausência explícita permanece distinta
de um grupo presente com máscara zero. O pacote imutável deve sobreviver ao Player.

O relógio retorna os pedidos signed32 em `PendingTick`. O Player consulta o pacote
com `kernel_range` e só publica o próximo Frame após todas as respostas serem
aceitas por `finish()`. Falhas de leitura/identidade/domínio não atualizam `frame_`.
Isso também mantém o reset do inspetor atômico: o novo Player é construído antes
de substituir o anterior. Não há callback original, combate nem sincronização
dessas máscaras na rede.

A saída adicional `EVENTS` contém tick, quatro máscaras atuais e quatro anteriores,
como inteiros uint32 decimais. O perfil passa a `diagnostic-one-layer-events-v1`;
a ordem e os campos de `TICK` continuam iguais. Sem a opção, o perfil legado
sem tabelas é preservado. O oracle com tabelas reais está em
`research/private/root-motion-runtime/real-events-sequences-v1/` e contém
2.590 ticks originais; detalhes e limites em [root-motion-runtime.md](root-motion-runtime.md).
