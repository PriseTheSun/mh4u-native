# Rosto e cabelo: materiais, primitivas e seleção explícita

Foram resolvidos os materiais e texturas dos recursos `m_face000` (entry 47197)
e `m_hair000` (47245). O resultado inclui 24 cenas isoladas e um candidato
estático com face 12–16 e cabelo 0:
`assets/local/player-appearance-probe/face12-16-hair0/scene.gltf`.
São seis primitivas, 589 vértices, 732 triângulos, três materiais, três texturas
e a rig de referência de 19 juntas do corpo 44780. A geometria do corpo não
está incluída. Não há animação, maquiagem ou sobreposição automática de outros
componentes de cabelo nesse candidato.

A captura nativa mostra uma cabeça coerente com essa escolha. O sufixo `000`
não estabelece aparência inicial, opção de menu ou equipamento original.
Também não foi recuperada a regra que seleciona partes sob cada capacete.
Esta seleção serve à inspeção, não é uma política padrão do motor.

## Identidades e ligação de materiais

`player_appearance.py` reutiliza `player_probe.load_piece` para extrair MOD/MRL/TEX
e `player_assembly.joint_map` para validar o bind. Não altera os produtores
compartilhados, decoder de animação, catálogo, renderer ou pacote 0.4.

| Recurso | ARC | Entry MOD / MRL | SHA-256 do MOD |
| --- | --- | ---: | --- |
| `player\mod\m\face000\m_face000` | `/data/pl_m_face000.arc` | 47197 / 47196 | `9972263f7a5b5996122477cf5805426a07df276c36f2fb59a29ec33c8181a7e5` |
| `player\mod\m\hair000\m_hair000` | `/data/pl_m_hair000.arc` | 47245 / 47244 | `d680672579c2100a2835ed3a0d48c1ffed45766c001e7ec23873b447c6560466` |

Os 14 materiais do rosto têm correspondência única por JAMCRC32 entre o nome
MOD e o registro MRL. Em cada registro, o comando tipo 3, shader hash `0xcd06f`,
refere um índice de textura baseado em um. A resolução por nome exato escolhe
o mesmo ARC ou o ARC compartilhado explicitamente fornecido
`/loc/data/pl_base_0.arc`. Nenhuma textura foi escolhida apenas por proximidade
de nome ou de entrada no índice.

| Materiais MOD do rosto | Registros MRL | Textura base | Entries TEX |
| --- | --- | --- | --- |
| 0–11: `XfBA0__m00_m_make000_` a `...make011_` | 0–11 | `player\mod\common\m_make000_BM` a `...011_BM` | 92567–92578, respectivamente |
| 12: `XfB__m00_m_face000_` | 12 | `player\mod\m\face000\m_face000_BM` | 47195 |
| 13: `XfB__m00_m_eye_` | 13 | `player\mod\common\m_eye_BM` | 92579 |

O material único do cabelo é `XfB__m00_m_hair000`, registro MRL 0, ligado a
`player\mod\m\hair000\m_hair000_BM`, TEX 47243. Todas as 15 texturas distintas
têm 64×64 pixels. Rosto e olhos são ETC1; maquiagem e cabelo são ETC1A4.
Os hashes de cada membro, registros MRL completos, comandos não interpretados,
bytes MOD e histogramas alpha estão em `isolated/inventory.json` e `isolated/source/`.

Os 12 mapas de maquiagem contêm muitos pixels alpha zero (2.567 a 4.011 de
4.096). O cabelo contém 154 zeros e 3.942 valores 255; rosto e olhos têm alpha
255 em toda a imagem decodificada. Isso descreve os dados, mas não determina
blend, alpha test, ordem de passes, tintura ou cor de pele. As prévias conservam
RGB opaco e guardam alpha em PGM separado. As manchas opacas nas prévias de
maquiagem não simulam a aplicação original sobre a face.

## Mapeamento das primitivas

Todos os registros observados têm u16 em +0 igual a `0xedd7`, tipo de vértice 4
e stride 28. No u32 em +4, os bits 12–23 apontam ao material. Os bits baixos
0–11 têm a correlação abaixo; seu significado no consumidor do jogo continua
desconhecido e não foi convertido em regra de visibilidade.

