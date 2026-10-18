# Cena compartilhada no visualizador nativo

O marco 0.8 conecta o visualizador ao servidor de movimento. Dois processos
nativos macOS foram operados pelo teclado e renderizaram os estados recebidos.
O endereço ainda é loopback: o teste conecta processos no mesmo computador.

Nos pacotes, abra `Iniciar servidor local`, depois `Abrir jogador 1` e
`Abrir jogador 2`. Use WASD ou os eixos configurados para movimento. A câmera
acompanha somente o jogador daquela janela; as demais posições vêm do mesmo
snapshot. Fechar uma janela envia Bye. O servidor aceita até quatro jogadores,
termina com Ctrl+C e tem limite de 24 horas nos atalhos.

```sh
build/mh4u-network-lab server --world assets/local/village-diagnostic-v1.world.tsv --port 47124 --run-ms 600000
build/mh4u-resource-lab --world assets/local/village-diagnostic-v1.world.tsv --connect-port 47124 --player-name Cacador1 --hide-primitive 3
```

## Integração

O visualizador usa `WorldClient` diretamente, com `pump()` não bloqueante.
Envia intenções limitadas a cada 50 ms, sem posição ou delta de tempo vindos
do cliente. A direção relativa à câmera vira um vetor inteiro truncado para
ficar dentro do disco permitido. A rotação tem velocidade limitada. O servidor
aplica a geometria e o relógio próprios descritos em [network-world.md](network-world.md).

A identidade cobre a receita e seus quatro recursos, dependências, parâmetros
do protocolo e pose inicial. É verificada antes e depois do carregamento.
O mundo e os buffers carregados pertencem a cada processo. Não há vigilância
de arquivos nem proteção prometida contra alteração hostil concorrente.

No modo conectado, movimento local, reset de posição e reprodução livre de
animação ficam desabilitados. A cena usa uma pose estática escolhida pelo
servidor, inicialmente clip0/frame0, coerente com o alinhamento dos pés já
documentado. O render desenha até quatro atores usando a mesma malha carregada.
Não prevê nem interpola posições; uma queda de conexão conserva a última
imagem e mostra o erro. Encerrar a janela tenta Bye por até 500 ms.

## Verificação visual

`research/private/network-viewer-validation/v1/validation.json` registra duas
janelas, três ações de teclado, quatro capturas e saída confirmada de ambas.
Foram comparados **1.703 snapshots iguais** nas duas janelas, incluindo **31
snapshots com colisão limitando movimento**. O primeiro jogador visitou 20
posições, o segundo 66. O cliente restante mostrou apenas um participante
após a saída do outro. Capturas mostram ambos os modelos em posições distintas
e o painel PT-BR.

Esse ensaio antecede a seleção inicial explícita de clip0 no protocolo final;
uma CLI temporária selecionou essa mesma pose durante o ensaio. Seu digest e
binário ficam retidos. A revisão final tem testes de protocolo próprios e
verificação separada do pacote. Uma consulta CUA à janela já fechada expirou;
os logs independentes confirmaram Bye e término do processo.

O pacote final `build/packages/20261017-205326` foi verificado novamente com
o visualizador e uma CLI nativa separada. **598 snapshots** coincidiram,
a pose inicial foi clip0, o teclado moveu o jogador e a saída do outro peer
foi confirmada pelo servidor. Evidência em
`research/private/network-viewer-validation/package-v2/validation.json`.
O launcher permanece aberto; o servidor desta prévia tem duração limitada
a 24 horas. Os 216 arquivos do pacote tiveram hashes verificados e os recursos
macOS/Windows são idênticos. Somente macOS foi executado.

Ainda não há colisão entre participantes, ações sincronizadas, combate,
missões, autenticação, LAN, relay ou acesso pela internet. A compilação Windows
contém o mesmo cliente, mas sua execução não foi validada em Windows.
