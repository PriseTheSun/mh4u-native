# Perfil executável da transição original

O perfil local executa `0xb1c050` do início até o retorno, incluindo os helpers
reais alcançados, a virtual concreta de `uPlayer` e o tailcall. **578 estados
sintéticos explícitos** passaram no ARM isolado; a implementação C++ reproduziu
todos os campos declarados, flags finais e argumentos de chamadas observadas
sem divergências. Isso não constitui uma captura de estado do jogo nem fecha
o controlador jogável. O módulo não está integrado ao viewer ou a gameplay.

A investigação parte de [player-transition-state.md](player-transition-state.md)
e mantém a distinção entre o escritor de estado e a política que decide
quando pedir uma transição. `global+0x5d=0` é uma precondição do perfil, não
uma suposição sobre o estado original de uma partida. O caminho com esse byte
não zero continua rejeitado.

## Fonte e dependências executadas

O ELF tem SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
`research/private/player-transition-runtime-v1/source-readback.json` retém
17 extratos de código/dados e 40 respostas das sessões REA `1792282296342`,
`1792283675798` e `1792285120229`. Duas respostas preservam a falha de resolução
da virtual como função; seu corpo foi lido nos bytes, sem atribuir o corpo da
função vizinha. As leituras REA verificáveis somam 1.424 bytes idênticos ao ELF.

| Trecho | Efeito recuperado e limite |
| --- | --- |
| `0xd04924` | Gate numérico por campos de global/pbw; comparação signed do byte global+2f fora do modo2 |
| `0xd0473c` | Classificação do byte pbw+31 nos conjuntos numéricos já documentados |
| `0x2b555c` | Atualização do u16 global+22; mesmo valor usado depois em pw+bc, com preservação opcional do byte baixo |
| `0x10b778 → 0x2fef54` | Limpeza real de 48 bytes pw+10a; nenhum memset foi interceptado |
| `0xb04e40` | `ADDS r0,r0,#0x370; VSTRNE s0,[r0,#0x48]; BX LR`; soma ao argumento, sem leitura de ponteiro em +370 |
| `0xb151b4 → 0xd047b4` | Tabela original em 0x100c4fa, dois bytes por tipo; teste de quatro bytes pbw+2f8..2fb e multiplicações binary32 em ordem |
| `0xb23f94 → 0xb2ab7c` | Consultas 0x800 antes de 0x400, com modo0 e reference_flag0 |
| Virtual `uPlayer+0x134` | Slot real em 0xe6bdd8 aponta 0xb31c64; corpo real é `BX LR` |
| `0xb66f0c → 0x2b5998` | A chamada entra de verdade; global+5d igual a zero faz o helper retornar e a função sair antes do corpo externo |
| `0xb6cdf0` | Tailcall real faz OR de 0x20000000 em pw+f0 |

O REA confirmou os helpers adicionais com Evidence
`ev_aef15dc2bbac82ff919a0287134b60024ce2815960dd128dae8a907594396409`
(`d047b4`),
`ev_2ff407519599e5465d9294848a7e05ac5d7ede33cc3cc55776ec2294926d521f`
(`2fef54`) e
`ev_31f1ecee999097e0dbd21691bded1fa7a8343173a83d820aec8cf3f4b99eeae9`
(`2b5998`). As respostas completas, comandos LLVM e hashes individuais ficam
no diretório privado `sources/`. Os bytes executados vêm do ELF fixado.

O ajuste de `b04e40` é relevante: no caller, r0 é o player e s0 contém zero,
portanto o destino é **player+0x3b8**. Não se lê `*(player+0x370)` nem se grava
em outro objeto através desse campo. O `NE` depende da soma, não de um
ponteiro carregado. A instância sintética escolhida não produz wrap para zero.

## Escopo admitido e rejeições

O harness declara um objeto `uPlayer` com ponteiros sintéticos explícitos para
pbw/pw/global, mas usa o endereço e conteúdo do slot virtual original. As
regiões começam preenchidas com bytes pseudoaleatórios não zero; somente
campos identificados recebem permissão de leitura. Estados zerados não são
apresentados como estado original válido.

