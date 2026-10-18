# Rótulos gráficos do menu e origem ainda aberta de Confirm/Back

Atualização: o teste posterior de marcadores identificou Common60/61 como
a origem de Back/Confirm na criação. Veja
[localization-common-buttons.md](localization-common-buttons.md). As fronteiras
abaixo descrevem o estado da investigação estática anterior ao teste.

Os rótulos **NEW GAME, CONTINUE, OPTIONS e LANGUAGE estão presentes como
pixels nas texturas do título**. A inspeção do canal alpha decodificado mostra
as palavras completas, e o layout `ui803` referencia os caminhos exatos desses
atlas. Isso explica por que traduzir somente o LMD Title não basta para cobrir
esses rótulos. Não foi feita substituição de textura nem validação de um atlas
traduzido no jogo.

**A origem dos botões Confirm/Back da criação de personagem permanece
desconhecida.** Há textos iguais no LMD compartilhado Common, mas igualdade de
texto não demonstra que a tela os consome. Não os classificamos como gráficos.

## Fonte e escopo observado

ROM EU `CTR-P-BFGP`, title ID `0004000000126100`, SHA-256
`c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`.
A seleção usa o índice `research/private/resources.sqlite`:

- `/eng/data/core_title.arc`: 65 membros no índice, dos quais foram extraídos
  os 30 membros TEX/LYT/LANL: 18 texturas, 11 layouts e uma lista de animações.
- `/eng/data/core_common.arc`: nove membros adicionais, incluindo cinco
  texturas referenciadas literalmente pelos layouts de core_title, mais duas
  texturas em inglês e dois layouts compartilhados candidatos.
- No conjunto: 39 payloads verificados; 25 TEX selecionados; 21 decodificados
  pelo produtor existente; quatro rejeitados explicitamente; 13 tabelas de
  caminhos de LYT examinadas. A LANL não foi interpretada.

`index_resources.extract` verificou os hashes comprimidos, limites e tamanhos
dos membros selecionados. A identidade da ROM inteira vem do índice; esta
investigação não recalculou seu hash completo. O resumo reproduzível verifica
novamente os 39 hashes dos payloads antes de ler suas estruturas.

## Atlas e regiões visíveis

Coordenadas em pixels decodificados, origem superior esquerda, limites finais
exclusivos. Os retângulos delimitam pixels alpha não zero dentro de regiões
selecionadas por inspeção visual. **Não são UVs de sprites extraídas do LYT.**

