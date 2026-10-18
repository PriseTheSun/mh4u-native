# Seleção de estado0 por contexto de área

O dono `player_selection` agora resolve os pedidos `0001`, `0015` e `021f`
do ramo acima de `int32(pbw+358)>0x43160000`. Os helpers originais de consulta
executaram até o retorno junto com o comando, setter e transição. As tabelas e
o estado inicial do ensaio são explícitos; isso não identifica a primeira
animação de gameplay nem escolhe um contexto padrão para o driver.

A extensão usa o mesmo `begin_common`/`begin_wrapper` e a mesma continuação
histórica, sem uma segunda decisão no consumidor. Os ramos diretos da
[entrega anterior](player-action-policy.md) continuam disponíveis. O contexto
vazio significa desconhecido, mesmo quando o construtor de um manager teria
inicializado alguns bytes com zero.

## Contrato e propriedade

```cpp
struct AreaMappingRecord {
    std::optional<std::uint8_t> mapped, gate;
};
struct AreaContext {
    std::optional<std::uint32_t> mode_c8c;
    std::span<const AreaMappingRecord> mapping;
    std::span<const std::optional<std::uint8_t>> categories;
};
```

Os spans são emprestados somente durante a chamada. `mapping` é indexado
pelo ID de origem e `categories` pelo ID retornado pelo helper. Um índice
fora do span ou um byte opcional ausente produz `unknown_selection_context`
quando o código original o consome. Não há alocação proporcional à extensão
desses spans: os índices alcançáveis são u8 ou menores que 21.

Foram anexados campos ao estado canônico:

- `PbwFields::field9`: ID original u8, opcional.
- `PbwFields::fields2fc`: 20 halfwords opcionais, em ordem de memória.
- `PwFields::field800`: word original opcional.

`pw+1f4` continua pertencendo a `player_transition::PwFields`; não foi
copiado para um segundo dono. `AreaContext` é argumento final com default
vazio em `begin_common` e `begin_wrapper`, e atravessa
`player_controller_step::apply_common/apply_wrapper` após `Limits`.
`player_animation::CommandContext::area` é o campo final com default vazio.
Nenhuma view externa é guardada no estado retornado ou em `PendingAnimation`.

Os demais gates anteriores permanecem: `pw9a8==0`, `pw7fc!=1`,
`pbw6f9!=1`, `pw594==0` e o limite signed de `pbw358`. Os caminhos
especializados `pw7fc==1` e `pbw6f9==1` ainda retornam
`unsupported_selection_context`. Campo consumido sem valor continua sendo
`unknown_selection_context`; um campo não consumido não é exigido.

## Operações recuperadas

`b40048` passa `u8(pbw+9)` e o ponteiro global de categoria a `6c33d4`.
Este chama `69d568`, que lê o low8 do word de modo global em `+c8c`:

1. Modo diferente de 13 devolve o ID original.
2. Modo 13 e ID zero devolve zero, sem consumir registro de remapeamento.
3. Qualquer outro ID em modo 13 lê `mapped` em `manager+e1cc+4*ID`.
4. ID 1 devolve esse byte. ID >=6 devolve 9, **mas a leitura anterior ainda
   acontece**. ID 2..5 lê `gate` em `manager+e1c7+4*ID`; exatamente 1 devolve
   9, os demais valores devolvem `mapped`.

`6c33d4` lê o byte em `categoryManager+15c+24*mappedID` apenas quando o ID
é menor que 21. Caso contrário, retorna categoria zero. A API representa os
bytes das tabelas por índices e não expõe endereços ARM. O carregamento dos
ponteiros globais em si não vira uma identidade de Resource inventada.

| Categoria retornada | Busca em `d048c4` | Gate de `pw1f4` | Gate de `pw800` | Sem nenhum gate |
| --- | --- | --- | --- | --- |
| 1 ou 2 | halfword `00da` | `&0200` | `&0100` | selector `0015` |
| 3 ou 4 | halfword `00dc` | `&0400` | `&0080` | selector `021f` |
| Qualquer outra | não consumida | não consumido | não consumido | selector `0001` |

Nos dois primeiros grupos, qualquer gate verdadeiro escolhe `0001`, com
curto-circuito na ordem da tabela. `d048c4` compara os 20 u16 entre
`pbw+2fc` e `pbw+323`, em pares ascendentes; para no primeiro igual. Portanto
uma célula desconhecida posterior a um match não impede a decisão, enquanto
uma célula desconhecida anterior impede. Não se atribuiu significado de
ataque, equipamento ou tipo de área aos valores numéricos.

Todos esses caminhos pedem `b27c1c` e depois fazem a continuação
`b1c050(player,0,0,flags)`. Ela é escolhida antes do pedido e preservada,
mesmo que `pw594` ou o contexto mude durante o comando. O estado fornecido
após o pedido continua obrigatório e seus efeitos são preservados.

## Evidência e oracle independente

Retenção: `research/private/player-selection-area-v1/`.
O ELF continua fixado em
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
`source-complete/source-facts.json` conserva 4 readbacks/236 bytes, Evidence
IDs e comandos LLVM. Os corpos vêm das sessões REA `1792300233949` e
`1792301774362`, esta última pertencente ao agente responsável pelo REA.
A fonte da seleção inteira e seus helpers anteriores são reutilizadas dos
harnesses congelados; nenhum novo parser LMT foi criado.

`arm-complete/arm-validation.json`, SHA-256
`9e78143be1afcda5e4f8db03fc744e0ee9f65c2b6c07e68ddbaf34286ff1b7cf`, registra:

