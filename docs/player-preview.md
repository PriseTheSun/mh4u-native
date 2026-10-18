# Player no visualizador nativo

O perfil `--player-preview` liga o controlador persistente ao caçador de sete
peças: pedido de estado → seleção e transição → passo de animação → matrizes
dos 19 ossos → skinning → desenho. A implementação usa os recursos MOD/LMT
mantidos pelo mesmo dono imutável do carregador. Depois de criar o Player,
o visualizador libera o estado legado de reprodução do Actor.

`PlayerPreview` concentra a política de inspeção: seed explícito em pose de
referência, duas camadas ativas/oito posições, banco 0 associado ao LMT co0
por SHA-256 e demais bancos desconhecidos. Templates e delta 1 são fornecidos
por esse adapter. A factory e o reset não pedem uma ação. O visualizador,
quando habilitado explicitamente, pede o estado 1 (ou `--player-state 3`).
Isso não estabelece o bootstrap, o contexto de uma missão ou o banco co2.

Os passos consultam o estado corrente e conservam transições e snapshots.
Não há seek de clip nesse perfil. Os atalhos são:

- **Espaço:** pausar/reproduzir; **ponto:** avançar um passo.
- **1 / 3:** pedir um dos dois estados de inspeção.
- **Home:** reiniciar em referência, sem pedido automático.
- **WASD:** deslocar na vila pela física diagnóstica existente.

O pedido de estado 3 não emite um `AnimationRequest`: preserva a animação já
ligada. Se partir da referência sem ação, os passos continuam sem amostras.
Ele não recebe o significado de parada, caminhada ou outra ação de gameplay.

O ritmo de 60 passos por segundo é uma convenção do laboratório, com delta
do host limitado a 50 ms. Capturas exigem `--paused` ou `--player-ticks N`,
para que o resultado não dependa do tempo de execução da máquina.
Falhas de pedidos/passos preservam o último estado publicado e pausam a
reprodução; a mensagem fica na interface. O contador de passos só avança
após a conclusão do passo e de sua geometria.

Exemplo na raiz do projeto:

```sh
build/mh4u-resource-lab \
  --world assets/local/village-native-actor-v2.world.tsv \
  --player-preview research/private/player-transition-runtime-v1/generated/player-transition-constants.bin \
  --player-ticks 12 --hide-primitive 3
```

O pacote local inclui o atalho **Abrir controlador nativo** para Mac e
Windows. O arquivo de constantes tem 88 bytes e é validado contra o SHA
`1cf3b6e6873f77008eb072e18e27579e6bec375622c7d4780413f06f1cee3d5f`.
Não são aceitas combinações com rede, catálogo, inspeção pareada ou seek.
O teste público `tools/player_preview_smoke.py` exercita recursos reais,
passos determinísticos, mundo, conflitos e constantes alteradas/truncadas.
Seus 18 casos passaram no macOS. A captura do passo 12 e a janela real foram
verificadas; teclado confirmou passo 12→14, reinício em zero, pedido 1,
reprodução e pausa no passo 602. Esses registros estão em
`research/private/scene-validation/player-preview-v1/`. Windows foi compilado,
sem execução nessa plataforma. O pacote teve seus 296 arquivos verificados.

Esta integração não executa o interpretador completo da tabela de comandos,
não converte teclas em ações originais e não implementa combate. A cena
compartilhada anterior continua sendo um teste separado de posições/colisão
em loopback. Os [predicados](player-policy-predicate.md) e a
[seleção por área](player-selection-area.md) têm validação ARM separada;
o [contexto de inicialização](player-gameplay-context.md) ainda tem lacunas.
