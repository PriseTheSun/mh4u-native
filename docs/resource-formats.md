# Recursos estáticos de MH4U

O primeiro recurso reproduzível é `/data/pl_one133.arc`: duas malhas da mesma arma (`one133` e `sld133`) e a textura compartilhada `one133_BM`. A seleção usa os nomes internos; o nome exibido da arma não foi identificado. Os parsers em `tools/resources/` leem a ROM local sem executar código do jogo. Usam somente a biblioteca padrão do Python 3.9 ou superior.

## Recriar a partir da ROM

A sequência completa usa somente scripts deste projeto e uma ROM local em formato NCSD/NCCH plaintext. Não exige arquivos do projeto REA anterior. Exemplo com o arquivo desta máquina, executado na raiz de `mh4u-native`:

```sh
python3 -B tools/resources/inspect_rom.py --rom '/Volumes/Games Preservação/GAMES/EMULACAO/Nintendo/Nintendo 3DS/Roms/Monster Hunter 4 Ultimate.3ds' --output research/private/rom-inspection
python3 -B tools/resources/inspect_arc.py --inspection research/private/rom-inspection --output research/private/arc-catalog.json --romfs-path /eng/data/core_title.arc --romfs-path /data/pl_one133.arc
python3 -B tools/resources/extract_title_messages.py --catalog research/private/arc-catalog.json --output research/private/message-samples --archive /eng/data/core_title.arc --resource 'eng\msg\common\Title_eng' --resource 'eng\msg\common\CharMake_eng'
python3 -B tools/resources/extract_model.py --inspection research/private/rom-inspection --output assets/local/one133
```

Use o caminho absoluto da sua ROM em `--rom`: ele será registrado como identidade da fonte e reutilizado nas etapas seguintes. A primeira etapa calcula o SHA-256 completo da ROM, valida os hashes declarados de exheader/ExeFS e superbloco RomFS, extrai `.code` e produz `manifest.json`, `romfs-inventory.json` e `memory-map.json`. Não descriptografa a ROM, autentica assinaturas ou verifica a árvore IVFC inteira. O layout RomFS suportado é o observado, com nível 3 em `+0x1000`. Todos os binários, amostras e manifestos ficam em diretórios privados ignorados.

`inspect_rom.py`, `inspect_arc.py` e `extract_title_messages.py` exigem destinos novos. Para repetir essas etapas, escolha novos destinos e atualize os argumentos das etapas seguintes; não substituem a inspeção existente. A promoção desses três parsers preservou a lógica de leitura/decodificação previamente usada; a alteração de interface fornece caminhos e seleções explícitos. O hash integral da ROM não foi recalculado apenas para promover os scripts.

O catálogo ARC cobre somente os arquivos indicados por `--romfs-path`, retém nomes/tipos/flags/offsets e hashes dos membros comprimidos, mas não os descompacta. A extração de mensagens escolhe nomes exatos via `--resource`, verifica os hashes comprimidos contra o catálogo e descompacta zlib com limite de 16 MiB por amostra. O resultado conserva nomes ordinais, bytes LMD e proveniência; esse script não interpreta LMD, traduz textos ou escreve patches. As ferramentas de localização em `tools/localization/` são uma etapa posterior.

## Reutilizar a inspeção existente

Na raiz deste projeto, usando a inspeção que já existe nesta máquina:

```sh
python3 -B tools/resources/extract_model.py --inspection '../rea-main/.cache/mh4u-native-2026-10-17/extracted' --output assets/local/one133
```

`--inspection` deve conter `manifest.json` (com `source.path`, `source.size`, `source.sha256`) e `romfs-inventory.json` (lista de entradas com `path`, `source_offset`, `size`). O caminho da ROM vem dessa inspeção, sem caminho fixo no extrator. A inspeção precisa ter sido produzida para o mesmo arquivo; a ferramenta exige um único `/data/pl_one133.arc` e não pesquisa outras armas.

A saída contém OBJ, PPM, PNG, alpha PGM, os membros MOD/TEX/MRL, ARC e `manifest.json`. São bytes do jogo ou derivados deles e permanecem em `assets/local/`, já ignorado pelo projeto. O manifest registra offsets, campos desconhecidos, contagens, hashes, caminhos e a seleção explícita de primitivas. Arquivos já existentes são aceitos somente se os bytes forem iguais; arquivos diferentes não são substituídos. Para repetir uma extração após mudar o produtor ou os insumos, use outra pasta explícita. Uma falha pode deixar resultados parciais, preservados para inspeção.

A leitura é restrita ao intervalo do ARC e confere tamanho e estabilidade do arquivo durante a leitura. O SHA-256 da ROM inteira é herdado da inspeção anterior e **não é recalculado**. Os hashes do ARC, membros, saídas e dos dois arquivos de inspeção são calculados nesta execução. A assinatura do jogo e a árvore IVFC não são autenticadas por este extrator; o manifest é proveniência desta ferramenta, não um registro Evidence do REA.

## Validar

```sh
python3 -B tools/resources/validate_probe.py --assets assets/local/one133 --reference research/private/resources/io_import_scene_mod.py --output assets/local/one133/validation.json
```