O perfil exige tipo pbw+31 em `[0,15]`, pois somente os primeiros 32 bytes da
tabela foram retidos para essa interface. Isso é um limite da cobertura,
não uma alegação sobre o tamanho completo da tabela original. Exige também
global+5d=0. Quando `d04924` retorna verdadeiro:

- Tipos 4, 5, 6 e 15 exigem pbw+3d4 com bit 0x10000000, evitando os helpers
  condicionais `b33778`/`ca550c` por um gate real.
- Com estado anterior7, admite somente novo argumento de estado exatamente7;
  sair desse estado precisa de `c017f0` e estado global adicional.
- Tipo13 é rejeitado antes de `cbf7b0`/`33ecc8`.
- Flags com bits 0x9000 exigem reference_flag0 para as consultas angulares.
  O módulo separado de referência pode ampliar isso depois, por coordenação.

Os seis limites foram testados em duas camadas: a validação rejeita o perfil,
e a execução ARM sem essa validação alcança exatamente código ou memória fora
do escopo declarado. Isso verifica que os gates não foram apenas substituídos
por respostas fixas. Há ainda quatro testes omitindo memória necessária e um
teste de orçamento de uma instrução: todos falham explicitamente.

O tipo13 com gate falso é coberto; seu helper externo não é alcançado. Não se
conclui que todas as combinações possíveis dos campos admitidos foram
enumeradas. A cobertura exata é o conjunto retido de 578 casos.

## Argumentos completos e ordem dos efeitos

O estado anterior de pw+f0/f4 é copiado antes de ser limpo. Os estados antigos
de pbw+371/372 vão para +377/378; somente as escritas dos novos valores reduzem
os argumentos a u8/u16. Os testes posteriores usam os 32 bits originais:

| Argumentos state/action | Armazenamento | Chamada observada a b66f0c no perfil |
| --- | --- | --- |
| 0 / 147 | 0 / 147 | selector9 |
| 0 / 65683 | 0 / 147 | selector1 |
| 256 / 147 | 0 / 147 | selector1 |
| state6 | byte6 | Pode copiar pw+4c2 para pw+504 |
| state262 | byte6 | Grava 65535 em pw+504 |

Flags0x9000 são consumidos e removidos do argumento de flags durante o ramo
angular antes da chamada posterior. O tipo seleciona um deslocamento numérico;
o setor0x800 é consultado primeiro e o0x400 apenas se necessário. As operações
em pw+b0 usam wrap de u32, salvo as duas atribuições anteriores que explicitamente
reduzem o resultado a u16. Não se converteu isso em direção de câmera ou
comando físico de joystick.

A atualização global ocorre antes de pw+bc. O cálculo mantém o produto
u32, a acumulação u64 do `UMLAL`, o deslocamento e a redução final a u16. Na
escala dos bytes pw+290/291, cada multiplicação arredonda separadamente para
binary32 e a conversão final trunca para u32 antes da escrita do byte. Os
casos incluem power255 e variantes dos flags multiplicativos; não se usa
`pow` de host nem se agruparam multiplicações.

## API C++ e constantes locais

Namespace `mh4u::player_transition`, arquivos `src/player_transition.hpp/.cpp`:

```cpp
Result<Constants> Constants::decode(
    std::span<const std::uint8_t>, const ExpectedConstants&);
Result<Output> apply(const State&, const Request&, const Constants&);
```

`State` contém estruturas `PlayerFields`, `PbwFields`, `PwFields` e
`GlobalFields`, com larguras numéricas e nomes por offset. Pointers e vtables
arbitrárias não são aceitos. `Request` conserva `state_arg`, `action_arg` e
`flags_arg` como u32. `Output` devolve uma cópia do estado final, `effective_flags`
e um optional `ObservedDispatchCall{selector,flags_argument}`.

