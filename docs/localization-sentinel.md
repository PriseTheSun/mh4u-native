# LMD: estrutura de texto e palavra de label ainda em investigação

**É possível exportar estruturalmente os textos dos oito LMD rejeitados e
reconstruir seus bytes sem inventar associações de labels.** O experimento
privado decodificou 4.691 fragmentos e preservou os 3.798 labels e 9.756 controles;
os oito roundtrips inalterados foram byte-exatos. A associação label→controles
e label→texto foi explicitamente marcada como desconhecida em todos eles.

Não houve alteração de `lmd.py`, `inventory.py`, `arc_patch.py` ou do seed. A
cobertura pública do inventário anterior continua a mesma. As novas saídas são
um experimento estrutural privado, não um patch ou uma validação no jogo.

## Observações nos bytes

A decomposição anterior de cada label em `uint32 + uint16 + uint16` preserva
os bytes, mas pode representar incorretamente o segundo campo semântico.
Nos 45 labels especiais, o par de palavras de 16 bits é sempre
`ffff 003f`: a palavra de 32 bits completa em `label+4` é **`0x003fffff`**.
Não se observou `0xffffffff`. Isto sugere um campo compactado de 22+10 bits;
a interpretação semântica depende da confirmação no consumidor do jogo.

Varredura dos **293 conteúdos ingleses únicos**, com todos os hashes dos
payloads retidos novamente verificados:

| Observação | Resultado |
| --- | ---: |
| Total de labels conservados | 32.809 |
| Palavras diferentes de `0x003fffff` | 32.764 |
| Palavras `0x003fffff` | 45 |
| Maior valor regular de `word & 0x003fffff` | 4.866 |
| Menor/maior valor regular de `word >> 22` | 1 / 45 |
| Valor `word >> 22` nos 45 especiais | 0 |
| Arquivos com desvios dos limites/ordem observados | 0 |

Em cada documento, os valores regulares low22 começam em zero, crescem
estritamente e estão dentro da tabela de controles. Seus bits 16..21 são zero:
**este corpus não testa índices regulares acima de 65.535**. A observação não
estabelece que o formato limite os índices a 16 bits.

Foi testada uma hipótese numérica separada: considerar o intervalo entre um
low22 regular e o próximo low22 regular, ou o fim da tabela. Em todos os
32.764 casos, `high10 == 1 + quantidade de registros cujo primeiro word é 11`
nesse intervalo hipotético. Isso oferece uma pergunta concreta ao consumidor,
mas não demonstra o significado do opcode 11, de high10 ou desses intervalos.
Os intervalos hipotéticos não foram exportados como relações de labels.

| Conteúdo/ID representativo | Textos decodificados | Labels especiais |
| --- | ---: | ---: |
| LbMenu / 65621 | 212 | 3 |
| GuildCard / 65642 | 2.396 | 1 |
| OtTalk / 65643 | 355 | 2 |
| mapName / 65802 | 21 | 1 |
| Menu / 65803 | 937 | 30 |
| levelQuest / 65812 | 51 | 1 |
| DLC / 65931 | 337 | 6 |
| Npc092 / 68237 | 382 | 1 |

Os três primeiros conteúdos também têm cópias exatas em outros ARCs, conforme
os 14 IDs preservados no inventário. Não foram somadas novamente nesta análise.

## Experimento de separação estrutural

`research/private/localization/sentinel/sentinel_probe.py` é um protótipo
privado, não importado pelas ferramentas de produção. Ele verifica magic,
versão, limites, tabelas contíguas, descritores, comprimentos repetidos,
UTF-16LE estrito, terminadores, padding e nome interno. Conserva literalmente
todos os labels e palavras de controle, incluindo os valores especiais.
Usa o reconstrutor inalterado de `lmd.py` apenas para reconstrução sem edição;
não chama `Document.references` nem o exportador que deriva intervalos.

No JSON experimental, todos os labels têm `association_status: "unknown"`
e `control_range: null`. Todos os textos têm `label_references: null` e
`label_reference_status: "unknown"`. **Null significa desconhecido**; não
é uma lista vazia que sugeriria ausência de referência. Offsets, texto original,
controles, placeholders conservadores e proveniência são mantidos.

O perfil do protótipo continua limitado ao padrão observado: o valor fora da
tabela aceito é somente o especial, os demais índices precisam respeitar os
limites e a sequência observados. Não é uma opção genérica para ignorar erros.
Oito mutações foram rejeitadas: especial substituído por outro valor inválido,
highword do especial alterado,
tabela sobreposta, offset textual fora da seção, comprimentos divergentes,
terminador ausente, índice candidato do opcode 1 além da tabela e versão
desconhecida. O significado do especial não é pressuposto por esses testes.

