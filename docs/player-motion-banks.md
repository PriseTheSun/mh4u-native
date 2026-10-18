# Bancos de movimentos do uPlayer

O selector de movimento só identifica uma ação junto com o contexto dos bancos
carregados. No carregador virtual de `uPlayer` em `0xb5bec8`, banco 0 recebe
`player\mot\co\co_0`, banco 2 recebe `player\mot\co\co_2` e banco 5 depende de
`pbw.pl_type`. Outro carregador, `0x2c9f90`, substitui bancos 0–3 pelos recursos
`lco_*` ou variantes `lco_*p`. Portanto o catálogo anterior baseado em `lco_0`
não deve ser aplicado automaticamente ao dispatcher principal.

A associação entre arquivos e carregadores é estática, a partir dos bytes
originais e de membros reais da ROM. A continuação abaixo executa o setter de
banco completo com os helpers reais de referência em um perfil isolado; não
executa os carregadores completos nem o bootstrap do jogo. O módulo nativo
`motion_bank` reproduz somente a tabela e as flags, com lifetime do recurso
explicitamente pertencente ao chamador.

## Identidade e evidência

- ROM: SHA-256 `c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`, title ID `0004000000126100`.
- ELF derivado: `research/private/mh4u-analysis-bss.elf`, SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
- Sessão REA `research/private/rea-session-1792282296342/`: Ghidra 12.1.4, ARM:LE:32:v6, heap 3 GiB; `Decompiler Switch Analysis` desabilitado. Importação e primeira consulta: 143,054 s. CPU e pico de memória não medidos.
- Bundle, snapshot fechado, manifesto, leitura independente e catálogo em `research/private/player-motion-banks/`. `resource-bindings.json` registra cada caminho pedido, endereço da tabela, candidatos ARC, offsets, hashes comprimidos/descomprimidos e slots LMT. Dados e disassembly do jogo permanecem privados.
- `source-readback.json`: 23 intervalos, 4.892 bytes devolvidos pelo REA comparados exatamente com os segmentos `PT_LOAD` do ELF. Bundle e fechamento retornaram sucesso; processo da sessão terminou com código 0.