| Rosto: primitiva | Material | Bits baixos +4 | Vértices / triângulos | Observação |
| --- | ---: | ---: | ---: | --- |
| 0 | 0 | 200 | 24 / 32 | grupo associado a `make000` |
| 1 | 1 | 201 | 47 / 68 | grupo associado a `make001` |
| 2 | 2 | 202 | 14 / 12 | grupo associado a `make002` |
| 3 | 3 | 203 | 24 / 20 | grupo associado a `make003` |
| 4 | 4 | 204 | 28 / 28 | grupo associado a `make004` |
| 5 | 5 | 205 | 74 / 106 | grupo associado a `make005` |
| 6 | 6 | 206 | 49 / 46 | grupo associado a `make006` |
| 7 | 7 | 207 | 88 / 112 | grupo associado a `make007` |
| 8 | 8 | 208 | 42 / 48 | grupo associado a `make008` |
| 9 | 9 | 209 | 51 / 71 | grupo associado a `make009` |
| 10 | 10 | 210 | 105 / 146 | grupo associado a `make010` |
| 11 | 11 | 211 | 109 / 158 | grupo associado a `make011` |
| 12 | 12 | 0 | 40 / 56 | cobertura posterior/superior do crânio |
| 13 | 12 | 1 | 145 / 212 | região superior da face |
| 14 | 12 | 2 | 95 / 154 | região inferior, boca e queixo |
| 15 | 12 | 3 | 28 / 24 | duas orelhas |
| 16 | 13 | 4 | 30 / 32 | geometria de olhos |

As descrições anatômicas são observações das prévias isoladas, corroboradas por
posição e textura; não são nomes recuperados do executável. Os grupos 12–16
se complementam espacialmente. Comparando posições arredondadas a quatro
casas nas mesmas coordenadas fonte, 12/13 compartilham nove posições,
13/14 compartilham 17, 12/15 compartilham oito e 13/16 compartilham 24.
Há quatro triângulos geométricos coincidentes entre 13 e 14; foram mantidos.
A contagem ignora ordem dos vértices e não prova mesma orientação, material
ou política de desenho. Detalhes em `overlap.json`.

| Cabelo: primitiva | Bits baixos +4 | Vértices / triângulos | Prévia isolada |
| --- | ---: | ---: | --- |
| 0 | 0 | 251 / 254 | cobertura ampla de cabelo, escolhida explicitamente no candidato |
| 1 | 6 | 54 / 28 | pequena geometria radial no topo |
| 2 | 4 | 58 / 48 | faixa posterior/inferior |
| 3 | 1 | 137 / 134 | porção superior/posterior |
| 4 | 5 | 172 / 160 | porção posterior mais ampla |
| 5 | 2 | 165 / 151 | porção frontal |
| 6 | 3 | 198 / 172 | outra porção frontal sobreposta em região semelhante |

Essas sete primitivas não foram rotuladas como sete penteados completos.
As partes 3/5 compartilham 22 posições e 3/6 compartilham 15; também existem
sobreposições espaciais. Isso é compatível com partes ou variantes condicionais,
mas a combinação original não foi demonstrada. O inventário não sobrepõe
todas por padrão, e nenhuma regra de exclusão foi inferida a partir do número.

## Candidato estático e validação

A escolha explícita `47197:12,13,14,15,16` + `47245:0` usa as coordenadas fonte
sem encaixe, escala, rotação ou translação ajustada. Rosto usa a junta ID 4;
cabelo usa IDs 2, 3 e 4. A validação exige as matrizes globais e inverse binds
compatíveis com a rig base, além de IDs e relações internas.

| Medida | Rosto | Cabelo |
| --- | ---: | ---: |
| Maior diferença de matriz global contra o corpo | 8,15331e−6 | 1,00607e−5 |
| Maior diferença de inverse bind | 2,75593e−6 | 7,62939e−6 |

O glTF combinado serializado tem erro bind máximo `2,835615e−6` unidades fonte.
As unidades físicas continuam desconhecidas. Materiais, texturas e índices
não usados são removidos das tabelas de cada prévia; a geometria selecionada
e sua identidade fonte permanecem intactas. O produtor exige seleção explícita
e rejeita lista vazia, índices repetidos, negativos, fora de faixa ou não inteiros.

Foram executados:

- Dois testes de fronteira para seleção e compactação das dependências,
  incluindo preservação de ordem, ausência de mutação da fonte e índices inválidos.
- 354 verificações de readback contra MOD/MRL/TEX: posições, normais, UVs,
  pesos, remapeamento de juntas, triângulos, material, entrada TEX e pixels RGB.
- Khronos Validator 2.0.0-dev.3.10 nas 24 cenas isoladas e no candidato:
  zero erros, avisos, infos ou hints.
- 25 execuções temporárias do renderer nativo, com dois quadros e captura,
  todas encerradas com código zero. Os processos persistentes do usuário
  e o pacote 0.4 não foram controlados ou alterados.

