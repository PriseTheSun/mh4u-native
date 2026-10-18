# Origem e tradução dos botões comuns

O teste discriminante no Azahar 2126.2 confirmou que, na lista de opções da
criação de personagem, **Back usa Common 60 e Confirm usa Common 61**. O segundo
texto Confirm, Common 62, não foi identificado nessa tela e permanece excluído
do seed. Essa é uma observação no runtime de referência, não uma implementação
do consumidor no motor nativo.

## Experimento preservado

O índice local identifica entry 65800, ordinal 85 de `/eng/data/core_common.arc`,
como `eng\msg\common\Common_eng`. O ARC tem 194 membros; o LMD tem 124
fragmentos. Antes do teste foram verificados o hash do ARC inteiro extraído,
o hash comprimido do membro, seus limites e o hash do payload LMD.

| Artefato original | SHA-256 |
| --- | --- |
| core_common.arc | `e37c70d88fe69e84f4f60c64545a64a3032f2ae53835ce2df78f81760a8f378d` |
| Common LMD | `015a1236c2146295b93cd201b76fa05b450d66707c74fff55f8e162f10e20fe7` |

Um catálogo privado aplicou três marcadores distintos, preservando labels,
controles e todos os demais fragmentos. Os outros 193 membros ARC foram
comparados byte a byte após descompressão e permaneceram iguais.

| Fragmento | Offset original | ID do label | Marcador | Resultado na criação |
| --- | ---: | --- | --- | --- |
| 60, Back | 7984 | `df7dff6d` | B60 | Visível no botão inferior direito |
| 61, Confirm | 7996 | `4faf8bb7` | C61 | Visível acima de B60 |
| 62, Confirm | 8012 | `1084d4dc` | C62 | Não observado nesse contexto |

O botão B, mapeado à tecla `s`, abriu a pergunta para cancelar a criação e
voltar ao menu principal. A captura mostra a pergunta e B60 ao fundo. A opção
No foi escolhida; nenhum personagem foi criado. Não foi acionada a confirmação
final de personagem. Uma tentativa posterior abriu Nome, sem inserir texto;
a execução foi encerrada pelo menu do emulador.

Os arquivos em `research/private/localization/common-button-probe/` preservam
preparação, catálogo de marcadores, fonte, patch, logs LayeredFS, capturas e
restauração. O patch temporário tinha SHA-256
`bda4a9c1ffb632dedc38d4ebf8f302b566c2199df2a8b79cbc2296ebdb04cb41`.

| Captura nativa | SHA-256 |
| --- | --- |
| character-options-markers.png | `1601df663f6023a6c6b497314ac2fc9d2c743aedea6897bda32c67cc388fc6ac` |
| back-dialog-markers.png | `7e50ef568584c0a62c5af7050aaf914a413402a21b95f721e6aeb83b6dad1636` |

As capturas foram copiadas sem alteração da pasta configurada no Azahar.
Todas as interações foram via CUA. A ROM foi parada pelo menu antes de cada
mudança de override. No fim do experimento, o Common temporário foi removido,
restaurando sua ausência anterior; Title/CharMake permaneceu byte a byte igual.
Os 20 arquivos de SDMC/NAND do perfil foram comparados com o backup, sem
nenhuma diferença. Nenhum save externo foi acessado. O tamanho, inode e mtime
da ROM original ficaram inalterados; seu hash completo não foi recalculado.

## Seed e override experimental PT-BR

`localization/pt-BR/common-buttons.seed.json` contém somente 60→Voltar e
61→Confirmar, em estado draft. Conserva texto, offset, referências, identidade
e tokens da fonte. Não declara revisão humana. O fragmento 62 mantém Confirm.

O produtor `lmd.py` aceitou o catálogo; os 124 fragmentos reconstruídos foram
comparados ao alvo esperado. Labels, controles e tokens foram preservados, e
um novo roundtrip do LMD traduzido foi byte a byte igual. `arc_patch.py` gerou
um ARC separado com os outros 193 registros preservados.

| Artefato traduzido | SHA-256 |
| --- | --- |
| Seed de dois botões | `582d82b075406fccdd0b2da28c019d230cd79e3fb347b6525b110372c2daee50` |
| Common LMD PT-BR | `a9d127b4602dda10fe974ae2d1eaa78b8b8984041495fca1b206560def277d74` |
| core_common.arc PT-BR | `00cf4fa2cd73cf28cc511325162cfd878aa0d0c2ac20d4ffceec358757b96fad` |

A instalação é aditiva no perfil Azahar exclusivo do projeto: anteriormente
não havia override `core_common.arc`. O `core_title.arc` Title/CharMake continua
com SHA-256 `76b732cc639b2cf36bc54e254607a3e64f9ad25fd491fb3fd6f14c33919d2020`.
Fontes geradas, readback e instalação estão em
`research/private/localization/common-buttons-reference/`. Essa versão é
experimental e sua validação visual fica registrada separadamente do seed.

A validação final observou **Voltar e Confirmar completos e legíveis dentro dos
botões**, sem corte visual na captura nativa de 400×480. Confirmar ocupa a maior
parte da largura disponível; não foi medido um limite tipográfico geral. Esses
dois textos não contêm acentos, portanto o experimento não amplia a cobertura
de glifos acentuados. Os rótulos acentuados de CharMake já instalados continuam
visíveis na mesma imagem.

A captura final `common-buttons-reference/character-creation.png` tem SHA-256
`9afdd68c30e23e3d556f48d920ec28307c381a6b91f640a0ec9a5c1d455910a1`.
`runtime-validation.json` registra os dois fragmentos observados, a identidade
do override congelado e instalado, o log LayeredFS e os limites do teste. O
seed conserva seu estado draft e a condição de verificação no momento em que
foi criado; a observação posterior está neste relatório, sem declarar revisão
humana ou integração no motor nativo.

O override final foi deixado instalado conforme autorizado, com Title/CharMake
intacto. A ROM ficou pausada na lista de criação; não foi criado personagem.
Os 20 arquivos de dados SDMC/NAND continuam iguais ao baseline. A observação
é limitada a essa tela; outros consumidores dos mesmos textos permanecem sem
validação visual.
