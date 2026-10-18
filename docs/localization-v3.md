# Catálogo PT-BR v3: baú, lojas e títulos de NPC

O pacote local `assets/local/localization-native-v3/catalog.mhtxt` contém
**756 fragmentos em seis documentos, com 615 traduções diferentes da fonte**.
São 333 traduções adicionais: 24 títulos/funções de NPC em Common e 309
fragmentos de ItemBox, I_shop e E_shop. As 282 traduções anteriores permanecem
idênticas. Esta revisão é um **rascunho editorial**, sem alegação de revisão
humana, terminologia oficial, tradução completa do jogo ou implementação das
telas e regras de inventário.

O C++ já existente lê este pacote pelo mesmo contrato UTF-8 e identidade
documento/hash/índice. Não houve alteração do loader, do parser LMD, da ROM,
dos ARC ou do perfil de referência. A integração de seleção do pacote no
viewer e no empacotamento é um passo separado; os resultados desta entrega
são do produtor e do consumidor C++ isolado, sem nova verificação visual.

## Conteúdo selecionado

| Documento | Fragmentos | Traduções v3 | Fallback preservado | Novas traduções |
| --- | ---: | ---: | ---: | ---: |
| Title | 136 | 58 | 78 | 0 |
| CharMake | 179 | 150 | 29 | 0 |
| Common | 124 | 98 | 26 | 24 |
| E_shop | 34 | 34 | 0 | 34 |
| I_shop | 36 | 34 | 2 | 34 |
| ItemBox | 247 | 241 | 6 | 241 |
| Total | 756 | 615 | 141 | 333 |

Dos 157 fallbacks da revisão anterior, 24 agora têm tradução e 133 continuam
na fonte. Os três documentos novos acrescentam oito fallbacks. O relatório
privado `native-final/fallbacks.json` conserva cada índice, texto original e
motivo de preservação:

- 78 marcadores DUMMY;
- 22 símbolos/rótulos numéricos, incluindo indicadores de classificação;
- 21 fragmentos de espaço;
- oito unidades/símbolos de moeda;
- sete ocorrências de termos próprios/siglas;
- cinco autônimos de idiomas.

Esses 141 fragmentos continuam com `translated=false`. Sua preservação é uma
decisão editorial explícita, não uma afirmação de que foram traduzidos. Os
seeds selecionam 616 entradas; HR já era igual ao original em CharMake e não
infla a contagem de 615 traduções. O inventário amplo continua sendo outro
escopo: [42.196 fragmentos aceitos pelo parser](localization-inventory.md).

ItemBox cobre guardar/retirar/combinar/vender itens, conjuntos, equipamentos,
talismãs, joias e aparência. I_shop e E_shop completam as mensagens de compra
e venda relacionadas. Cada documento foi lido integralmente antes da escolha
do conjunto; não foram inventados textos ausentes.

## Fonte e identidade

ROM EU `CTR-P-BFGP`, title ID `0004000000126100`, SHA-256
`c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`.
Os três novos documentos vieram dos payloads já extraídos do inventário
inglês. O representante escolhido é o membro de `/eng/data/core_arena.arc`;
isso identifica a origem dos bytes, sem provar uma tela de execução específica.

| ID do índice | Recurso | SHA-256 do LMD |
| ---: | --- | --- |
| 65626 | `eng\msg\lobby\E_shop_eng` | `ec4295693e69993423b2dd111b3c61984cbe155107b6fde2d7181c33b14888f5` |
| 65628 | `eng\msg\lobby\I_shop_eng` | `6ef553688eddeda58baf4f25da86f1213ba3273b9a98ca0d160e35134ebc42c7` |
| 65641 | `eng\msg\lobby\ItemBox_eng` | `14f8c3f7267615f073faf251c58706b1e3c07319ad723d5b9aa18e9df2725264` |

O produtor de seeds consulta o inventário SQLite somente em leitura e exige
seu SHA-256 `410afd41e0fe08d328e160070de86c13cb588c3d5334956f7bde9efeb23c5287`.
Valida ID, arquivo, nome de membro e digest do payload. O hash da ROM é a
identidade retida pelo inventário; esta revisão não reabriu nem recalculou a
ROM inteira. Estas são verificações locais Python/C++, não novas Evidence REA.

Novos seeds, sem substituição dos anteriores:

- `localization/pt-BR/common-v3.seed.json`;
- `localization/pt-BR/equipment-shop.seed.json`;
- `localization/pt-BR/item-shop.seed.json`;
- `localization/pt-BR/itembox.seed.json`.

Title e CharMake reutilizam os seeds já existentes. Os novos registros
preservam texto original, índice, offset, referências e tokens provenientes
de `lmd.py`. Repetições exatamente iguais dentro de cada documento recebem
a mesma tradução redigida, mantendo registros/identidades separados. O
produtor não agrupa textos entre variantes, nem executa controles LMD.

## Escolhas editoriais e limites

O glossário fixa **baú de itens**, **bolsa de itens**, **conjunto de itens**,
**conjunto de equipamentos**, **joia**, **espaço** e **talismã**. “Detalhes”
mantém a escolha de CharMake para características cosméticas. Nomes próprios
e autônimos de idiomas permanecem como na fonte. Caravaneiro, Assistente da
Guilda, Mercador wyveriano e Transportador Palico são adaptações editoriais
dos títulos em Common, não nomes oficiais confirmados em outro produto.

