# Caçador MOD/LMT na cena controlável

O preset `village-native-actor-v2.world.tsv` usa o caçador nativo de sete peças
na vila e na sessão local compartilhada. Geometria, skinning, pose e transições
do caçador passam pelo mesmo `native_actor::Actor` usado no inspetor. O cenário
e a colisão ainda usam os recursos glTF extraídos anteriormente.

```sh
python3 tools/launch_lab.py --world --hide-primitive 3
```

WASD/analógico esquerdo movem o personagem, e o mouse/analógico direito orbitam
a câmera. A primitiva 3 fica oculta por escolha de inspeção. O seletor de ações
continua manual e independente do deslocamento. A rede mantém pose estática;
não sincroniza animações, combate ou missões. O transporte continua TCP local,
sem acesso pela internet. O controlador original ainda não dirige essa cena.

## Identidade e responsabilidade

`world_config` continua sendo o dono da receita do mundo. `Actor::load` continua
sendo o único parser de produção da receita nativa e valida MOD, LMT, texturas,
snapshots e referências. O ator expõe a lista imutável dos arquivos que
efetivamente consumiu. O inventário de rede usa essa lista e os nomes ordenados
de ações; não mantém outro parser da receita nativa.

A identidade inclui 34 arquivos, 1.524.310 bytes e 44 ações: receita do mundo,
cenário, piso, paredes, receita do ator e suas dependências. O perfil de física
e protocolo permanece igual; o digest distingue os novos recursos. Carregamento
e verificação são locais e não criam contexto gráfico. As duas verificações
de startup existentes detectam alterações entre inventário e carregamento;
não são um observador contínuo de arquivos.

A câmera acompanha a posição mundial do caçador. A atualização da pose nativa
não substitui esse alvo pelo centro da malha em coordenadas locais. O alinhamento
visual de 2,09622 unidades é uma escolha diagnóstica baseada no mínimo Y de
clip0/tick0, não uma regra original de contato dos pés. O preset anterior e sua
proveniência foram preservados.

## Constantes de transição

As receitas agora usam a saída de 24 bytes do inicializador ARM original
`d3f6fc`, executado isoladamente com três padrões anteriores de memória:
`[0,1]`, `[0x3eaaaa3b,0x3f2aaa3b]` e `[0,0]` nas três tabelas.
O SHA-256 é
`283bcb5502bd9e7c2daf2245fa36170f8f1f838accd1ed146ff3ba0f3e03b922`.
Isso substitui o snapshot anterior do ELF, anterior à execução do inicializador.
O [relatório do bootstrap](player-gameplay-bootstrap.md) liga a função à tabela
de startup e delimita a execução. Os arquivos anteriores e as receitas foram preservados como evidência. Não se
trata de captura de uma partida nem de prova de ausência de escritores futuros.

## Evidência local

`research/private/scene-validation/native-world-v2/` retém as verificações da
CLI, captura da cena, execução de rede, instalador idempotente das constantes,
backups e medições. O teste de inventário compara a CLI C++ a um inventário
Python independente; arquivos adulterados ou ausentes, travessia de caminho,
registros duplicados e desconhecidos são rejeitados. Uma alteração válida de
snapshot com novo hash produz uma identidade diferente.

As medições de `verify --world` foram feitas em seis processos novos por caso,
sem limpar o cache do sistema. Medianas:

| Caso | Tempo | CPU | Pico RSS |
| --- | ---: | ---: | ---: |
| Binário anterior, preset anterior | 35,14 ms | 33,93 ms | 13,17 MiB |
| Binário atual, preset anterior | 36,01 ms | 34,82 ms | 13,10 MiB |
| Binário atual, ator nativo | 36,12 ms | 34,79 ms | 13,70 MiB |

As duas primeiras linhas comparam o mesmo trabalho. A terceira usa recursos
diferentes. São custos da CLI de identidade, sem processos filhos; não medem
renderização interativa, tempo frio de disco, custo de tokens ou Windows.
