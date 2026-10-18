# Composição observada de v00a01

`assets/local/stage-composition-probe/v00a01-first-keys/scene.gltf` reúne o
modelo principal 93727 e o modelo 93729 (`v00a01-a00`). O segundo usa a posição
e escala registradas no recurso SDL 93730 e a rotação zero de sua primeira
chave. É uma **fotografia estática dos valores das primeiras chaves**, não uma
afirmação sobre o estado inicial observado no jogo.

O resultado tem 24 primitivas, 6.944 vértices, 5.662 triângulos, sete materiais
e seis referências a texturas. Nenhuma primitiva foi removida. Os dois buffers
de geometria, os PNGs e os alpha PGM são cópias byte a byte dos exports anteriores.
Os materiais mantêm a prévia opaca neutra; o shader original, a iluminação e
as regras de visibilidade continuam desconhecidos.

SHA-256 do glTF:
`bf14ec4d06d3cc0f7005a200313f90a98f8e8f7a0254b4cc3b9a0feda06b2cd2`.
O arquivo `provenance.json` registra ROM, entradas, hashes, offsets,
transformação selecionada, dependências e referências REA. O snapshot anterior
à verificação mais estrita dos tipos de propriedade está preservado em
`provenance.before-profile-hardening.json`. A proveniência anterior à confirmação
do loader está em `provenance.before-loader-proof.json`; os bytes glTF/BIN/PNG
não mudaram.

## Escopo do inventário

Foram inventariados todos os 263 membros de cinco ARCs explícitos:
`/loc/data/v00.arc`, `/loc/data/v00a00.arc`, `/loc/data/v00a01.arc`,
`/snake/data/v00a01_.arc` e `/snake/loc/data/v00a01_.arc`.
Somente os recursos selecionados de agendamento, câmera e colisão foram
descomprimidos nesta investigação. Os dois ARCs `snake` examinados contêm
texturas e materiais, sem tabela de colocação identificada.

O parser novo em `tools/resources/stage_composition.py` leu oito SDLs:
93730, 93731, 93732, 93737, 93671, 93676, 93677 e 93678. São 299 registros de
track e 248 chaves. O inventário conserva registros binários, tipos de
propriedade, pais/slots, nomes, hashes, modos de chave e valores sem atribuir
semântica de gameplay. Os limites desta ferramenta são 8 MiB por SDL,
4.096 tracks, 65.536 chaves acumuladas e strings de até 1.024 bytes; spans
são conferidos antes da expansão. Valores desconhecidos permanecem explícitos.

Relatórios privados em `research/private/stage-composition/`:

- `neighborhood-inventory.json`: membros dos cinco ARCs e metadados dos
  853 arquivos RomFS fora de ARC, sem descompressão indiscriminada.
- `initial-extraction.json`: identidades das nove extrações iniciais.
- `sdl-inventory-final/`: oito tabelas completas, bytes fonte e confirmação do loader.
  O inventário inicial em `sdl-inventory/` permanece preservado.
- `references/`: fontes primárias fixadas e hashes.

O recurso de câmera 93740 começa com `SCD\0`, versão 19; sua estrutura e sua
relação com entradas de jogador não foram decodificadas. A busca de nomes e
tipos não demonstra ausência de spawn em outros dados ou no executável.

## Evidência da colocação de a00

O tipo ARC `0x4c0db839` coincide com o hash de `rScheduler`, usando a função
CRC já corroborada no projeto. O SDL 93730 tem SHA-256
`25a38338971e7fa81fdc10a04e9fe6a49186694e6b1e0aaa439d6adad71693b0`.

A estrutura observada coincide com o cabeçalho x86 de 24 bytes e registros
de 24 bytes descritos em [RevilLib, sdl.cpp, commit e17f9718](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/sdl.cpp).
O caminho para versões a partir de 20 é compatível com a versão 21 destes
arquivos. Nomes e referências de recurso usam offsets relativos à tabela de
strings; tempos e valores apontam para offsets absolutos no SDL. A aplicação
desse layout é corroborada pelos limites e referências de todos os oito arquivos.