**`returned_dispatch_call` registra uma chamada já executada que retornou
pelo gate original. Não é um comando para despachar depois.** Dar esse valor
a um sistema de gameplay criaria efeitos que este perfil não demonstrou.

A validação acontece antes de produzir o novo estado. Ausência de dependência
ou gate não suportado gera `ErrorCode` explícito, sem mutação do argumento.
As consultas angulares reutilizam `player_input::in_sector`; não há segundo
classificador. O ambiente precisa de float IEEE binary32 e `FE_TONEAREST`.
Uma compilação com `__FAST_MATH__` é rejeitada. Os operandos multiplicativos
desse perfil não são subnormais; não se extrapola isso para outras rotinas.

As constantes são um payload little endian de 88 bytes `MH4UPT01`: magic,
SHA do ELF em 32 bytes, quatro words de constantes e 32 bytes da tabela.
O decoder exige tamanho exato, SHA selecionado externamente, versão, identidade
do ELF e variante numérica suportada. A tabela não está embutida no código
público. Saídas privadas em
`research/private/player-transition-runtime-v1/generated/`:

- `player-transition-constants.bin`, SHA-256
  `1cf3b6e6873f77008eb072e18e27579e6bec375622c7d4780413f06f1cee3d5f`.
- `player_transition_constants.hpp`, com
  `mh4u::player_transition::generated::constants_bytes` e
  `generated::expected_constants()`.
- `player_transition_fixtures.hpp`, esperado gerado a partir das saídas ARM.
  O gerador não calcula o estado esperado com o runtime nativo.
- `manifest.json`, incluindo esquema de projeção e verificação de que cada
  byte declarado, salvo os três pointers fixos do harness, está representado.

## Validação executada e limites

O ARM isolado executou **300.754 instruções**, 112.722 leituras e 70.963
escritas nos 578 casos. Foram observados 635 PCs distintos. Cada caso chama
`d04924` quatro vezes, o helper de atualização global, a limpeza real, o
escalador de bytes, a virtual real e o tailcall. A chamada opcional b66f0c e
seu gate real ocorreram em 70 casos. Não houve interceptação de helper,
substituição por no-op, acesso a serviço ou execução do jogo completo.

O harness verifica retorno, stack, registradores preservados, todas as
leituras/escritas permitidas e mudanças nas regiões completas. Retém hashes
dos estados completos, campos antes/depois, chamadas com argumentos e digest
do trace. Para C++, a comparação inclui todos os campos tipados declarados,
inclusive os que devem ficar inalterados. Bytes de padding do objeto original
não são inventados como membros do motor; no ARM eles foram verificados como
inalterados fora dos spans de escrita permitidos.

Oracle final: `arm-final/arm-validation.json`, SHA-256
`1987ec66f10cc327d12300ddb3928cbf7ba8afe7e8fea2c033ed05b8a79c0842`.
A execução levou 10,738 s de parede, 10,811 s CPU e pico RSS do processo de
85.032.960 bytes em macOS ARM64. Não se mediram memória agregada da família,
cache frio ou tokens. Os relatórios de tentativas anteriores foram preservados
como diagnóstico; somente `arm-final` é o oracle selecionado.

Os testes nativos incluem o estado completo dos 578 casos, rejeições de
perfil sem mutação, divergência de identidade, constantes alteradas,
truncamentos nos 88 limites, bytes excedentes, arredondamento de host
incompatível e rejeição de fast-math na compilação. Logs, hashes de fontes
exatas compiladas e resultados normal/sanitizado ficam em
`native-validation.json` e `native-final/`. O corpus anterior de transições,
pacotes e snapshots não foi regravado.

Continuam em aberto: origem de todos os campos em uma partida real, política
de seleção dos pedidos, corpo externo de b66f0c com gate aberto, helpers
opcionais listados, outros tipos/tabelas virtuais e sincronização de gameplay.
Executar o escritor dentro desse perfil não autoriza chamar um clip de andar,
correr ou atacar, nem implica que uma ação completa do jogo foi portada.