A identidade `uPlayer`, sua vtable `0xe6bca4` e `pbw = *(player+0xe30)` reutilizam
[as evidências de reflexão anteriores](player-action-controller.md). `pl_type`
é o nome refletido do byte `pbw+0x31`. O descritor `rMotionList` em `0x10e7568`,
seu inicializador `0xd40bc0` e loader LMT67 em `0x23e3d8` reutilizam
[as evidências do carregamento de animações](rea-findings.md#carregamento-de-animações).
O tipo ARC correspondente é `0x76820d81`.

## Cadeia até a ação

1. `0x9592d4(model, resource, bank)` troca `*(model+0xc04+4*bank)`, liberando a referência anterior e adquirindo a nova. Para cada camada de 0x130 bytes, marca `internal_flags=2` se o banco de seu selector coincide. Não é apenas uma tabela de nomes de arquivos.
2. O setter `0x9575dc` e wrapper `0x9577ec`, já investigados, usam `bank = (selector >> 8) & 15` e `slot = selector & 255`, obtendo o recurso em `model+0xc04+4*bank` e a ação no LMT. `0xffff` tem tratamento separado de selector inativo.
3. Os pedidos dos handlers podem passar pelo remapeamento virtual `uPlayer+0x94` (`0xb15040`). É necessário resolver esse remapeamento e o carregamento vigente antes de escolher o arquivo/slot.

Setter do banco: pseudo/assembly REA 013/014,
`ev_08a0ae14fd5b17903d252f09070dcddc90968d138a6d33988a42e4e58d18f90a`
e `ev_11a29771f29ccb9e8cac28248bb4d3d56f1a85d22d73a5d355410a2e00c72978`.
A leitura original de 136 bytes em 032 foi corroborada contra o ELF.

## Carregador principal

A vtable `uPlayer+0x18` aponta para `0xb5bec8` (bytes REA 050). Ghidra não
reconheceu esse endereço como função no perfil usado (002); por isso este recorte
usa os bytes REA 031 e LLVM ARMv6/VFP2, retidos em
`b5bec8-complete-llvm.asm`. Não há pseudocódigo fabricado para essa função.
Evidence dos bytes: `ev_2acde48b91d2ebaba2692da0d22a42b4bcea8a956b220c5cc8ae04bb90e73d3a`.

O laço `0xb5bee8–0xb5bf6c` indexa a tabela `0xef50e0`. A chamada de aquisição
usa `*(0x10572e4)`, método virtual +0x38, descritor `0x10e7568`, nome da tabela
e quarto argumento 1. Resultado zero sai pelo ramo `0xb5c1b0`; resultado não
zero passa a `0x9592d4` em `0xb5bf50`, com o índice do laço como banco.

| Banco | Nome solicitado | Membro no índice | ARC encontrado | Slots / ocupados |
| --- | --- | ---: | --- | ---: |
| 0 | `player\mot\co\co_0` | 92608 | `/loc/data/pl_base_0.arc` | 136 / 103 |
| 1 | `player\mot\co\co_1` | 92609 | `/loc/data/pl_base_0.arc` | 87 / 54 |
| 2 | `player\mot\co\co_2` | 92610 | `/loc/data/pl_base_0.arc` | 219 / 98 |
| 3 | ponteiro de nome zero | — | não determinado | — |
| 4 | `player\mot\co\co_4` | 92611 | `/loc/data/pl_base_0.arc` | 24 / 10 |

O banco 3 só chega à aquisição quando o byte `(*(0xfb6b7c))+0x5a` vale 1; a
entrada da tabela é literalmente zero. Não foi determinado o efeito de pedir
um nome nulo ao gerenciador. Banco 4 depende do retorno de `0x69a870` chamado
com `*(0xfbeffc)`; retorno zero pula sua aquisição. Não damos nomes de gameplay
a essas condições.

### Banco 5

`0xb5bf70–0xb5bfbc` indexa `0xef5144` pelo byte `pbw+0x31`, pede `rMotionList`
e chama `0x9592d4` com banco 5 em `0xb5bfb0`. Abaixo estão as 16 entradas da
tabela até a tabela seguinte `0xef5184`. O trecho não valida o domínio de
`pl_type`; a presença da tabela não prova que todos os valores são alcançáveis.

| `pl_type` | Sufixo de `player\mot\` | Membro(s) | ARC(s) encontrado(s) |
| ---: | --- | --- | --- |
| 0 | `w00\w00` | 63432 | `/data/pl_w00.arc` |
| 1 | `w01\w01` | 63470 | `/data/pl_w01.arc` |
| 2 | `w02\w02` | 63490 | `/data/pl_w02.arc` |
| 3 | `w03\w03` | 63520 | `/data/pl_w03.arc` |
| 4, 15 | `w04\w04` | 63553, 64667 | `/data/pl_w04.arc`, `/data/pl_w15.arc` |
| 5, 6 | `w06\w06` | 63704, 63861 | `/data/pl_w05.arc`, `/data/pl_w06.arc` |
| 7 | `w07\w07` | 64018 | `/data/pl_w07.arc` |
| 8 | `w08\w08` | 64041 | `/data/pl_w08.arc` |
| 9 | `w09\w09` | 64071 | `/data/pl_w09.arc` |
| 10 | `w10\w10` | 64108 | `/data/pl_w10.arc` |
| 11 | `w11\w11` | 64279 | `/data/pl_w11.arc` |
| 12 | `w12\w12` | 64313 | `/data/pl_w12.arc` |
| 13 | `w13\w13` | 64332 | `/data/pl_w13.arc` |
| 14 | `w14\w14` | 64491 | `/data/pl_w14.arc` |

As duas cópias `w04` são idênticas entre si, assim como as duas `w06`. O código
examinado pede o nome do membro, não o nome ARC. Não foi observado qual ARC o
gerenciador escolhe entre candidatos; a igualdade dos bytes não apaga essas
identidades de origem. Não foi inferida a classe de arma a partir do número.

## Substituição por `lco_*`

`0x2c9f90` liga bancos 0–3 num laço em `0x2c9fe8–0x2ca058`. Usa a tabela
`0xef5108` quando ambos os testes são verdadeiros:

- o byte **com sinal** em `(*(0xfb6b7c))+0x2f` iguala o byte sem sinal `pbw+0x33`;
- o byte em `(*(0xedbe90))+0x978b` vale 8.

Caso contrário usa `0xef50f4`. A aquisição usa o mesmo descritor e método
virtual do carregador principal. Resultado nulo interrompe o laço; resultados
anteriores já foram ligados, portanto não se pode presumir troca atômica dos
quatro bancos. O laço observado não escreve bancos 4/5.

| Banco | Tabela `0xef50f4` / membro | Tabela `0xef5108` / membro |
| ---: | --- | --- |
| 0 | `player\mot\co\lco_0` / 54913 | `player\mot\co\lco_0p` / 54917 |
| 1 | `player\mot\co\lco_1` / 54914 | `player\mot\co\lco_1` / 54918 |
| 2 | `player\mot\co\lco_2` / 54915 | `player\mot\co\lco_2p` / 54919 |
| 3 | `player\mot\co\lco_3` / 54916 | `player\mot\co\lco_3p` / 54920 |

A primeira família está em `/data/pl_mot_lco.arc`, a segunda em
`/data/pl_mot_lcop.arc`. As duas cópias `lco_1` são idênticas. A função recebe
um objeto com `pbw` no mesmo offset; seu caller observado `0xaf7394` obtém o
objeto via `0xa095d0(*(0xedbe90))` e só chama o loader se não nulo. A origem
completa desse objeto e a política que chama o wrapper continuam desconhecidas;
nomes de cenários ou modos não foram atribuídos por abreviação dos arquivos.

Pseudo/assembly 015/016:
`ev_17c58cd747875a76363f0e335b241b4faf602ddf7485c9881b59bd6ae1001673`,
`ev_45736b0279aab88d1948a46495c0bc1976fc93ce9fb6e20cabe34bb2fe3d4b1b`.
Caller 061/062 e bytes 036/037 preservam a ligação e os literais.

Há ainda uma substituição comprovada de banco 0 por `player\mot\co\pl_make`
na função `0xb77d1c`, chamada a `0x9592d4` em `0xb78110`, nome obtido da tabela
`0xef511c`. Pseudo/assembly 055/056, Evidence
`ev_e0f2bf8bdde94e89457c23345695fd7f254930290de9e2c753af82577e7a6e64`.
O índice encontra cópias idênticas em `core_title.arc` de eng/fre/ger/ita/spa,
IDs 67191/70047/73084/76121/110744. A função constrói o objeto por
`0xb0c444`/`0xb0c1d4`; não tratamos o banco 0 desse objeto como sendo o banco 0
de toda instância `uPlayer` em qualquer fase.

## Comparação com os selectors já encontrados

A associação numérica agora permite verificações concretas, sem nomear ações
pela pose. Frames e loop abaixo são valores brutos do LMT67; domínio de amostra
termina em `frame_count−1`, conforme a investigação temporal anterior.

| Selector e contexto | Recurso/slot | Conteúdo observado |
| --- | --- | --- |
| `0x0001`, banco principal 0 | co_0/1 | 199 frames, loop 0 |
| `0x0005`, banco principal 0 | co_0/5 | 275 frames, loop 144 |
| `0x0203`, banco principal 2 | co_2/3 | 267 frames, loop 62 |
| `0x021f`, banco principal 2 | co_2/31 | 251 frames, loop 68 |
| `0x0203` ou `0x021f`, banco alternativo 2 | lco_2 ou lco_2p / 3 ou 31 | slots vazios |
| `0x0502`, `pl_type=0` | w00/2 | 45 frames, loop −1 |
| `0x0502`, `pl_type=4` ou 15 | w04/2 | 121 frames, loop −1 |
| `0x0523`, `pl_type=4/5/6/15` | w04 ou w06 / 35 | 41 frames, loop −1 |
| `0x0528`, `pl_type=5/6` | w06/40 | 37 frames, loop −1 |

Nos demais recursos de banco 5 catalogados, slot 35 está vazio; slot 40 está
vazio em todos exceto w06. Isso é consistente com os gates do handler anterior,
mas não prova a política completa de escolha nem os nomes de ataque/locomoção.

## Validação e próximos pontos

O coletor privado `catalog_selected.py` usa os donos existentes
`index_resources.extract` e `animation_probe.parse_lmt`. Verificou 33 membros:
bounds e hash comprimido contra o índice, descompressão exata, versão 67,
tabelas/ações/tracks por parser. A ROM inteira não foi relida para hash nesta
etapa; sua identidade integral é a previamente validada no índice. O relatório
registra os hashes de entrada e dos parsers para invalidar resultados obsoletos.

Próximas perguntas mínimas antes de integrar o controlador:

1. A continuação identifica chamadas virtuais +0x18 no gerenciador de unidades e distingue a classe base da criação. Ainda falta a ordenação global que torna cada família vigente; a ausência de callers detectados pelo Ghidra não prova que uma rotina seja inalcançável.
2. Resolver a política de aquisição do gerenciador +0x38 e a montagem de ARCs somente se a identidade física escolhida for necessária; todos os nomes duplicados deste recorte produziram LMT idêntico.
3. Combinar banco vigente, `pl_type`, remapeamento virtual e gates de estado antes de ligar um selector à ação. Não converter flag do cabeçalho LMT em atributo runtime, nem atribuir `idle`/`walk`/`run` pela aparência.

As consultas de input solicitadas pelo outro investigador foram retidas na mesma
sessão (017–030, 038–049), evitando uma segunda importação. A interpretação
input→estado pertence a `docs/player-input-state.md`, não a este catálogo.


## Continuação: operação utilizável e contexto de criação

A receita verificável fica em
`research/private/player-motion-banks-v1/source-reviewed/facts.json`
(SHA-256 `83951233fc6c339690c82d30246af7b70708b763391da202bee838dabfffaf1d`).
O produtor `collect_facts.py` verifica novamente o catálogo anterior, a identidade
integral do índice, os 33 LMT retidos e as tabelas do ELF. Os 33 membros representam
26 conteúdos distintos e 1.460 slots ocupados contando cópias de recursos; isso
não representa todas as animações do jogo. O parser LMT existente é o único dono
da interpretação. Não houve nova leitura integral da ROM nem extração nesta etapa.

Cada operação da receita contém banco, tabela/call site, gate, candidatos ARC,
SHA do conteúdo e política de interrupção na aquisição nula. Ela deve ser aplicada
como sequência de substituições, preservando bancos não escritos. `bank5_choices`
contém as alternativas para `pl_type`; não é uma sequência que instala todas.
Não existe receita selecionada automaticamente, e nenhum banco não observado é
convertido em ausente. O arquivo entrega caminhos locais e hashes diretamente
utilizáveis por `motion_resource::Resource::load`.

### Classe base da criação e pedidos condicionais

O construtor `0xb0c1d4`, usado por `0xb0c444` em `0xb77d1c`, instala a vtable
`0xe6bc00` e chama `0x9574dc(this,2)`. O construtor-base de modelo reserva oito
camadas e começa com quatro ativas; o objeto da criação termina com duas ativas
neste caminho estático. O `uPlayer` derivado usa outra vtable, `0xe6bca4`.
Não são contextos intercambiáveis.

Na classe da criação, virtual +0x18 é `0xb0b93c`. Seu corpo completo apenas
solicita o descritor `0x10e7508` para certos valores de `pbw+0x30` e grava o
resultado em `object+0xd84`; não escreve bancos de movimento. Portanto essa
chamada virtual não executa o carregador `uPlayer→co_0` observado anteriormente.
O gerenciador de unidades possui chamadas virtuais +0x18 em `0x644ffc` e
`0x645cb4`, em ramos que exigem, entre outros gates do manager, `flags4&7==1`,
alterando esses três bits para 2 antes da chamada. Isso
estabelece a ligação estrutural do dispatcher, não a ordem de uma sessão viva.

O objeto que `0xb77d1c` armazena em `*(0x109e324)+0xc` é também o objeto usado
por `0xb7ade8`. Nesse último, o caminho com `u8(Q+4)==0`, retorno não zero de
`0x660378`, `u8(pbw+0x93d4)==1` e `u8(Q+0x110)==0` grava o byte `Q+0x110=1` e emite:

| Gate adicional | Call site | Wrapper | Selector | Banco/slot de `pl_make` |
| --- | --- | --- | ---: | --- |
| `u8(pbw+0x34)!=0` | `0xb7af34` | `0xb0c060` | 170 | 0/170 |
| `u8(pbw+0x34)==0` | `0xb7af68` | `0xb0c060` | 190 | 0/190 |

Os argumentos são interpolação +0, frame inicial +0, flags 0; o wrapper
comprovado fornece velocidade 1 e encaminha a camada 0, incluindo o comportamento
já documentado da segunda camada. Os dois slots existem nas cinco cópias idênticas
de `pl_make`, com 201 frames e loop bruto 0. O SHA do LMT é
`1841d5b5552e02d8c1e108c355dbdf72121308fcca67ff7b70dc7b920a43fc9f`.
A receita conserva todos os cinco membros candidatos; não escolhe um ARC físico
como se sua resolução tivesse sido observada. Não atribuímos gênero ao byte +0x34,
nem afirmamos que esse seja o primeiro pedido de toda sessão ou do gameplay.
Há helpers de preparação entre esses gates; eles não foram executados como cadeia
completa aqui. O driver deve fornecer o contexto e os recursos de origem.

REA da continuação: sessão `1792298838065`, consultas 020–029, 038–042 e
072–078. `0xb7ade8` tem Evidence
`ev_ba9ce799e49a009da134492cea677825b76eda4dad5ce93f7a95c88bcd28eb84`.
O novo coletor compara dez readbacks, 4.416 bytes, com o ELF; os IDs, parâmetros,
hashes de payload e arquivos estão em `facts.json`. Os intervalos adicionais
obtidos diretamente por LLVM são identificados separadamente como fonte estática.

### Lifetime original e fronteira nativa

`0x9592d4` executa esta ordem, mesmo se o recurso novo for o mesmo ponteiro:

1. Se o banco antigo não for nulo, chama `be20c8`, depois grava zero no banco.
2. Se o recurso novo não for nulo, chama `be2094` e depois grava o novo ponteiro.
3. Para cada camada ativa, extrai `(selector>>8)&15` e, quando coincide, **substitui**
   `flags` por 2. Não é OR. O selector `0xffff` coincide com banco 15 neste setter.

`be2094→c159b8` incrementa o word de referência em `resource+0x48`.
`be20c8` usa o manager em `*(0x10572e4)` e seu virtual +0x48. A vtable original
`0xe6cc0c` contém `c164a8`, que decrementa esse word e chama destruição quando
chega a zero. Com contador anterior pelo menos 2 e locks local/global zero,
essa função retorna sem outros helpers. As chamadas de aquisição nos loaders
mantêm uma referência local e a liberam depois da instalação. Não modelamos
numericamente o gerenciador inteiro ou sua política de threads/destruição.

O oracle `arm-lifetime/oracle.bin` (SHA-256
`6702b6627069465dff2d6441bc5432440ad98766b32414e4bdc759b65900fd4b`) contém
400 execuções completas do setter com esses helpers reais e a vtable original,
sem stubs, substituição de resultados ou reescrita de PC. Cobre 16 bancos,
contagens ativas 0/1/2/4/8 e cinco operações: nulo→nulo, instalar, remover,
trocar e reinstalar o mesmo ponteiro. Compara arenas completas do modelo e dos
recursos e a ordem de cada store. Onze controles rejeitam leituras omitidas,
execução fora do perfil, escrita negada e orçamento insuficiente, incluindo
locks ativos e liberação até zero. Os contadores e a instância do manager são
entradas diagnósticas explícitas; não foram capturados do jogo em execução.

A API em `src/motion_bank.hpp` é:

```cpp
motion_bank::Result replace(
    const motion_request::Model& model,
    std::span<const motion_pending::Layer> layers,
    std::size_t active_count, std::uint32_t bank,
    const motion_request::Bank& replacement,
    motion_bank::Limits limits = {});
```

O resultado possui `model` e cópia de todas as `layers`. Só as camadas ativas
participam da marcação; inativas, canais, cursores, valores float brutos,
bindings de eventos e identidades lógicas antigas ficam preservados. Índices,
extents, limites de cópia, replacement desconhecido e identidade zero produzem
erros tipados sem publicar estado parcial. O limite padrão 8 camadas/256 juntas
por camada é uma política configurável de cópia, não um máximo geral do formato.

Essa API representa apenas tabela/flags. Não executa acquire/release/destruição
ARM, não cria um segundo dono de recursos e não consome pending. Os bancos
emprestam spans do `Resource`; o chamador deve manter vivos tanto os snapshots
nos bancos atuais quanto os referidos por canais/bindings antigos. Reinstalar a
mesma identidade ainda marca pending. Um banco antigo desconhecido pode ser
substituído numericamente; isso não estabelece sua política de liberação.

### Comparação e limites

A comparação nativa usa os mesmos 400 resultados originais, 22.021 verificações
por variante, mais 15 fronteiras no modo sem fixture. As duas variantes são
normal e ASan/UBSan com `-fno-sanitize-recover=all`, stderr vazio e contagens
assertadas. Os controles negativos de processo alteram digest, versão, contagens,
extents, dados de saída e bytes opacos, exigindo erro conhecido. O relatório final
é `research/private/player-motion-banks-v1/native-final/native-validation.json`.

A projeção compara 32.000 bytes dos campos de tabela/flags; os 3.244.800 bytes
restantes do modelo são representação opaca ou emprestada preservada. Além disso,
1.811.200 bytes de serialização de fatos canônicos verificam todos os membros
C++, inclusive identidades de 64 bits sem equivalente em ponteiro ARM. Não somamos
essas categorias como se todos fossem payloads novos produzidos pelo motor.
As arenas de refcounts são prova do oracle original, não uma implementação C++
de refcount. Fontes, headers transitivos, executáveis e fixture são pinados antes
e depois; cópias das fontes ficam no relatório privado.

A operação agora permite instalar/trocar um banco no driver sem marcar flags à
mão. A escolha de qual receita está ativa, o primeiro estado do controlador e a
ordenação de todos os carregadores continuam dependências separadas. A inspeção
pode usar uma receita explícita; não deve chamá-la de bootstrap original completo.