O loader nativo em `0x16b670` confirma magic `SDL\0`, versão 21, cabeçalho de
24 bytes, contagem `u16` em `+6`, registros de 24 bytes e tabela de strings em
`+0x14`. Os registros usam nome em `+8`, tempos em `+16` e dados em `+20`;
o consumidor reloca esses ponteiros conforme o mesmo layout. Referências de
recurso/string cujo valor é zero permanecem nulas, em vez de apontar para o
início da tabela. O parser conserva esse caso e o exportador exige uma
referência de modelo não nula.

A confirmação usa 680 bytes retornados pelo REA e desassemblados com LLVM,
pois o Ghidra não reconheceu o corpo como procedimento. O loader também
compara o campo do cabeçalho em `+8` com um valor no objeto global em `+0x618`.
A semântica desse valor permanece desconhecida: o parser o conserva como
`crc_raw`, seguindo o nome da referência primária, sem reproduzir a condição
de aceitação do jogo. Isso limita o parser a inspeção estrutural local.

| Track | Nome / vínculo | Bytes observados |
| --- | --- | --- |
| 1 | unidade `uModel` | hash `0x45a468ee`; slot 9 |
| 4 | `mPos` | uma chave, frame 0; float4 em `0x150`: `(0,588,11385,0)` |
| 5 | `mAngle` | frames 0 e 599; `(−0,0,0,0)` e `(−0,6.283185005,0,0)` |
| 6 | `mScale` | uma chave, frame 0; `(1,1,1,0)` |
| 7 | `mQuat` | nenhuma chave |
| 8 | `mpModel` | tipo `0x58a15856` e nome completo `stage\v00\v00a01\v00a01-a00` |

`mpModel` tem exatamente uma correspondência de tipo e nome no mesmo ARC:
entrada MOD 93729. Isso sustenta a associação; o sufixo do nome sozinho não
foi usado para posicionar o modelo. Os valores de modo de chave permanecem
brutos. O máximo do cabeçalho é 600, mas não foi inferida taxa de quadros,
política de looping ou equivalência temporal ao jogo.

A reflexão nativa `0x6d33a8` associa `mPos` ao offset `+0x40`, `mScale` a
`+0x60` e `mQuat` a `+0x50`. `mAngle` usa getter `0xd00628` e setter
`0x6d336c`. O setter copia XYZ e passa o vetor a `0x6d4080`; este seleciona
entre seis ordens pelo campo `(model+0x38)&0xffff`, calcula a rotação e grava
o quaternion. Por isso o snapshot aceita somente a primeira chave de ângulo
zero, comum às seis ordens, e não implementa a curva de rotação.

Evidências REA, análise estática sem execução do jogo:

- Loader, bytes: `ev_cf15ef9ced602ea27e1024b71131db72bb80a268f7d954e8bf2287f8a4e70507`,
  sessão `1792277213730`, resposta 027; SHA-256 da resposta
  `43161739476955ce2fe347787c9a1e687917ba4136ea8e258cacab22b466519c`.
  Desassemblagem preservada em `rscheduler-loader-llvm.asm`, com hash no
  relatório privado `loader-evidence-manifest.json`.

- Reflexão: `ev_475ea503f38ac2ef74e78667782814bfa2b2230357bfbe3e0a3411fef52362ee`,
  sessão `1792274318294`, resposta 056.
- Setter: `ev_0bac0a409ff231e493047c0d6548d7c6b906c8b85b844f5defe5f1d785448e44`,
  sessão `1792277213730`, resposta 003.
- Conversão: `ev_cad792b05e8b3919087275f4869856a1cb58fdd33b8075bf2c5cd2b1fe6216ea`,
  mesma sessão, resposta 006.

O ELF analisado tem SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Os hashes completos das respostas do setter e conversor estão na proveniência.
O motor nativo não executa esses procedimentos: recebe apenas o snapshot glTF.

