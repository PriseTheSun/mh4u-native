# Mundo diagnóstico com autoridade no servidor

`mh4u-network-lab --world` acrescenta movimento geométrico a uma sessão local.
Clientes enviam direção e giro; **somente o servidor avança o relógio de
simulação, calcula posições e resolve piso/paredes**. Não há mensagem que aceite
posição ou `dt` do cliente. O modo de seleção de animação com `--manifest`
continua usando o protocolo v1 descrito em `network-foundation.md`.

Este é um fundamento para integrar clientes ao servidor próprio. O cenário,
as colisões e o ator vêm dos recursos locais já investigados; a frequência,
velocidade, raio, altura e regras de movimento são parâmetros do laboratório.
Não são física, cooperação ou protocolo online originais recuperados do jogo.
Não há combate, missão, inventário, colisão entre jogadores, relay, contas,
Internet ou interoperabilidade com 3DS/emuladores nesta implementação.

## Usar a CLI

```sh
build/mh4u-network-lab verify \
  --world assets/local/village-diagnostic-v1.world.tsv

build/mh4u-network-lab server --port 36440 --run-ms 15000 \
  --world assets/local/village-diagnostic-v1.world.tsv

build/mh4u-network-lab client --port 36440 --name um \
  --world assets/local/village-diagnostic-v1.world.tsv \
  --move-x 1000 --run-ms 5000

build/mh4u-network-lab client --port 36440 --name dois \
  --world assets/local/village-diagnostic-v1.world.tsv \
  --move-z 1000 --run-ms 5000
```

Os comandos de cliente devem ser executados em processos separados. Somente
`127.0.0.1` é usado. `--port 0` escolhe uma porta efêmera no servidor, publicada
na linha `READY`. No Windows, o binário tem extensão `.exe`.

`--move-x`, `--move-z` e `--turn` são inteiros em `[-1000,1000]`; XZ também
precisa satisfazer `x²+z² <= 1000000`. A CLI repete essa intenção a cada 100 ms.
`--move-after-ms` adia o início e `--move-ms` limita a duração; ao terminar a
fase de movimento, envia uma intenção zero. `--run-ms` encerra a conexão por
Bye. `--select` conserva a seleção compartilhada de uma pose de inspeção;
não sincroniza tempo, eventos ou transições de uma animação. O mundo exige
ao menos um clip e inicia na pose de inspeção do clip 0, coerente com o ground
offset dessa receita. Isso não identifica uma ação original como idle. O modo
v1 continua iniciando na pose de referência `65535`.

## Identidade e fontes de autoridade

`WorldConfig` permanece o dono do parser da receita. `src/net/content.cpp`
é o dono da descoberta das dependências glTF e de seus hashes; o caminho v1 e
o caminho de mundo usam o mesmo descritor. `src/net/world.cpp` recebe os fatos
da receita e constrói `locomotion::World` sobre os dois `collision::World`
avaliados pelo carregador existente. Não há um segundo parser SBC ou glTF no
servidor de movimento.

Antes de abrir sockets, a receita e os quatro papéis `stage`, `actor`, `floor`
e `walls` são inventariados. O texto canônico de identidade contém:

- Versão de identidade e perfil `disk-floor-v1`, incluindo protocolo 2,
  pose inicial clip 0, tick 50 ms, retenção 250 ms, escala de intenção 1000, giro máximo pi rad/s,
  skin 0,001, deslocamento máximo 4096 e oito contatos por passo.
- Nome relativo e bytes da receita, que contêm spawn diagnóstico, velocidade,
  raio, altura, inclinação e parâmetros de apresentação.
- Caminhos dos quatro glTFs, ordem e nomes dos clips do ator.
- Lista única ordenada de todos os arquivos, com tamanho e SHA256: receita,
  glTFs, buffers e imagens referenciados. Caminhos precisam ficar dentro da
  pasta da receita; referências de rede/data e escapes por symlink são recusados.

SHA256 desse texto é o digest negociado. Cada processo o calcula a partir dos
arquivos locais; não recebe um ID arbitrário por argumento. O servidor e a CLI
carregam a geometria, recalculam o inventário e releem os fatos da receita antes
de entrar em rede, recusando divergências durante essa preparação. A geometria
carregada passa a ser propriedade privada do motor. Isso verifica consistência
na carga; não é proteção contra um escritor local hostil, observação contínua
do filesystem ou autenticação remota do cliente. Recursos não são transferidos
pela rede. Um pacote cosmético aplicado somente em memória não muda o asset-base
pactuado; editar os arquivos-base muda o digest.

