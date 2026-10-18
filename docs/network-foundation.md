# Fundação de sessão nativa

`mh4u-network-lab` é uma CLI C++ para verificar transporte e estado de uma sessão
de laboratório em processos separados. O servidor mantém até quatro participantes
e uma seleção de animação. Clientes enviam intenções; somente o servidor altera
esse estado e atribui sua revisão global. O visualizador permanece independente.

O transporte inicial é TCP confiável em `127.0.0.1`, implementado com sockets
POSIX no macOS e WinSock no Windows. O endereço está fixo em loopback nesta
versão. TCP ainda não foi avaliado para snapshots de combate, latência,
predição ou reconciliação. Esta sessão não contém combate, posição, missão,
inventário, autenticação, descoberta, relay ou reconexão com retomada.

## Executar

Compile o target `mh4u-network-lab`. Gere o manifesto ao lado da cena exportada:

```sh
python3 tools/network_manifest.py \
  assets/local/animation-probe/em062-actions/animated.gltf \
  --output assets/local/animation-probe/em062-actions/session.manifest

build/mh4u-network-lab verify \
  --manifest assets/local/animation-probe/em062-actions/session.manifest

build/mh4u-network-lab server --port 36440 --run-ms 15000 \
  --manifest assets/local/animation-probe/em062-actions/session.manifest
```

Em outros dois terminais:

```sh
build/mh4u-network-lab client --port 36440 --name um --select 0 --run-ms 5000 \
  --manifest assets/local/animation-probe/em062-actions/session.manifest
build/mh4u-network-lab client --port 36440 --name dois --select 1 --run-ms 5000 \
  --manifest assets/local/animation-probe/em062-actions/session.manifest
```

No Windows, use `mh4u-network-lab.exe`. `--port 0` no servidor escolhe uma porta
efêmera, publicada na linha `READY`. `--select-after-ms` adia a intenção inicial;
`--select 65535` seleciona a pose de referência. Cada cliente diagnóstico envia
no máximo uma intenção, mas o servidor admite uma sequência de intenções válidas.
Os nomes de participantes têm 1–24 caracteres ASCII alfanuméricos, `_` ou `-`.

O snapshot é ordenado por ID mesmo quando a ordem dos `Hello` difere da ordem
das conexões TCP. A projeção de membros é compartilhada com o protocolo v2;
veja a correção e o teste em [network-world.md](network-world.md).

`--run-ms` limita a duração entre 1 ms e um dia; o padrão é 5 s. Ao terminar,
os processos enviam `Bye`, drenam filas por tempo limitado e fecham os sockets.
SIGINT/SIGTERM também iniciam a finalização no ambiente POSIX. Encerramento
forçado do processo é observado pelos outros participantes como perda de conexão.
O cliente deixa de emitir intenções/heartbeat ao iniciar a finalização, envia
`Bye` após o handshake e aguarda confirmação por até 500 ms. Esse prazo também
cobre um sinal recebido durante o handshake; a falta de confirmação é um erro
de encerramento, nunca uma saída limpa reportada como confirmada.

## Identidade do conteúdo

O manifesto em UTF-8/LF contém versão, caminho da cena, quantidade e ordem dos
clips com seus nomes, além de caminho, tamanho e SHA256 de cada arquivo. A lista
de arquivos é ordenada e cobre exatamente o glTF e seus buffers e imagens
referenciados, inclusive o buffer de animação. Caminhos absolutos, `..`, URIs
de rede/data, escapes por symlink e extensões glTF obrigatórias não suportadas
são rejeitados. O gerador não substitui um manifesto diferente já existente.

Antes de abrir sockets, a CLI relê cada recurso, confere tamanho e SHA256 e usa
o parser cgltf para conferir as referências e a ordem/nome dos clips. O ID
negociado é SHA256 dos bytes do manifesto. O hash é calculado por PicoSHA2,
fixado por commit e hashes em `tools/dependencies/picosha2.lock.json`; a geração
Python usa `hashlib`, permitindo comparação independente. Os arquivos de recursos
não são enviados pela rede.

Isso verifica o conjunto local no início da execução e detecta incompatibilidade
entre clientes que seguem o protocolo. Não autentica um participante nem prova
remotamente que ele verificou ou executa esses arquivos; também não monitora
alterações de arquivo posteriores à verificação. Uma futura sessão jogável deve
definir o manifesto completo e a política de imutabilidade do conteúdo carregado.

## Protocolo de laboratório v1

Todos os inteiros são unsigned em ordem de rede. Cada frame começa por:

| Offset | Campo | Tamanho |
| --- | --- | --- |
| 0 | Magic ASCII `M4NL` | 4 bytes |
| 4 | Versão `1` | uint16 |
| 6 | Tipo | uint8 |
| 7 | Flags reservados, obrigatoriamente zero | uint8 |
| 8 | Tamanho do payload, máximo 512 | uint32 |