## Referência primária e seus limites

O código-fonte do Kuriimu no commit
`ebfbf8de50755cc32a7e1ea4aee394628d49d3d2` conserva a estrutura de labels e
descritores em [LmdSupport.cs](https://github.com/IcySon55/Kuriimu/blob/ebfbf8de50755cc32a7e1ea4aee394628d49d3d2/src/text/text_lmd/LmdSupport.cs).
O leitor de [LMD.cs](https://github.com/IcySon55/Kuriimu/blob/ebfbf8de50755cc32a7e1ea4aee394628d49d3d2/src/text/text_lmd/LMD.cs)
obtém cada string diretamente do descritor textual, independentemente da
associação de labels. Isso corrobora a separação estrutural. Entretanto, o
tratamento da segunda seção usa offsets/tamanhos em bytes e não explica os
valores especiais desta ROM. Não o utilizamos como prova de ausência, fallback,
intervalos de controle ou estrutura compactada 22/10.

As duas fontes foram retidas com commit, URL, tamanho e SHA-256 em
`sentinel/references/manifest.json`. A busca limitada por referências públicas
sobre LMD/`0xffff` não encontrou uma explicação primária adicional aplicável.
Isso não estabelece inexistência de outra documentação. Os bundles REA já
retidos revisados também não forneceram um consumidor de `rLayoutMessage`;
não houve abertura de nova sessão por esta investigação.

## Pergunta concreta ao consumidor e proposta de integração

Foi observada a string `rLayoutMessage` no `code.bin` em offset `0xc4c9e8`.
Usando o mapeamento de segmentos já validado, seu endereço é `0xd4c9e8`.
O JAMCRC com máscara de 31 bits desse nome resulta em `0x62440501`, igual ao
tipo ARC dos LMD. O registro `sentinel/consumer-seed.json` conserva o hash do
código, mapeamento ELF e derivação. Isto identifica uma pista, não prova qual
função carrega ou interpreta o recurso.

Falta seguir o consumidor do segundo word do label e verificar:

1. Se usa máscara `0x003fffff`/shifts de 22 bits e o que significa high10.
2. Qual caminho executa para low22=`0x003fffff`: retorno, fallback, lookup ou
   outra operação. Não converter esse valor em “ausência” antes dessa prova.
3. Como determina o fim dos comandos de um label, inclusive perto de labels
   especiais, e como o opcode 11 participa dessa interpretação.

A sessão REA compartilhada tem outro agente como dono; a pergunta foi enviada
para ser examinada após a prioridade atual. Nenhuma importação foi duplicada.

A proposta inicial de um perfil explícito de exportação estrutural é viável,
como o experimento demonstra. Após a descoberta da palavra possivelmente
compactada, a recomendação coordenada é **aguardar a confirmação do consumidor
e corrigir a representação no produtor `lmd.py`**, preservando o word32 bruto,
em vez de adicionar um perfil provisório redundante baseado em dois campos
uint16 possivelmente incorretos. Qualquer export futuro deverá distinguir
validade estrutural do texto, estado da associação e semântica comprovada.

Se ainda faltar a interpretação do consumidor, um export estrutural separado
pode manter os estados desconhecidos acima. O pipeline de patch/tradução não
deve passar a aceitar esses arquivos implicitamente. Os consumidores de JSON
e SQLite precisarão tratar associação desconhecida como tal; este é um ponto
de contrato a coordenar antes de modificar inventário, seed ou aplicação de ARC.

## Artefatos e reprodução

Tudo abaixo fica no diretório privado `research/private/localization/sentinel/`:

- `report.json`: oito documentos, hashes, contagens, roundtrips e mutações.
- `entry-ID.structural.json`: textos/tabelas sem associações inventadas.
- `entry-ID.unchanged.lmd`: reconstruções idênticas às fontes.
- `entry-ID.observations.json`: posições dos labels especiais e campos crus.
- `packed-word-report.json`: contagens e limites de todos os 293 conteúdos,
  além da comparação numérica explicitamente hipotética.
- `consumer-seed.json` e `references/`: fonte da pergunta e referências fixadas.

```sh
python3 -B research/private/localization/sentinel/sentinel_probe.py
python3 -B research/private/localization/sentinel/packed_word_probe.py
```

Não houve tradução, edição de ROM/ARC, execução do jogo ou atribuição de
semântica de página/ausência/fallback nesta etapa. Os 4.691 fragmentos decodificados
adicionais continuam identificados como experimento estrutural privado.