Quantidade/posição de quebras, linhas vazias, quebra final, placeholders e
espaços de junção inicial/final foram preservados. Por exemplo, fragmentos de
preço continuam separados do valor e do símbolo de moeda. Não foram reunidos
em uma sentença nova nem alterados para simular comandos do jogo.

O limite de largura do layout original não está provado. Maior linha traduzida
nos documentos novos: E_shop 36, I_shop 38 e ItemBox 37 caracteres Unicode;
contagem de caracteres não mede largura de glifos. Não houve validação de
fontes, truncamento, ordenação de elementos ou telas originais nesta revisão.
Textos sobre Circle Pad e tipos de equipamento descrevem o conteúdo legado;
não implementam esses dispositivos ou regras no novo motor.

## Reprodução e verificação

```sh
python3 -B tools/localization/native_catalog.py build \
  --document research/private/localization/Title.unchanged.lmd localization/pt-BR/title-expanded.seed.json \
  --document research/private/localization/CharMake.unchanged.lmd localization/pt-BR/charmake.seed.json \
  --document research/private/localization/common-button-probe/Common.source.lmd localization/pt-BR/common-v3.seed.json \
  --document research/private/localization/inventory-english/source/entry-65626.lmd localization/pt-BR/equipment-shop.seed.json \
  --document research/private/localization/inventory-english/source/entry-65628.lmd localization/pt-BR/item-shop.seed.json \
  --document research/private/localization/inventory-english/source/entry-65641.lmd localization/pt-BR/itembox.seed.json \
  --output assets/local/localization-native-v3
```

Pacote: 437.641 bytes, SHA-256
`992d32d160cf884d548c89c4713999897987a0f8f86f253e616df53dad31b226`.
O header `catalog_identity.hpp` gerado expõe a mesma função
`mh4u::localization::generated::expected_catalog()` dos pacotes anteriores.
Inclui identidades e contagens, sem textos.

O perfil opcional `MH4U_LOCALIZATION_GENERATED` do teste C++ usa esse header
como identidade independente. As chamadas antigas sem a macro mantêm o
perfil de três documentos e 439 fragmentos; a regressão v2 passou novamente.

```sh
clang++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
  -DMH4U_LOCALIZATION_GENERATED -Iassets/local/localization-native-v3 \
  -Ivendor/picosha2 -Isrc src/localization.cpp src/localization_tests.cpp \
  -o /tmp/mh4u-localization-v3-tests
/tmp/mh4u-localization-v3-tests \
  assets/local/localization-native-v3/catalog.mhtxt \
  992d32d160cf884d548c89c4713999897987a0f8f86f253e616df53dad31b226 \
  assets/local/localization-native-v3/lookup-oracle.tsv 615
```

Resultados executados em macOS ARM64:

- seis roundtrips LMD originais byte a byte, reconstrução traduzida, leitura
  de todos os fragmentos e roundtrip das reconstruções;
- 114 verificações do CLI/parser com dados reais e mutações malformadas;
- 12 verificações sintéticas do produtor e sete rejeições adicionais com o
  seed real de ItemBox: hash, duplicação, texto fonte, offset, referências,
  marcação de idioma e remoção de quebra de linha;
- 5.675 asserções C++ em cada compilação normal e ASan/UBSan com
  `-fno-sanitize-recover=all`; stderr vazio, 756 consultas comparadas em inglês
  e PT-BR, presença/fallback/offset conferidos;
- cinco negativos de processo no binário normal: pacote adulterado, arquivo
  ausente, pin divergente, expectativa de tradução alterada e contagem errada;
- 3.446 asserções no perfil legado v2, mantendo suas 282 traduções.

O teste antigo de fonte malformada supunha que o primeiro registro de controle
era uma referência de texto. Common começa com outro opcode. A tentativa
inicial foi retida em `attempt-first-control/`; `verify_samples.py` agora
localiza um registro opcode 1 pelo documento já decodificado antes de
adulterar seu índice. O parser não mudou. Isso corrige a produção do caso de
teste, sem tratar um controle válido como se fosse uma referência inválida.

Evidência e snapshots ficam em
`research/private/localization/native-v3-validation/`. O relatório autoritativo
`native-final/validation.json` tem SHA-256
`533d1040dc42ab645b6c62b9b9a2fef50a6e89fa0be4839a7caa1b525600ca43`.
Ele fixa comandos, saídas, compilador, seeds, fontes, pacote, executáveis e
cópias dos inputs; verifica os hashes novamente após a execução. O diretório
`before/` conserva todos os seeds anteriores e as cinco saídas v2, cuja
identidade foi conferida antes/depois. Não houve medição de cache frio,
memória, CPU ou tokens da tarefa; o relatório registra tempos de parede.

Texto original amplo, exports, reconstruções e oracle permanecem locais.
A validação de dados/loader não estabelece revisão linguística contextual,
interface visual, inventário funcional, bootstrap do jogo ou funcionamento
online.