`tools/network_world_identity.py` produz independentemente o mesmo inventário
com `hashlib`, reutilizando o descritor Python do manifesto de cena. Por padrão
apenas imprime o digest; `--output` grava o texto em um arquivo novo. A receita
real examinada contém 26 arquivos, 2.626.069 bytes e 44 clips. Cópias do mesmo
conteúdo sob a mesma organização relativa têm a mesma identidade; caminhos ou
bytes diferentes são incompatíveis mesmo que pareçam visualmente equivalentes.

## Simulação e limites

O servidor avança ticks de 50 ms (20 Hz), sem depender da frequência de envio.
A cada tick, a intenção vigente produz:

```text
dx = speed_da_receita * 0.05 * x / 1000
dz = speed_da_receita * 0.05 * z / 1000
delta_yaw = pi * 0.05 * turn / 1000
```

`NetworkWorld::tick` entrega esse deslocamento ao contrato geométrico descrito
em `locomotion-runtime.md`. O cenário atual aceita raio 22, altura 175 e normal
Y mínima 0,8. Todos os participantes entram no mesmo ponto de diagnóstico,
com apoio na face 30. Não há spawn diferente por jogador nem colisão entre eles.

Cada participante possui uma única intenção substituível. Sequências precisam
ser contíguas desde 1; uma intenção nova pode substituir a anterior antes de
ela produzir movimento. `accepted` informa a última recebida, enquanto
`applied` informa a última consumida por um tick, inclusive se já expirada e
substituída por zero. Após 250 ms sem intenção nova, a entrada vira zero; o
heartbeat mantém a conexão viva, mas não mantém o movimento. Ao iniciar saída
ou rejeição, o participante deixa de ser simulado e é removido do estado após
a finalização limitada do socket.

O servidor admite até quatro membros, oito conexões ativas/pendentes, 40
intenções de movimento por janela de um segundo e 128 frames totais por segundo
por conexão. Cada peer processa até 32 frames por iteração. As filas são de
8 KiB por direção, o payload máximo é 512 bytes e a cota acumulada continua
16 MiB por direção. Leitores lentos são removidos por limite de fila; não
bloqueiam os demais sockets. As cotas acumuladas também limitam a duração de
uma sessão longa, especialmente quando há quatro participantes.

Os IDs são atribuídos quando o `Hello` é aceito, não quando a conexão TCP é
aberta. A montagem do snapshot ordena uma visão dos membros por ID, compartilhada
pelos protocolos v1 e v2. A coleção de conexões conserva sua ordem para os
índices do polling. Isso corrige a rejeição de participantes quando uma conexão
mais antiga demora a enviar `Hello` e outra se identifica primeiro. Os leitores
continuam exigindo IDs estritamente crescentes; não foi relaxada a validação.

`tools/network_membership_smoke.py` força essa ordem invertida com dois peers
reais: A conecta, B recebe seu `Welcome`, e só então A envia `Hello`. O teste
verifica a identidade dos IDs e a ordem dos snapshots nos dois protocolos.

Há no máximo quatro ticks de recuperação por iteração. Se o servidor continuar
atrasado, registra `WORLD_LAG`, descarta o excesso de tempo de parede e retoma
com passos fixos; não amplia o `dt` nem faz recuperação ilimitada. É uma política
de degradação do laboratório, sem compensação de latência ou determinismo entre
plataformas declarado. A simulação limita trabalho por contatos e segmentos
conforme o perfil geométrico. O inventário limita 256 arquivos, 512 MiB de
conteúdo, 8 MiB por glTF e 64 KiB de receita.

## Wire protocol v2

O envelope mantém magic `M4NL`, inteiros em ordem de rede, flags zero e formato
v1; somente o campo de versão é `2`. Um endpoint v1 recusa v2 e vice-versa.
Hello, Intent de seleção, Ping, Pong, Bye e Reject mantêm seus payloads.

| Tipo | Mudança em v2 |
| --- | --- |
| 1 Hello | digest do mundo `[32]`, nome como string uint8 |
| 2 Welcome | payload v1 mais tick_ms:uint16 e hold_ms:uint16; total 50 bytes |
| 3 State | snapshot de mundo descrito abaixo |
| 9 Motion | sequência:uint64, x:int16, z:int16, turn:int16; total 14 bytes |

Os int16 usam complemento de dois. Não há timestamp, duração, posição, estado
físico nem campo extra permitido em Motion. A seleção e o movimento têm
sequências independentes. Salto, repetição e overflow são rejeitados.

State começa com sessão:uint64, digest[32], tick:uint64, revisão:uint64,
seleção:uint16 e quantidade:uint8. Cada participante, em ID crescente, contém:

