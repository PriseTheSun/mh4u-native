# Catálogo PT-BR v4: forja, missões e ajuda de controles

O pacote local v4 reúne **1.082 fragmentos em dez documentos, com 925
traduções diferentes da fonte**. Acrescenta quatro documentos completos ao
[v3](localization-v3.md): 326 fragmentos, dos quais 310 recebem tradução e 16
permanecem na fonte por serem moeda, símbolos, siglas/requisitos ou uma palavra
idêntica em português. Não substitui v3 nem altera suas 615 traduções.

As traduções são **rascunhos editoriais**. Não há declaração de revisão humana,
terminologia oficial, tradução completa do jogo ou fidelidade de layout. O
conteúdo foi validado no produtor LMD e no consumidor UTF-8 nativo; a integração
da revisão no viewer/empacotamento é um passo separado.

## Documentos novos e cobertura

| Documento | Fragmentos | Traduzidos | Preservados | Conteúdo |
| --- | ---: | ---: | ---: | --- |
| E_makeshop | 180 | 174 | 6 | Forja, melhorias, aprimoramento, joias, equipamentos de Palico, Kinseto e peças de fuzilarco |
| Help | 65 | 65 | 0 | Instruções de busca de caçadores, painéis, câmera, itens, gestos e controles |
| Quest | 39 | 39 | 0 | Objetivos, entregas, suprimentos, missão secundária, abandono e retorno |
| Q_counter | 42 | 32 | 10 | Seleção, requisitos e categorias de missão |
| Total novo | 326 | 310 | 16 | |

Foram traduzidas as mensagens completas, incluindo **53 mensagens com três
ou mais linhas**. As consequências descritas ao abandonar uma missão, os
requisitos de melhorias e as instruções dos painéis não foram reduzidos a
rótulos. Nomes de monstros e a abreviação `A.Kecha Wacha` permanecem na fonte,
sem expandir o significado da abreviação.

Os 16 novos fallbacks são cinco fragmentos de preço `z?` (um com quebra
final), dois símbolos/separadores, oito ocorrências de `VS`/requisitos `HR`
e `Arena`, que também se escreve assim em português. O catálogo completo
tem 157 fallbacks: esses 16 somados aos 141 preservados do v3. Continuam com
`translated=false` e não contam como traduções. Há 926 entradas nos seeds
selecionados; HR de CharMake já era igual ao original e não infla a cobertura.

## Proveniência e identidade

A fonte continua sendo a ROM EU `CTR-P-BFGP`, title ID `0004000000126100`,
SHA-256 `c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`.
Esta revisão usa payloads locais já extraídos; não reabre a ROM, não modifica
ARC e não instala alterações no perfil de referência.

| ID representativo | Arquivo ARC | Nome de membro | SHA-256 do LMD |
| ---: | --- | --- | --- |
| 65625 | `/eng/data/core_arena.arc` | `eng\msg\lobby\E_makeshop_eng` | `55445e08302b507132a01d008abaea8dab60fb828d843fcfaa50a86dd6e1ead7` |
| 65810 | `/eng/data/core_common.arc` | `eng\msg\common\Help_eng` | `60d86bd1f090c382dc6a59fbf343a01226b76c539f1cb6a2c6126bfb78be89f2` |
| 66449 | `/eng/data/core_event.arc` | `eng\msg\quest\Quest_eng` | `a4baf3ea686f5fbdbfc0de0088c1752c3b7643125ee104d9f682f359893324da` |
| 65636 | `/eng/data/core_arena.arc` | `eng\msg\lobby\Q_counter_eng` | `4e3c6e25c5de36525a29b7d676c91409dd484df587b5b01f0906ecd789981625` |

O inventário SQLite foi aberto somente para leitura e verificado por SHA-256
`410afd41e0fe08d328e160070de86c13cb588c3d5334956f7bde9efeb23c5287`.
Os quatro IDs, nomes, caminhos e hashes correspondem ao inventário. O hash da
ROM inteira é o fato retido na proveniência anterior; não foi recalculado
nesta revisão. São verificações Python/C++ locais, sem nova sessão ou Evidence
REA. Caminhos identificam a origem dos bytes; não provam execução de uma tela.

Seeds novos:

- `localization/pt-BR/forge.seed.json`;
- `localization/pt-BR/panel-help.seed.json`;
- `localization/pt-BR/quest-menu.seed.json`;
- `localization/pt-BR/quest-counter.seed.json`.

Cada registro mantém SHA do documento, índice, texto original, offset,
referências e tokens fornecidos por `lmd.py`. As traduções foram redigidas por
mensagem completa. Repetições de uma mensagem fonte exatamente igual dentro
do documento recebem a mesma tradução, preservando seus IDs separados; não
há tradução por substituição de palavras ou frases parciais. Os exports
integrais ficam somente em `research/private/localization/native-v4-validation/`.

## Glossário e limites de interpretação

O glossário anterior foi mantido: baú/bolsa de itens, conjuntos, joias,
espaços, talismãs, fuzilarco, Kinseto, Glaive Inseto e Mercador wyveriano.
Os novos termos distinguem:

- **forjar** (`Forge`), **melhorar** (`Upgrade`) e **aprimorar** (`Hone`);
- **missão**, **missão secundária** e **expedição**;
- **vida**, **vigor** e **afiação**;
- **painel**, **câmera de alvo** e **direcional**.

“Aprimorar” não foi confundido com afiação ou polimento. As descrições dos
limitadores preservam munição carregada, recarga, modos desativados e efeitos
descritos pela fonte. Foram conferidas 43 mensagens fonte repetidas que
envolvem os novos documentos: todas têm a mesma tradução completa no conjunto
selecionado. Essa consistência mecânica não substitui revisão contextual.

