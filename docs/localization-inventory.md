# Inventário dos LMD com marcação de inglês

O produtor atual exporta **42.196 fragmentos de 285 conteúdos LMD únicos**,
presentes em 352 membros ARC. O índice contém 366 membros com marcação de
inglês; os 14 restantes correspondem a oito conteúdos rejeitados explicitamente.
Esta é a cobertura observada do parser, não o total de textos do jogo nem uma
tradução concluída. Os 315 fragmentos de Title/CharMake descritos na investigação
inicial eram apenas duas amostras e fazem parte deste inventário maior.

## Escopo e identidade

Fonte: ROM EU `CTR-P-BFGP`, title ID `0004000000126100`, SHA-256
`c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`.
O índice `research/private/resources.sqlite` contém 1.830 membros de tipo
`0x62440501` (LMD), distribuídos igualmente entre as marcações eng, fre, ger,
ita e spa: 366 por idioma. O inventário seleciona inglês somente quando
concordam os três marcadores de identidade: ARC em `/eng/`, nome interno de
recurso começando com `eng\` e terminando em `_eng`. Não tenta detectar idioma
pelo conteúdo textual. Nesta fonte não houve marcador ambíguo.

`tools/localization/inventory.py` usa o extrator `index_resources.extract` e o
produtor `lmd.py`; não contém outro decoder LMD. Cada membro selecionado foi
extraído e teve seu hash comprimido, tamanho, limites no ARC/ROM e estabilidade
da fonte durante a leitura verificados, inclusive quando outra cópia já existia.
O hash da ROM inteira vem do índice previamente validado; não foi recalculado
por este inventário. Os dados são observações de ferramentas Python locais, não
Evidence emitida pelo REA.

## Contagens observadas

| Medida | Contagem |
| --- | ---: |
| Membros ingleses examinados | 366 |
| Falhas de extração / marcadores ambíguos | 0 / 0 |
| Conteúdos extraídos distintos por SHA-256 | 293 |
| Conteúdos aceitos / rejeitados pelo parser atual | 285 / 8 |
| Instâncias de membros aceitas / rejeitadas | 352 / 14 |
| Grupos de payloads exatamente duplicados | 38 |
| Instâncias adicionais desses payloads | 73 |
| Fragmentos em conteúdos únicos aceitos | 42.196 |
| Fragmentos contando cópias de membros ARC | 51.026 |
| Textos distintos por bytes UTF-16LE exatos | 33.719 |
| Fragmentos vazios entre os aceitos | 0 |
| Labels / registros de controle em conteúdos únicos aceitos | 29.011 / 87.294 |

Não há normalização de Unicode, remoção de espaços ou fusão de textos
semelhantes. Mesmo textos byte a byte iguais conservam a identidade de cada
fragmento. Um fragmento pode ser apenas parte de uma mensagem e não equivale
necessariamente a uma unidade de tradução.

Todos os 293 conteúdos têm magic `lmd\0` e palavra de versão `0x0001010b`.
O perfil suportado permanece `mh4u-lmd-0001010b-contiguous-v1`: tabelas
contíguas de labels/controles/descritores, UTF-16LE estrito e limites/terminadores
verificados. Os 285 aceitos passaram por reconstrução inalterada byte a byte.
A concordância de versão, por si só, não autoriza interpretar layouts ou
semânticas ainda desconhecidos.

## Conteúdos ainda rejeitados

| Recurso sem prefixo `eng\msg\` | IDs no índice | Motivo emitido pelo parser |
| --- | --- | --- |
| `lobby\LbMenu_eng` | 65621, 66674, 67122 | `invalid_control_index` |
| `lobby\GuildCard_eng` | 65642, 66695, 67143 | `invalid_control_index` |
| `lobby\OtTalk_eng` | 65643, 66696, 67144 | `invalid_control_index` |
| `common\mapName_eng` | 65802 | primeiro label não começa em zero |
| `common\Menu_eng` | 65803 | primeiro label não começa em zero |
| `common\levelQuest_eng` | 65812 | primeiro label não começa em zero |
| `DLC_eng` | 65931 | `invalid_control_index` |
| `npc\Npc092_eng` | 68237 | primeiro label não começa em zero |

Uma inspeção delimitada das tabelas desses oito conteúdos observou valores
`0xffff` no campo uint16 de início de controle de alguns labels; nos quatro
últimos casos de primeiro label não zero, ele contém esse valor. Os demais
índices de cada arquivo, quando considerados isoladamente, são estritamente
crescentes. O significado de `0xffff` ainda não foi demonstrado. Não tratamos
esses arquivos como corrompidos nem descartamos labels para fazê-los passar.
As observações numéricas e hashes estão em
`research/private/localization/rejected-label-observations.json`.

Os cabeçalhos rejeitados declaram ao todo 4.691 descritores de texto em conteúdos
únicos. Essa contagem é apenas um campo de cabeçalho observado; não representa
fragmentos decodificados ou referências de mensagem verificadas. Nenhum texto
parcial desses documentos foi incluído no catálogo aceito.

## Catálogo privado e proveniência

Saída: `research/private/localization/inventory-english/`.

- `catalog.sqlite`: catálogo de 45.273.088 bytes, SHA-256
  `410afd41e0fe08d328e160070de86c13cb588c3d5334956f7bde9efeb23c5287`.
- `summary.json`: resumo compacto com cobertura, erros e medição.
- `source/entry-ID.lmd`: 366 payloads extraídos, inclusive duplicatas/rejeitados.
- `verification.json`: verificação de integridade e readback das tabelas.
- `inventory-producer.py`: cópia do produtor correspondente ao hash registrado.

`members` conserva o ID do índice, ARC, nome completo, marcações de idioma,
proveniência da extração, hash do payload e referência à cópia representativa.
`documents` usa o hash como identidade e conserva cabeçalho, perfil, status,
erro e resultado de roundtrip. `labels`, `controls` e `fragments` conservam
offsets, palavras desconhecidas, controles, referências, comprimentos, padding,
texto original e proteção conservadora de placeholders do produtor.

A chave de fragmento é **(hash do documento, índice do texto)**. O rótulo
`internal_name:índice` original permanece disponível, mas não é chave global.
Embora nesta ROM os 285 nomes internos aceitos sejam distintos, os testes
comprovam a conservação de documentos diferentes com o mesmo nome interno.
`metadata` registra o índice e seu digest, identidade da ROM, hashes dos dois
produtores, seleção, deduplicação e indicador explícito de conclusão.

Uma primeira execução foi preservada em `inventory-english.initial/`, antes
de ajustar apenas a contagem de ambiguidades/falhas e o resultado sem qualquer
documento suportado. A execução final acima manteve todas as contagens reais.
Texto original amplo e bancos permanecem em `research/private/`, ignorados;
não houve geração de traduções em massa ou alteração do seed de título.

## Verificação e custo observado

Quatro testes pelo CLI completo usam ARC/ROM sintéticos, o índice de produção
e o inventário. Cobrem deduplicação sem perda de identidade, nomes internos
colidentes, Unicode sem normalização, controles/placeholder, versão desconhecida,
marcadores conflitantes, adulteração de bytes comprimidos, recusa de sobrescrita,
leitura histórica, catálogo incompleto e cobertura suportada vazia. Passaram.

No resultado real, SQLite retornou `integrity_check=ok` e nenhuma violação de
chave estrangeira. O readback conferiu as 366 identidades de membros, os hashes
dos produtores e os bytes de 42.196 textos, 29.011 labels e 87.294 controles
contra os payloads retidos. Os 285 roundtrips foram feitos durante o build.

O build final levou 8,998 s de parede, 6,812 s de CPU de usuário e 0,955 s de
CPU de sistema; pico RSS do processo 81.068.032 bytes. Não houve processo filho.
Foi uma exportação nova, mas o estado do cache de arquivos do sistema operacional
não foi medido: não é uma medição de disco frio nem um benchmark warm/cold.
Custos de tokens da investigação não foram medidos. O resumo seguinte lê o
resultado histórico; não verifica novamente a fonte.

```sh
python3 -B tools/localization/inventory.py build \
  --index research/private/resources.sqlite \
  --output research/private/localization/inventory-english-repeat
python3 -B tools/localization/inventory.py summary \
  --database research/private/localization/inventory-english/catalog.sqlite
python3 -B -m unittest discover -s tools/localization -p 'test_inventory.py' -v
```

Para consultar IDs e nomes com texto, sem perder cópias por ARC:

```sql
SELECT m.entry_id, m.archive, m.name, f.document_sha256, f.text_index,
       f.source_text, f.protected_tokens_json, f.label_references_json
FROM members m JOIN fragments f ON f.document_sha256=m.sha256
WHERE m.entry_id=65804
ORDER BY f.text_index;
```

O exemplo seleciona `itemName_eng`. Para distinguir ausência de um erro em
outras consultas, consultar também `members JOIN documents` e seus campos
`status`/`error_json`; não interpretar uma consulta vazia como falta de texto no
jogo. A gramática completa dos controles, fontes, layout de telas, textos em
imagens/outros formatos e cobertura do jogo continuam em aberto.
Este inventário não executou o jogo e não estabelece que botões ainda em inglês
sejam gráficos.
