# Investigação com REA — 17/10/2026

REA 6.5.0 executou Ghidra 12.1.4 sobre uma imagem ELF derivada da imagem de
código descompactada da ROM. Nenhum código do jogo foi executado. O arquivo
NCSD original e os bytes ARM permaneceram inalterados.

## Primeira análise concluída

Sessão: `research/private/rea-session-1792269293078/`. Perfil ARM little-endian
v6, compilador padrão da linguagem, analisadores padrão do Ghidra, heap de 3 GiB.
O primeiro `binary_overview` levou 717,819 segundos incluindo inicialização e
análise. CPU total e pico de memória da família de processos não foram medidos.

| Observação | Evidence e arquivo |
| --- | --- |
| 25.939 procedimentos e 17.620 strings reconhecidos pelo Ghidra | `ev_be29e734b0a8fce9ca8314967e0d5691bcdf40210400de93e93c9d73ffd62eb1`, `002-binary_overview.json` |
| Importação com `ARM:LE:32:v6`; bytes dos blocos ligados ao ELF sintético | `ev_9a102b678f4d1c1183671618ec467afa01d9c3f6ac30771e909fe9e29139a55b`, `003-inspect_native_load_image.json` |
| Strings de tipos `rModel`, `rTexture` e `rArchive` | `ev_ab3420a66e70a4405484432bdef4bc4bc6e2315cf216b284c7e361305898b8eb`, `004-search_strings.json` |
| Referência de `0x00d2fb08` à string `rTexture` em `0x00d2fb44` | `ev_121edd5920e7b14448f1f16908850c6270d306dedd5558b05fc1e8f58fd5309d`, `005-xrefs.json` |
| Referência de `0x00d1ebbc` à string `rModel` em `0x00d1ebf8` | `ev_ec0dd0ffa019c2877fa87ac07aceb47859a54df07fb2e8175a70ad10ab94f819`, `006-xrefs.json` |

As contagens são resultados do analisador, não uma prova de que todos os limites
de função ou todas as strings foram identificados corretamente. Referenciar o
nome de um tipo não basta para identificar seu desserializador.

`inspect_native_load_image` retornou **unsupported** para a verificação
independente de formato ELF, preservando observações dos mapeamentos e hashes.
Essas observações não devem ser apresentadas como uma verificação completa do
loader. A primeira imagem não incluía BSS e sua `.data` usava o tamanho alinhado
em páginas. Logs registraram referências sem memória mapeada e alguns conflitos
ARM/Thumb. Os resultados precisam dessas limitações ao serem reutilizados.

Os seis pedidos terminaram imediatamente antes de interrompermos o cliente para
mudar a imagem de análise. As respostas completas foram preservadas; não houve
exportação de snapshot dessa primeira sessão. O registro de interrupção contém
essa correção. O servidor e o processo Ghidra foram encerrados e a pasta
temporária foi removida pelo ciclo de limpeza do REA.

## Intervalo BSS

A chamada inicial em `0x00100000` leva a `0x00100024`. O stub carrega dois
literais, escreve palavras zero enquanto o primeiro endereço é menor que o
segundo e retorna. Os literais são `0x01081b7c` e `0x0111d128`: a diferença é
636.332 bytes, igual ao tamanho BSS do exheader. O início também coincide com o
fim inicializado de `.data`. O disassembler LLVM corroborou a leitura das
instruções ARM. Isso permite derivar o intervalo BSS por análise estática,
sem afirmar que foi observado em execução.

Uma segunda imagem e perfil focado incorporam essa informação. O perfil
desativa `Decompiler Switch Analysis`; sua cobertura de switches automáticos
é menor, e seus resultados não são intercambiáveis com os da primeira análise.
Não se compara o tempo dos dois perfis como se fossem trabalhos equivalentes.

## Consultas focadas concluídas