- 328 retornos completos originais; 503 chamadas ao setter, 280 aceitas.
- Seletores `0001`: 132 casos; `0015`: 97; `021f`: 99.
- 311.745 instruções, 105.066 leituras e 69.647 escritas observadas.
- Categorias 0..5/255, os quatro modos de dispatcher 0..3, wrappers0/2,
  uma/duas layers, flags0/1/2, pedido já igual, bancos ausentes e conversões
  int32 de interpolação/frame.
- Todas as 20 posições possíveis de match, gates de flags discriminantes,
  ID0/1/2..5/>=6, modo observado pelo low8, gate exatamente1 e ID remapeado
  fora da tabela de categorias.
- Seis omissões consumidas interrompem por guarda de memória, incluindo o
  mapped byte descartado de ID>=6; ELF com digest alterado é rejeitado antes
  de mapear código.

O ensaio usa Unicorn2.1.4/ARM11MPCore apenas como ferramenta privada de
análise. Não há stub, resultado substituído ou PC rewrite no snapshot
canônico. Código e leituras/escritas têm allowlists; toda região opaca do
player permanece protegida e tem hashes antes/depois. Ponteiros de tabelas
são instalações diagnósticas, não endereços de uma sessão do jogo.

Os LMT co_0/entry92608 e co_2/entry92610 são bytes reais fixados por SHA, nos
bancos0/2 explicitamente selecionados da receita original. Seus actions e
eventos são relocados pelo harness; o C++ os lê pelo dono imutável
`motion_resource::Resource`, incluindo identidade do Resource/banco/slot/grupo
nos bindings. Nenhum recurso inválido é transformado em setter aceito.
As curvas `104e048/50/58` usam os 24 bytes pós-inicializador original,
SHA `283bcb5502bd9e7c2daf2245fa36170f8f1f838accd1ed146ff3ba0f3e03b922`,
em vez dos zeros do arquivo ELF antes da inicialização.

A comparação nativa projeta todos os campos declarados de seleção/transição,
as duas layers, máscaras/bindings, pedidos completos, retornos e efeitos do
comando. O estado opaco protegido pelo ARM não é contado como bytes de
resultado C++ produzido. As tabelas emprestadas são verificadas inalteradas.
A fonte do fixture são somente observações ARM, sem reutilizar a fórmula
nativa para fabricar esperados.

## Limites

O template de transição usa contexto diagnóstico explícito com gate local
fechado; a cobertura ampla do statewriter continua pertencendo aos seus
oracles anteriores. `layer.flags&2` impede a consulta de canais antes do
setter neste recorte. Não se executam binder, clock, política completa de
input, scheduler, aquisição do manager, carregamento da receita ou startup
integral. O novo contexto não escolhe automaticamente a área/categoria usada
pelo jogo. Os nomes idle/walk/run não foram atribuídos aos seletores.

## Validação nativa e reprodução

`native-complete/native-validation.json`, SHA-256
`244c88f02044500daa39cdf4bd137694c3d108cfa5ee471236e6c7a44ccbdd89`,
registra **5.342 checks** em cada execução normal e ASan/UBSan, sem diferenças
nos 328 casos. A compilação usa `-ffp-contract=off`, warnings como erros e
`-fno-sanitize-recover=all`; retorno zero e stderr vazio são exigidos. Fontes,
headers transitivos e picosha2 são copiados e têm pins antes/depois.

Também passaram os fixtures históricos, sem regenerá-los: seleção de
estado1/3 (1.892 checks), controlador (257 cadeias/3.854 checks) e estado0
direto (162 cadeias/2.066 checks). O teste histórico de contexto ausente acima
do limite agora espera `unknown_selection_context`; os bytes esperados das
execuções antigas não mudaram. A passagem de `AreaContext` em `player_animation`
é pequena e tem fonte fixada no relatório; o ensaio desta página executa
`player_controller_step`, não a cadeia completa de animação do driver.

`fixture-negatives-complete/validation.json` demonstra rejeição de arquivo
truncado, saída observada com um bit alterado e pin de dono alterado, antes
de gerar o header. São controles do produtor de fixtures; não há alegação
de um novo parser nativo de arquivos. O C++ compila o header gerado:
`generated-final/player_selection_area_fixtures.hpp`, SHA-256
`856475cc4217de4dd1a6b22d89d95efb675fc3e72ce60620897133a9d451afa5`.

Reprodução privada, sem alterar snapshots existentes:

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/player-selection-area-v1/verify_arm.py --output NOVO_DIRETORIO
python3 -B research/private/player-selection-area-v1/validate_native.py \
  --output NOVO_DIRETORIO_NATIVO
```

O teste C++ usa `MH4U_PLAYER_SELECTION_AREA_GENERATED`, includes do fixture
acima e `player-transition-runtime-v1/generated`, e recebe a raiz absoluta do
projeto como único argumento. O snapshot ARM canônico levou 18,366s wall,
18,442s CPU e pico RSS de processo de 454.901.760 bytes no macOS. As compilações
normal/sanitized levaram 41,171s/34,058s; execuções, 0,299s/0,508s. Isso não é
benchmark comparativo; tokens, CPU/memória da família do compilador e execução
Windows não foram medidos por este recorte. A consulta REA usa a sessão
compartilhada do dono e tem seus custos no relatório dessa sessão.

As evidências históricas de estado0 direto e os fixtures de estado1/3
permanecem intactos; o manifesto registra sua relação com esta extensão.