```text
id:uint32, nome:string(uint8), seleção_confirmada:uint64,
accepted:uint64, applied:uint64,
x:float64, y:float64, z:float64, yaw:float64, flags:uint8
```

Float64 usa os bits IEEE754 em ordem de rede; valores não finitos são recusados.
Flags bit 0 indica bloqueio geométrico no último tick e bit 1 entrada expirada.
Outros bits são inválidos. A revisão cresce em adesões, saídas, seleção e ticks
com participantes; tick não retrocede e pode permanecer igual entre dois
snapshots de adesão/seleção. A revisão ordena também esses estados do mesmo tick.
Cada snapshot repete sessão e digest. Com quatro nomes de 24 bytes, seu payload
tem 403 bytes. O cliente verifica identidade, ordem, limites, presença do próprio
ID e monotonicidade das confirmações; não trata uma posição recebida como
comando a reenviar ao servidor.

## Cliente para integração nativa

`src/net/world_client.hpp` expõe `WorldClient`, usando o mesmo `Peer` e framing
da CLI. `WorldSnapshot` e os codecs públicos ficam em `world.hpp/.cpp`.
Não é necessário iniciar subprocesso ou interpretar stdout:

```cpp
mh4u::net::SocketSystem socket_system; // Deve sobreviver ao cliente.
mh4u::net::WorldClient client(port, name, verified_content);
client.pump(); // Sem espera: select timeout zero, um read/write, até 32 frames.
if (client.ready()) client.motion({x, z, turn});
if (client.snapshot()) consume(*client.snapshot());
// Ao fechar: client.close(); continuar pump até done().
```

A conexão também começa sem bloqueio; o deadline de handshake inclui essa fase.
O cliente conserva apenas o snapshot completo mais recente. `error()` informa
falhas e `confirmed_bye()` diferencia encerramento confirmado. `close()` inicia
Bye e prazo de 500 ms, inclusive durante um handshake tardio. Destruição sem
esse ciclo fecha o socket por RAII e resulta em desconexão abrupta observada pelo
servidor; não implica confirmação. O chamador deve bombear regularmente a rede.

## Verificação executada e limites restantes

Os scripts públicos criam somente processos/sockets próprios, coletam todos os
filhos e preservam logs em uma pasta nova quando `--output` é fornecido:

```sh
python3 -B tools/network_world_smoke.py \
  --binary build/mh4u-network-lab \
  --world assets/local/village-diagnostic-v1.world.tsv
python3 -B tools/network_world_client_smoke.py \
  --binary build/mh4u-network-lab \
  --world assets/local/village-diagnostic-v1.world.tsv
python3 -B tools/network_smoke.py --binary build/mh4u-network-lab
```

A execução final macOS de mundo confirmou 73 snapshots idênticos recebidos
por dois clientes nativos, deslocamento real e bloqueio por paredes, remoção do
membro desconectado, velocidade limitada por tick, expiração de entrada,
capacidade, identidade por textura/parâmetro e rejeições de mensagens inválidas.
A regressão v1 passou incluindo framing, filas, peers lentos, sinais e limpeza.
Os resultados finais estão em `initial-pose-process/validation.json` e
`initial-pose-client/validation.json`, sob `research/private/network-world/`.
O digest do mundo nessa revisão é
`70095bbf53ec60472939f45dfe04d0f0976f4ac6f8dadd7602de38551cfd5b0e`.

O teste do consumidor passou 12 casos de servidor inválido (identidade, ordem,
não finitos, flags, membro ausente, confirmação indevida, versão, limite de frame
e deadlines) e SIGTERM durante um handshake tardio com Bye confirmado. O primeiro
fixture de frame parcial expirou corretamente por deadline de handshake porque
ainda não havia Welcome; o fixture foi corrigido para testar frame parcial após
Welcome, sem alteração do runtime por esse resultado. Os relatórios em
`research/private/network-world/` conservam ambas as tentativas e o resultado
final, sem afirmar execução Windows.

Dois processos `verify` adicionais recusaram ator sem clips e inventário
expandido acima de 1 MiB. O limite de texto é conferido antes de cada acréscimo;
o digest válido permaneceu idêntico. Build macOS arm64 com C++20 e
`-Wall -Wextra -Wpedantic` concluiu sem avisos. CPU, pico de memória e latência
de rede não foram medidos nesta etapa.

Esses testes são TCP em loopback. Não estabelecem comportamento em LAN/Internet,
latência adequada a combate, interoperabilidade ou cooperação jogável. A etapa
não controla janelas persistentes nem altera os recursos, pacotes ou o protocolo
original do jogo.