Strings usam comprimento uint8 seguido dos bytes, sem terminador. Não existe
padding nem payload extra permitido. Frames podem chegar fragmentados ou
coalescidos pelo TCP.

| Tipo | Direção | Payload |
| --- | --- | --- |
| 1 Hello | cliente → servidor | digest[32], nome |
| 2 Welcome | servidor → cliente | id:uint32, clips:uint16, sessão:uint64, digest[32] |
| 3 State | servidor → cliente | revisão:uint64, seleção:uint16, quantidade:uint8, participantes |
| 4 Intent | cliente → servidor | sequência do cliente:uint64, seleção:uint16 |
| 5 Ping | cliente → servidor | nonce:uint64 |
| 6 Pong | servidor → cliente | mesmo nonce:uint64 |
| 7 Bye | ambas | vazio |
| 8 Reject | servidor → cliente | motivo textual limitado |

Cada participante de `State` contém id:uint32, nome e última sequência aceita:uint64.
A lista segue IDs crescentes. IDs não são reutilizados durante uma sessão.
A revisão cresce com adesão, intenção aceita e saída; clientes rejeitam revisões
repetidas ou regressivas. A sequência por cliente começa em 1 e deve ser
exatamente a anterior mais um. Repetição, salto, overflow e seleção fora do
catálogo encerram a conexão. `65535` representa a pose de referência.

A ordem global é a ordem de processamento do servidor. Não representa relógio
sincronizado ou justiça entre emissores. A seleção é estado global do laboratório;
qualquer participante pode alterá-la. Ao desconectar o emissor, a última seleção
permanece. A simulação futura deverá definir suas próprias regras de autoridade
antes de introduzir comandos de gameplay.

## Limites e finalização

Sockets são não bloqueantes. Leituras/escritas parciais são preservadas entre
iterações; cada peer processa até 32 frames por iteração. As filas de entrada e
saída têm 8 KiB cada. Leitores lentos ou rajadas que excedam a fila são removidos,
sem aguardar que consumam dados. Há no máximo oito conexões pendentes/ativas e
quatro participantes com handshake concluído.

O teto é 128 mensagens por janela de um segundo e 16 MiB cumulativos por direção
em cada conexão. O manifesto admite 256 arquivos, 4096 clips, 512 MiB de conteúdo,
8 MiB de glTF e 1 MiB de texto do manifesto. São orçamentos desta ferramenta,
não limites conhecidos do jogo original.

O handshake vence em 1000 ms; um frame parcial tem o mesmo deadline, que não é
renovado por bytes parciais. Inatividade vence em 1500 ms e o cliente envia
heartbeat a cada 250 ms. `--handshake-ms` permite 100–5000 e `--idle-ms` permite
500–10000. A fila de um peer rejeitado recebe até 100 ms para drenar; o servidor
terminando dá até 150 ms ao conjunto. A rejeição é registrada mesmo quando a
conexão interrompida não permite entregá-la ao cliente.

## Validação executada

`tools/network_smoke.py --binary build/mh4u-network-lab` cria dados de diagnóstico
temporários e usa a CLI pública. `--scene caminho/animated.gltf` copia o conjunto
local para uma pasta de teste e repete a validação com os recursos reais. `--output`
preserva logs e `validation.json` em uma pasta nova; sem ele, a pasta temporária
é removida. O teste sempre coleta/encerra somente os processos e sockets que criou.

O teste executa um servidor e dois clientes nativos independentes, confere que
ambos recebem a seleção ordenada, testa capacidade e saída limpa, altera um
buffer local, remove uma dependência do manifesto e altera a identidade de clip.
Peers de teste enviam versão, comprimento, conteúdo, sequência e seleção inválidos;
também exercitam rate limit, deadlines, fragmentação, coalescência e fila de saída
sob leitores parados. Um cliente nativo novo confirma que o servidor continua
utilizável; no término, ele recebe `Bye` e a porta deixa de aceitar conexões.
Em POSIX, dois clientes adicionais recebem SIGINT e SIGTERM: ambos devem
confirmar `Bye`, sair com código zero e aparecer no servidor com `client_bye`.
Esse caso de sinais é omitido no Windows, onde terminar um processo via Python
tem semântica diferente; encerramento de console Windows não foi verificado.

As execuções macOS e seus resultados ficam em `research/private/network-validation-*`.
Compilação Windows não substitui execução Windows, nem loopback substitui LAN ou
duas redes externas. A exigência de online do jogo permanece em aberto até existir
uma sessão jogável verificada nas condições do roteiro do porte.

Referências primárias: [PicoSHA2](https://github.com/okdshin/PicoSHA2),
[WinSock não bloqueante](https://learn.microsoft.com/en-us/windows/win32/winsock/nonblocking-i-o-2)
e os contratos de glTF em [Khronos](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).
