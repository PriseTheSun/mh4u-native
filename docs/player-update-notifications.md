# Notificações no estado nativo

`Player::prepare_and_update_notifications` estende a operação persistente até
antes de `b145f0`, após o retorno de `b41804`. A fila pertence ao mundo e é
fornecida explicitamente a cada chamada. Dois jogadores podem usar a mesma
fila; criar um Player não cria nem reinicializa essa fila.

Preparação, despacho, velocidade, indicadores e notificações compartilham o
mesmo candidato. O Player, a coleção de objetos, as alterações da grade e a
fila só são publicados depois de todas as etapas e da renderização terem
sucesso. Um erro na segunda notificação também descarta a primeira inserção
calculada naquela chamada.

A fila recuperada tem 16 registros de 148 bytes, 16 referências ordenadas e
um contador de 32 bits. Esses três elementos não precisam concordar: o código
original permite referências repetidas, referências opacas e contador com
wraparound. A implementação preserva essas situações. Ela também preserva
os 51 bytes de cada registro que a inserção não escreve, inclusive quando seu
conteúdo ainda é desconhecido.

Fila cheia é um resultado normal. Quando os slots estão cheios mas existe
um registro livre referenciado por eles, o código original remove a primeira
referência correspondente, limpa o registro e ainda retorna sem inserir.
A próxima notificação usa esse estado atualizado. Essa sequência também foi
implementada; não foi substituída por uma fila convencional.

Os campos anteriores `PW1fc` e `PW200`, e o seletor `PW586`, têm donos únicos
no estado compartilhado. Seu produtor ainda não foi recuperado. Esta etapa
não copia automaticamente os indicadores atuais para os anteriores. O byte
de pilha exigido pela função isolada permanece explícito nessa API; a
composição contínua usa o zero deixado pela execução anterior de `b22164`.

O build público passou em 151/151 CTests no macOS, em 68,45 segundos de
testes. A compilação para Windows x64 também passou sem avisos, com os
mesmos 332 arquivos de entrada preservados. O executável Windows ainda
não foi executado em Windows.

A comparação com o original cobre 163 execuções contínuas até esse ponto e
dois prefixos interrompidos antes da emissão de eventos ainda não recuperada.
Foram conferidas 171 tentativas de notificação, 164 inserções e seis chamadas
de limpeza. A função isolada reutiliza outros 216 casos originais preservados.
Os perfis normal e com ASan/UBSan passaram, incluindo 100 testes de rejeição de entradas
inválidas no comparador contínuo. As falhas de alocação e renderização também
preservaram os quatro estados até uma repetição bem-sucedida da chamada.

As evidências ficam em `research/private/player-update-notifications-v1/`.
A implementação nativa não executa ARM nem depende de runtime de 3DS; a
execução ARM é usada separadamente para comparar resultados da ROM.

A API ainda é uma operação explícita de diagnóstico. Ela não inicializa a
fila original, não consome ou apresenta as notificações e não completa o
chamador `b1407c`. A emissão de eventos em `c08238`, as etapas seguintes,
a inicialização e o scheduler continuam abertos. O launcher publicado
permanece 0.17.0. Combate, missões, saves, tradução integral, controle físico,
execução no Windows e coop pela Internet continuam necessários para o port.