## Superfície conectada e início diagnóstico

O SBC 93738 já exportado em `assets/local/collision-probe/v00a01-core2/`
tem um único componente por arestas e índices exatos: 86 triângulos,
oito arestas de borda e nenhuma aresta não manifold. As normais fonte têm
Y entre `0,938463807` e `0,999999583`.

O SBC 93739 foi exportado em
`assets/local/stage-composition-probe/v00a01-walls/`: 90 triângulos,
um componente, normais com Y zero. Sua projeção XZ tem 45 segmentos únicos,
45 vértices de grau dois e um componente fechado. Nenhuma transformação
foi aplicada a qualquer SBC. Isso estabelece relações geométricas; não
atribui walkability, resposta física ou significado aos atributos originais.
As 945 combinações de segmentos não adjacentes foram verificadas sem encontrar
autointerseções no contorno XZ.

`composition-collision-comparison.json` mede a distância dos vértices SBC à
superfície visual composta, preservando a única transformação SDL descrita.
No piso, 16/48 pontos coincidem a até 0,001 unidade e 39/48 ficam a até dez;
a mediana é 1,01944 e o máximo 499,3693. Nas paredes, os vértices de extremidade
ficam entre 234,9839 e 387,2639 unidades da malha visual: esses vértices
estendem o volume vertical de Y≈−264 a 861 e não estabelecem correspondência
com a superfície renderizada. Não foi feito ajuste para aproximá-los.

Para um teste controlado foi selecionado o centroide da face 30 do piso:
`(-61.1420871, -17.2122447, 11963.7447917)` em unidades fonte. Entre os 61
centroides de piso dentro do contorno XZ das paredes, ele tem a maior distância
mínima aos segmentos: `187.2135224`. É uma escolha geométrica explícita de
diagnóstico, **não o spawn original**. Não determina escala/altura do ator.
Um raycast do executável nativo, lançado de Y=100 para baixo, atingiu exatamente
a face 30, com coordenadas baricêntricas aproximadamente `(1/3,1/3,1/3)`.

Os relatórios `collision-93738.json`, `collision-93739.json`,
`diagnostic-floor-point.json` e `diagnostic-point-native-ray.json` preservam
o cálculo e a consulta. O piso e as paredes permanecem recursos separados do
visual; a composição não transforma volumes de depuração em geometria visível.

## Verificações e limites

- Cinco testes de parser, ponteiros, contagens, não finitos, referências nulas,
  seleção de propriedades, rejeição de rotação/escala não comprovadas e
  conectividade.
- 278 verificações de readback: bytes das dependências idênticos, todas as
  tabelas glTF preservadas após remapeamento e transformação contra bytes SDL.
- Khronos Validator 2.0.0-dev.3.10: zero erros, avisos, infos e hints no
  snapshot visual e na colisão vertical.
- Renderer nativo: dois quadros estáticos, captura `capture.png`, saída zero.
  Nenhum processo persistente do usuário foi controlado ou encerrado.
- Raycast nativo no ponto diagnóstico, com saída preservada.

Não foram recuperados o spawn original, transições entre áreas, temporização
dos SDLs ou todas as regras de colocação. `v00a01-hikari` continua excluído
porque depende de TEX formato 16, ainda sem decoder; não foi convertido em
luz presumida. Não houve ajuste visual para encaixar peças ou colisão.

```sh
python3 -B tools/resources/stage_composition.py \
  --index research/private/resources.sqlite --entry 93730 \
  --primary assets/local/stage-probe/v00a01 \
  --detail assets/local/stage-probe/v00a01-a00 \
  --output /caminho/novo/primeiras-chaves
python3 -B -m unittest discover -s tools/resources/tests \
  -p test_stage_composition.py -v
python3 -B research/private/stage-composition/verify_snapshot.py
```

Conteúdo extraído e derivado permanece nos diretórios privados ignorados.