| ID no índice | Recurso, sem prefixo `eng\lyt\demo\texture\` | Texto observado | Retângulo `(x0,y0,x1,y1)` |
| --- | --- | --- | --- |
| 67160 | `title_menu_ID` | NEW GAME | `(3,59,127,76)` |
| 67160 | `title_menu_ID` | CONTINUE | `(9,80,120,97)` |
| 67160 | `title_menu_ID` | OPTIONS | `(16,101,115,118)` |
| 67160 | `title_menu_ID` | DLC | `(43,122,88,138)` |
| 67167 | `title_menu2_ID` | LANGUAGE | `(24,15,184,35)` |

Ambos são TEX de formato 12, ETC1A4, um mip, respectivamente 256×256 e
256×128. Os pixels são produzidos por `tools/resources/extract_model.py`; o
canal alpha é mostrado em tons de cinza, sem redimensionamento, retoque ou
modificação do recurso. As referências e os limites do decoder já estão em
[resource-formats.md](resource-formats.md).

| Artefato | SHA-256 |
| --- | --- |
| TEX 67160 | `81424002ceccc15221892b776ee07a6fbc108862f7651c087b41b1cb47c75a1c` |
| Preview alpha 67160 | `1e63c89d32543757875e85741683cf3749c0a66d3eb83c042bf74905b97b258a` |
| TEX 67167 | `ef7ebc8f64ad22e2087bcaf9c8a64a6dbf6174a466408d3bfbe72d835a3267e8` |
| Preview alpha 67167 | `489efa9f413a2ba09913fb7b34a0ae043600d3f6d731b3b973d6c89fb1b96485` |

Os hashes completos de todos os recursos e previews estão no relatório privado;
as imagens permanecem em `research/private/localization/ui-buttons/preview/`.

## Ligação com o layout

`eng\lyt\demo\ui803`, entry 67168, SHA-256
`2aebd672421d04f0fa5feb772edd639074e9e951db87f6fb832ae9416af942eb`,
começa com magic `lyt\0`, versão `0x70c`. Os bytes observados contêm quatro
registros de oito bytes a partir de `0x30`; cada registro tem uma palavra zero
preservada como desconhecida e um offset para um caminho ASCII terminado por
NUL. Os quatro caminhos são:

| Offset do registro | Offset da string | Recurso |
| --- | --- | --- |
| 48 | 80 | `eng\lyt\demo\texture\title_menu_ID` |
| 56 | 116 | `lyt\demo\texture\title_menu1_ID` |
| 64 | 148 | `lyt\demo\texture\title_menu_wp_ID` |
| 72 | 184 | `eng\lyt\demo\texture\title_menu2_ID` |

O mesmo arquivo contém nomes como `ui803_top_newgame`, `ui803_top_continue`,
`ui803_top_options` e `ui803_language_settei`. São evidências de associação
do recurso ao menu, mas não estabelecem a ligação individual entre cada nó e
uma região do atlas. A representação dos sprites, seus UVs, seleção por idioma
e caminhos do consumidor ainda não foram decodificados. O arquivo também contém
`Language Settings` e `Language` em UTF-16LE, nos offsets 43056 e 43092; a
existência desses textos adicionais não elimina os pixels observados no atlas.

## Confirm/Back: candidatos e fronteira

A captura de referência preservada pelo responsável pelo runtime em
`research/private/localization/title-charmake-reference/character-creation.png`
mostra Confirm/Back em inglês mesmo com os textos selecionados de CharMake em
PT-BR. A captura tem SHA-256
`2ace04aec951ec3ecd6173002beb6b2facb442db88a770aa90bf3f629c6b5d58`.
Esta investigação apenas inspecionou a captura existente; não executou o jogo.

O catálogo já exportado contém os seguintes candidatos em entry 65800,
`eng\msg\common\Common_eng`, do ARC compartilhado. SHA-256 do LMD:
`015a1236c2146295b93cd201b76fa05b450d66707c74fff55f8e162f10e20fe7`.

| Índice do texto | Offset UTF-16 | Texto | ID de label registrado pelo produtor |
| --- | --- | --- | --- |
| 60 | 7984 | Back | `df7dff6d` |
| 61 | 7996 | Confirm | `4faf8bb7` |
| 62 | 8012 | Confirm | `1084d4dc` |

Os textos Back/Confirm também existem em CharMake e em outros catálogos. Nenhum
dos três candidatos acima foi associado ao botão por um consumidor observado.
Uma busca delimitada de literais ASCII/UTF-16 e IDs candidatos nos layouts
selecionados do título não encontrou uma ligação direta para Confirm/Back.
Isso não exclui lookup indireto, código, fonte gráfica ou outro recurso.

Os previews compartilhados inspecionados mostram painéis, ícones, molduras e
outros elementos; não identificamos neles as palavras completas Confirm/Back.
Há quatro texturas fora da cobertura do decoder atual:

| Entry | Recurso | Formato bruto |
| --- | --- | --- |
| 67161 | `demo_hadairo_BM_HQ_NOMIP` | 3 |
| 67183 | `lby_colorscale_BM_HQ_NOMIP` | 3 |
| 65745 | `cmn_sicon_BM_MQ_NOMIP` | 1 |
| 65744 | `cmn_icon_GSM_NOMIP` | 16 |

Não se infere ausência de palavras nesses recursos nem no restante do jogo.
Os layouts compartilhados `ui000`/`ui001` também referenciam fontes/ícones fora
da seleção decodificada. Para resolver a origem do botão falta observar o
lookup do consumidor ou realizar um experimento controlado que distinga os
fragmentos 60/61/62 de Common. Esse teste fica separado desta investigação;
nenhum seed, override instalado ou imagem do jogo foi alterado.

## Reprodução e limites

Em `research/private/localization/ui-buttons/`:

- `extraction.json` e `external-observations.json`: seleções, caminhos, offsets,
  hashes comprimidos e extraídos, parâmetros de textura, falhas e previews.
- `source/` e `preview/`: payloads originais e representações RGB/alpha.
- `layout-string-observations.json` e `label-byte-matches.json`: observações
  brutas delimitadas, sem presumir semântica de desenho.
- `summarize_probe.py` e `report.json`: verificação de identidade, reprodução
  byte a byte dos 21 previews RGB/alpha, tabelas de caminhos com bounds
  verificados e mapa das cinco regiões identificadas.

Comando, a partir da raiz do projeto:

```sh
python3 -B research/private/localization/ui-buttons/summarize_probe.py
```

A execução terminou com 39 payloads verificados, 21 texturas reproduzidas,
quatro formatos rejeitados e 13 tabelas de LYT verificadas. Não é um parser
completo de layouts, um editor de textura, um pacote gráfico de tradução ou
uma demonstração da renderização original. Os resultados são observações
locais sobre bytes e previews; não são novos objetos Evidence emitidos pelo REA.