O relatório é `assets/local/player-appearance-probe/verification.json`.
A galeria local `gallery.html` reúne as capturas. Cada prévia isolada tem
enquadramento próprio, portanto o tamanho na imagem não compara escala.
O candidato tem SHA-256 glTF
`45d5e60caebceb896cb35ad3b733e8d65e2d2dd8bcb0939f284a6a59e4a357cb`
e BIN `e89e1124df717ddc405eaa24d0b0a0eedab550ddb5e3b37bc33f2952d0825b0a`;
seu `provenance.json` inclui dependências, seleção e omissões exatas.

Reprodução, a partir da raiz do projeto e com diretórios de saída novos:

```sh
python3 -B tools/resources/player_appearance.py inventory \
  --index research/private/resources.sqlite --base-entry 44780 \
  --entry 47197 --entry 47245 --shared-archive /loc/data/pl_base_0.arc \
  --output /caminho/novo/isolated
python3 -B tools/resources/player_appearance.py compose \
  --index research/private/resources.sqlite --base-entry 44780 \
  --selection 47197:12,13,14,15,16 --selection 47245:0 \
  --shared-archive /loc/data/pl_base_0.arc --output /caminho/novo/candidato
```

Esta etapa não executou código original do jogo nem abriu nova sessão REA.
São observações de bytes extraídos com hashes e verificações do motor nativo.
Uma associação completa ao sistema de criação de personagem ainda exige
recuperar o consumidor dos campos de seleção, materiais e parâmetros de cor.

## Composição estática de sete peças

`assets/local/player-assembly-probe/seven-piece-candidate/scene.gltf` combina
as cinco peças antes aceitas com a seleção de rosto e cabelo acima. Não substitui
o candidato de cinco peças nem inclui animações. A seleção foi explícita:

| Entrada MOD | Primitivas fonte mantidas | Índices no novo recurso |
| --- | --- | --- |
| 44780, torso | 0, 1, 2, 3 | 0–3 |
| 42456, braços | 0, 1 | 4–5 |
| 49875, pernas | 0, 1 | 6–7 |
| 52640, cintura | 0, 1, 2 | 8–10 |
| 47355, acessório da cabeça | 0, 1, 2 | 11–13 |
| 47197, rosto | 12, 13, 14, 15, 16 | 14–18 |
| 47245, cabelo | 0 | 19 |

O novo recurso contém 4.507 vértices, 5.043 triângulos, 20 primitivas,
16 materiais, nove texturas e uma skin com 19 juntas. Os primeiros 19 nós
esqueléticos são idênticos aos do candidato de cinco peças; as 14 identidades
de primitiva anteriores também permanecem na mesma ordem. Os readbacks contra
os bytes fonte verificam os atributos e índices de todas as peças. Não houve
ajuste de encaixe ou modificação das coordenadas fonte.

Foram executados dois testes de seleção, 508 verificações de readback e o
Khronos Validator 2.0.0-dev.3.10, com zero erros, avisos, infos ou hints.
O erro máximo de posição bind após serialização é `1,335686e−5` unidades fonte;
o maior erro de soma de pesos é `4,470349e−8`. Os relatórios estão no diretório
do candidato: `readback.json`, `khronos-validation.json` e
`composition-verification.json`.

A captura `inspection-hide3.png` foi gerada pelo renderer nativo, em dois
quadros, com a escolha explícita `--hide-primitive 3`; a execução terminou
com código zero. Ela mostra corpo, rosto e cabelo em pose estática no mesmo
esqueleto. Essa máscara apenas facilita a inspeção: oculta a geometria do torso
cujo UV constante gera a silhueta preta já diagnosticada. A primitiva permanece
no arquivo e é visível por padrão. Sua condição original de visibilidade,
as regras de cobertura entre equipamento e cabelo e a aparência inicial do
jogo continuam desconhecidas. O shader é a prévia neutra já descrita acima.

O SHA-256 do glTF é
`92a6ce135d99fcc9192b1dad967d2143acdd0f42b1497ec3d6c2a70609181f3e`;
o do BIN é
`d1b684aa2bf3ab11576ee875aa7a0b27d2c2b3b367c8d1bde65bfc6e76f9cfbd`.

```sh
python3 -B tools/resources/player_appearance.py compose \
  --index research/private/resources.sqlite --base-entry 44780 \
  --selection 44780:0,1,2,3 --selection 42456:0,1 \
  --selection 49875:0,1 --selection 52640:0,1,2 \
  --selection 47355:0,1,2 --selection 47197:12,13,14,15,16 \
  --selection 47245:0 --shared-archive /loc/data/pl_base_0.arc \
  --output /caminho/novo/sete-pecas
build/mh4u-resource-lab --model /caminho/novo/sete-pecas/scene.gltf \
  --animation -1 --hide-primitive 3 --frames 2 \
  --screenshot /caminho/novo/sete-pecas/inspection-hide3.ppm
```