Sessão `research/private/rea-session-1792270161709/`, imagem
`mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
O novo overview levou 141,820 segundos e reconheceu 25.710 procedimentos e
17.595 strings. Essas diferenças refletem outro mapa/perfil; não significam
que o jogo mudou. CPU total e pico de memória não foram medidos.

| Questão | Resultado e Evidence |
| --- | --- |
| BSS foi importada? | Bloco não inicializado `0x01081b7c`–`0x0111d127`, 636.332 bytes; `ev_02f5a17f42dc5412de4273d217ee87bc8a08a667c7764c60b9af113aabaf63fd`. A verificação independente de formato continua `unsupported`; as observações estão disponíveis. |
| O stub realmente corresponde ao loop de zero? | Pseudocódigo de `0x00100024` percorre esse intervalo com palavras de quatro bytes e grava zero; `ev_40fce5f612081b9f601cdecbc03c88335b141aa42d57b3a411a1bcdda5ef6b70`. |
| O que referencia `rTexture`? | Função `0x00d2faf4`, 72 bytes, passa o nome, um descritor em BSS e `0xf0` a `0x005bf984`; `ev_f82352dc736a16ef8ef015fcb080bc8df0e2230f518a31aae329ec32ef8dfb8f`. O comportamento indica inicialização/registro de tipo, não o carregamento dos pixels. |
| Como o registro identifica um tipo? | `0x005bf984` associa nome, hierarquia e valor mascarado com `0x7fffffff` a tabelas; `ev_cc32c16e5c81fdf3b63e17a657181cccbb223034b17bcbcc79c8a066c3db72dc`. |
| Há um ponto concreto para estudar o fluxo de arquivo? | `0x00bba9b4`, 1.064 bytes, referencia a mensagem `rArchive::DecompressStream::read`, limita leituras pelo tamanho restante, manipula buffers e chama `0x00d6c570`; `ev_79cbb2a35145fe8e3e0d3628d66e2735e896778ec256d552efb24bddd4da0abb`. Chamadas virtuais e os tipos exatos ainda exigem resolução. |

A referência a `rModel` em `0x00d1ebbc` não está dentro de uma função reconhecida
neste perfil (`ev_cea78416fd5cf3b38e79a8e69dada25db9ecbbebbd4c5ed6a8015d0b2215d373`).
Isso é um limite da análise, não evidência de que código não existe nesse local.

## Identificadores dos recursos

O assembly de `0x005bf778` mostra um acumulador em `r0`, índice pelo byte baixo,
tabela de 256 palavras e deslocamento de oito bits:
`ev_45bea86df2bf13d7f26e9a85f8a9af59631368984a743aafc12b0f5204c30a82`.
Leitura local dos bytes confirmou que a tabela em `0x00e30310` é CRC-32 refletido,
polinômio `0xedb88320`, em todas as 256 entradas. Com semente `0xffffffff`, sem
complemento final, e a máscara aplicada pelo registro, os resultados são:

| Nome | Valor calculado | Tipo observado no ARC |
| --- | --- | --- |
| `rTexture` | `0x241f5deb` | `0x241f5deb` |
| `rModel` | `0x58a15856` | `0x58a15856` |
| `rMaterial` | `0x2749c8a8` | `0x2749c8a8` |

Em Python, o valor equivalente é `(~zlib.crc32(nome_ascii)) & 0x7fffffff`.
O relatório `research/private/resource-type-id-verification.json` vincula os
bytes examinados aos Evidence e aos três membros ARC. Essa verificação é
derivada por ferramenta local, não um Evidence emitido pelo REA nem um teste
de execução do jogo.

O pseudocódigo isolado dessa função inferiu retorno `void` e eliminou o cálculo
do hash. O assembly preserva o cálculo. Essa divergência é um exemplo concreto
de por que pseudocódigo não pode ser convertido cegamente em implementação.

## Preservação e próximos pontos

O bundle `research/private/rea-focused-evidence.json` e o snapshot
`research/private/rea-focused.snapshot.json` foram exportados com sucesso.
A sessão foi fechada pelo MCP e o cliente saiu com código zero. As respostas
completas, incluindo limitações, permanecem junto ao catálogo de ferramentas.
O snapshot preserva os dados exportados da sessão; não equivale a um projeto
Ghidra editável persistente.

Próximas perguntas: resolver a tabela de métodos de textura em `0x00e7f478`,
seguir seus métodos de leitura, identificar o papel de `0x00d6c570` no fluxo de
descompressão e confirmar variantes dos formatos antes de ampliar o importador.
Materiais completos, colisão, gameplay e rede continuam sem reconstrução.
A animação avançou no subconjunto descrito a seguir.


## Carregamento de animações

Sessão `research/private/rea-session-1792271307255/`, mesma imagem BSS e perfil
focado. A primeira busca levou 140,729 segundos incluindo a importação; as
consultas seguintes reutilizaram a sessão. Os fatos abaixo são derivados de
análise estática, não de execução do código original.

| Caminho observado | Resultado |
| --- | --- |
| Nome `rMotion` → inicializador `0x00d2d7d0` → descritor `0x010e7548` → fábrica `0x00d03718` | Objeto de 0x68 bytes, tabela de métodos `0x00e6b624`; dados em objeto+0x64. |
| Nome `rMotionList` → inicializador `0x00d40bc0` → descritor `0x010e7568` → fábrica `0x00cd756c` | Objeto de 0x68 bytes, tabela de métodos `0x00e5d118`. |
| `rMotion` método `0x00aed2a8` | Lê bytes, valida `MOT` e versão 65. Tracks de 0x24 bytes começam em dados+0x3c. Não é o mesmo cabeçalho de LMT. |
| `rMotionList` método `0x0023e3d8` | Valida `LMT` e versão 67. Conta slots por u16 em +6 e percorre offsets de quatro bytes a partir de +8. Reloca tracks e blocos de eventos. |

Evidence dos inicializadores: `ev_84ff130877331e290ba8e586a1cd922a5289cdbe760943fdd32a5793caa60e72`
e `ev_81b6caee6829e23b066d935ccd4382f2a70bb41c037217f010dfe7bc99996957`.
Fábricas: `ev_bf202173ea2a3a5c865363899d23f7d1847bd95ecde9f9e9212658fbf3e46719`
e `ev_74c12d44de4416edb726c2fdf7b6ade8ebd00fd405feb090c4407ada2652376a`.
Loaders: `ev_c8602f4b7f5add294cf82d4e6d9958905fd36bc8c986f3ef3321452cdb63c132`
e `ev_2341ca4a57100d3cb50d1e37279681b559e3e148d294bc09ad4206286775a533`.
O assembly do loader LMT também foi preservado:
`ev_e47b280d8f0af89670cc238f406d944077835f6a5e1ab33f8874fbfa2ea75096`.

O registro de propriedades em `0x00aecd88` liga nomes aos getters. Bytes ARM e
disassembly LLVM com VFP2 corroboram no layout **MOT**: número de frames em +8,
loop em +0xc, translação-base em +0x10, quaternion-base em +0x1c, tipo de precisão
em +0x2e e precisões de translação/quaternion/escala em +0x30/+0x34/+0x38.
A ordem dos componentes do quaternion não foi determinada por esses getters.
Não se transfere essa interpretação automaticamente ao cabeçalho LMT.

O loader LMT reloca o buffer de cada track em +0xc e, conforme o codec, dados
adicionais em +0x20. Os codecs 0,1,2,3,6,8,9 não passam pela segunda relocação.
A flag 0x01000000 da ação controla a relocação de tracks; 0x02000000 controla
parte do bloco de eventos. Os significados de gameplay dessas flags e eventos
continuam desconhecidos. O conversor independente registra sua cobertura em
[animação](animation-findings.md).

## Colisão e limite de resolução de endereço

O nome `rCollision` leva ao inicializador `0x00d3b594`, descritor `0x010e7608` e
tabela de descritor `0x00e70620`. Sua fábrica indicada em `0x00ccf330` não foi
reconhecida como função nesse perfil do Ghidra. A consulta de pseudocódigo com
esse texto foi interpretada como **offset de arquivo** e retornou outra função,
`0x00dce054`. Esse resultado (`030-procedure_pseudo_code.json`) foi descartado
como evidência da fábrica. A mesma limitação ocorreu em `0x0016171c`, resolvido
como offset e convertido em `0x002606f4` (`035-procedure_pseudo_code.json`).
Isso não constitui comportamento observado dessas duas funções solicitadas.

O disassembly LLVM no endereço virtual correto da fábrica mostra a tabela de
objeto `0x00e5b568`. O slot +0x2c contém `0x0016014c`, confirmado pelo REA como
função exata. Seu pseudocódigo lê um cabeçalho de 0x48 bytes, verifica `SBC` e
versão `0x77cf8df8` e lê sequências com strides 72,10,32,16,32, seguidas de
estruturas de busca. Evidence:
`ev_01511f30730443caddfd86a170c3c2bd77261ffc9c5b2a974172ffbb5c47e2a6`.
Os contadores e strides não bastam para atribuir semântica a cada campo; a
investigação do recurso deve corroborar os consumidores e os bytes da ROM.


A interpretação dos grupos foi corroborada por `FUN_0016c380`:
`ev_f9126f947889e85fb0e7110e96cdfe6b20860f269c228d343d0d166f319ee71e`,
com assembly `ev_3cb8f29050addb53b6df299a354ec6d4c928af582b4e3a87ad0fb4d0c1bbb6e4`.
A função usa grupo+0x34 como início do intervalo de faces e grupo+0x3c como
início dos vértices. O u16 em face+0x12 indexa a tabela global de registros de
32 bytes em objeto+0xc0. Os bits desses atributos ainda não foram identificados
como piso, parede ou câmera. O chamador reconhecido `0x001872a8` passa a
estrutura para callbacks; não foi inteiramente reconstruído.

Os candidatos foram encontrados por uma busca local em instruções ARM, seguida
de resolução de função e consulta por nome pelo REA. A busca sintática sozinha
não estabelece pertencimento ao código de colisão; candidatos não relacionados
foram descartados. `research/private/collision-consumer-candidates.json` e o
assembly local preservam essa etapa intermediária. Detalhes do parser e das
amostras ficam em [colisão](collision-findings.md).


A sessão de motion/colisão foi exportada e fechada com sucesso pelo MCP, e o
cliente saiu com código zero. Bundle: `research/private/rea-motion-collision-evidence.json`;
snapshot: `research/private/rea-motion-collision.snapshot.json`. As 51 respostas
completas, incluindo consultas inconclusivas e limitações, permanecem na pasta
da sessão. Nenhum byte da ROM ou do executável original foi alterado.