O arquivo de referência já foi copiado para a pasta privada nesta máquina. Para uma instalação nova, obtenha o arquivo pela URL fixada em `tools/resources/reference.lock.json` e mantenha-o nessa pasta privada. O validador não baixa nada e recusa qualquer SHA-256 diferente de `af2d5c18d226d02644695b339e7fd8edae9041518284d7e91c0814d8c809eb68`.

A referência é `n3ds/io_import_scene_mod.py` do repositório [svanheulen/mhff, commit 6ef14c8ffa341ccbe44e0e6a93eb4bfe541265cf](https://github.com/svanheulen/mhff/blob/6ef14c8ffa341ccbe44e0e6a93eb4bfe541265cf/n3ds/io_import_scene_mod.py). Seu cabeçalho declara **Copyright 2015 Seth VanHeulen; GPL-3.0-or-later** e foi preservado. Ela serve somente como referência de validação: por AST, são selecionados `modifier_tables` e `decode_etc1`; não são executados o import de `bpy` nem o restante do módulo. O decoder histórico exige host little-endian com `unsigned int` de 32 bits, verificado antes da execução. O viewer nativo não carrega essa referência.

As verificações comparam todos os 32.768 pixels RGB com o decoder histórico, conferem hashes e tamanhos do manifest, contagens de vértices/polígonos e reexportação dos OBJ. Também exercitam rejeição de MOD/TEX truncados, buffer e índices inválidos, contagem de índices excessiva, formato não suportado e tamanho zlib incorreto. Isso estabelece consistência estática, não equivalência visual com frames do jogo original. O relatório inclui as limitações verificadas.

## Formatos e interpretação

A descrição completa dos offsets usados está em `tools/resources/format-specification.json`. As fontes públicas primárias são as anotações de engenharia reversa do autor: [ARC](https://github.com/svanheulen/mhff/wiki/MH4U-rArchive-Format), [MOD](https://github.com/svanheulen/mhff/wiki/MH4U-rModel-Format), [TEX](https://github.com/svanheulen/mhff/wiki/MH4U-rTexture-Format) e [MRL](https://github.com/svanheulen/mhff/wiki/MH4U-rMaterial-Format), corroboradas pelo importador fixado acima. Os links da wiki não são snapshots imutáveis. O JSON registra a interpretação utilizada aqui; campos não interpretados permanecem como valores brutos. Não se atribui uma licença às páginas da wiki por inferência a partir da licença do código.

| Recurso | Subconjunto implementado |
| --- | --- |
| ARC | `ARC\0`, versão 19, diretório no offset 12, registros de 80 bytes, membros zlib. Tamanho descompactado nos 28 bits inferiores do campo em `record+0x48`; bits superiores preservados. |
| MOD | `MOD\0`, versão 230; descritores de primitiva de 48 bytes; somente vértices tipo 4, stride 28. Posição float32×3 no offset 0; normal i8×3 no 12; UV float32×2 no 16. |
| Índices | Strips u16 com reinício `0xFFFF`. Lê `extent+1` índices conforme a referência e os arquivos observados; alterna orientação mesmo em triângulos degenerados, omitindo apenas os degenerados na exportação. |
| TEX | `TEX\0`, versão 165, uma textura 2D, ETC1 ou ETC1A4. Somente mip base decodificado. Offsets de mip relativos ao final da tabela de offsets. Tiles 8×8, cada um com quatro blocos 4×4. |
| MRL | `MRL\0`, versão 32. Somente cabeçalho e primeiro registro de recurso: caminho de 64 bytes no offset `section+0x0C`. Estado de shader não interpretado. |

Os limites explícitos são 128 MiB por ARC, 64 MiB descompactados por membro e 16.777.216 pixels por textura. Extensões são validadas antes de acesso ou alocação dos índices; formatos fora desse subconjunto são recusados. Isso não é um leitor geral de todos os recursos de MH4U.

Na seleção observada, a primitiva 0 de `one133` contém **278 vértices e 344 triângulos**; `sld133`, **158 vértices e 124 triângulos**. Cada MOD tem uma segunda primitiva retida no manifest, mas não exportada no OBJ. Sua função, inclusive se representa algum LOD, permanece desconhecida. Ambos têm um material e uma junta; os primeiros registros MRL nomeiam a mesma textura `weapon\one\one133\one133_BM`.

A textura é 128×256, ETC1A4, com quatro níveis de mip e offsets `[0, 32768, 40960, 43008]`. RGB e alpha são exportados separados. A interpretação do alpha como opacidade, brilho ou outra finalidade **não foi estabelecida**; o preview usa RGB opaco. O alpha não recebeu comparação independente.

Posições e UVs são preservados. Normais usam i8 normalizado por 127, com saturação em −1; interpretação corroborada pelo importador, sem validação em execução do jogo. Não há transformação de esqueleto, animação ou transformação de mundo. Unidades físicas e pose correta permanecem desconhecidas. PPM/PNG são linhas de cima para baixo; para os UVs originais, a amostragem de imagem usa `y=(1-v)*height`. Não se inverte o UV gravado no OBJ.

O resultado é um preview estático RGB de um recurso real. Materiais completos, reflexos, iluminação original, animações, colisão e comportamento do jogo continuam fora deste extrator.