Help descreve funções e controles do **3DS original**: duas telas, toque,
direcional, botões, busca sem fio e Salão de Reunião local. Traduzir essas
instruções não implementa rede, controles, painéis ou recursos do PC. Os
comandos não foram adaptados silenciosamente para teclado, mouse ou joystick
do laboratório. Nenhum opcode LMD é executado pelo catálogo.

Foram preservados placeholders, controles, número/posição das linhas vazias,
quebras finais e espaços de junção. Os requisitos G1★/G2★/G3★ continuam na
primeira linha; somente `Permit` foi traduzido na segunda. A primeira redação
que invertia essas duas linhas foi preservada como histórico de rascunho e
corrigida antes do pacote e da validação final. Não há mudança do v3.

Uma segunda leitura de autoria das mensagens longas corrigiu apenas
E_makeshop:130 após a primeira validação: a possibilidade de melhorar a
Glaive Inseto agora é expressa como “pode ser melhorada”, em vez do imperativo
“melhore”. O pacote anterior `45735d0c…` e seu relatório `c1ea0056…` permanecem
em `package-before-final-wording-review/` e `native-final/`, respectivamente.
A validação final confirma que somente esse fragmento mudou entre as duas
revisões v4. A revisão editorial independente solicitada não estava
disponível e permanece pendente; não foi confundida com essa leitura de autoria.

Maiores linhas traduzidas nos novos documentos: E_makeshop 45, Help 41,
Quest 37 e Q_counter 33 caracteres Unicode. Isso não é largura em pixels nem
prova de encaixe no layout original. Fontes, glifos, cortes de linha visuais e
fluxo dos menus não receberam nova validação nesta entrega.

## Reprodução e testes

```sh
python3 -B tools/localization/native_catalog.py build \
  --document research/private/localization/Title.unchanged.lmd localization/pt-BR/title-expanded.seed.json \
  --document research/private/localization/CharMake.unchanged.lmd localization/pt-BR/charmake.seed.json \
  --document research/private/localization/common-button-probe/Common.source.lmd localization/pt-BR/common-v3.seed.json \
  --document research/private/localization/inventory-english/source/entry-65626.lmd localization/pt-BR/equipment-shop.seed.json \
  --document research/private/localization/inventory-english/source/entry-65628.lmd localization/pt-BR/item-shop.seed.json \
  --document research/private/localization/inventory-english/source/entry-65641.lmd localization/pt-BR/itembox.seed.json \
  --document research/private/localization/inventory-english/source/entry-65625.lmd localization/pt-BR/forge.seed.json \
  --document research/private/localization/inventory-english/source/entry-65810.lmd localization/pt-BR/panel-help.seed.json \
  --document research/private/localization/inventory-english/source/entry-66449.lmd localization/pt-BR/quest-menu.seed.json \
  --document research/private/localization/inventory-english/source/entry-65636.lmd localization/pt-BR/quest-counter.seed.json \
  --output assets/local/localization-native-v4
```

Pacote: **638.071 bytes**, SHA-256
`b271948651a6d7d036168d08afc67a8cb972af0d00b9761766d59417912be379`.
O header gerado usa `mh4u::localization::generated::expected_catalog()` sem
alteração de API. O teste existente é compilado com
`-DMH4U_LOCALIZATION_GENERATED -Iassets/local/localization-native-v4` e recebe:

```sh
<localization-tests-v4> \
  assets/local/localization-native-v4/catalog.mhtxt \
  b271948651a6d7d036168d08afc67a8cb972af0d00b9761766d59417912be379 \
  assets/local/localization-native-v4/lookup-oracle.tsv 925
```

Resultados executados no macOS ARM64:

- 76 verificações de parser/CLI nos quatro documentos novos, incluindo
  mutações malformadas, reconstrução inalterada byte a byte e controles;
- reconstrução das traduções com leitura de todos os fragmentos e novo
  roundtrip, conservando labels e palavras de controle;
- cinco negativos de seed real de Quest: hash, referência, duplicação,
  remoção de quebra de parágrafo e deslocamento de linha vazia;
- 7.965 verificações C++ por perfil normal e ASan/UBSan estrito, com
  `-fno-sanitize-recover=all`, retorno zero e stderr vazio;
- comparação dos 1.082 fragmentos em inglês/PT-BR, identidade, offsets,
  presença de tradução e fallback; os 756 registros do v3 e suas 615
  traduções continuam byte a byte idênticos no oracle;
- cinco negativos de processo: pacote adulterado, arquivo ausente,
  identidade esperada de Help alterada, texto esperado de Help alterado e contagem
  incorreta.

Parser, produtor do pacote, loader e teste C++ não mudaram. Seus hashes foram
conferidos contra o relatório v3; os testes exclusivos dos targets v2/v3 não
foram repetidos. Seus relatórios permanecem como evidência histórica. A nova
execução compara o pacote v4 completo, que inclui os documentos anteriores.

Relatório autoritativo:
`research/private/localization/native-v4-validation/native-editorial-final/validation.json`,
SHA-256 `c56168af191d58ffcde9f88750433fa5910835007e9707bdc9c9240a5a4dbb48`.
Há snapshots dos inputs, comandos, stdout/stderr, hashes dos executáveis,
catálogos e reconstruções, além de backups dos seeds antigos e das cinco
saídas v3. Os pins foram conferidos após a execução. A validação final levou 5,30 s
de parede; não houve medição de cache frio, CPU, memória ou tokens.

Nenhuma alteração de ROM, perfil Azahar, launcher, CMake, viewer ou pacote
distribuível faz parte desta entrega. O resultado prepara conteúdo local para
o consumidor nativo; não prova menus funcionais, missões, combate ou online.
