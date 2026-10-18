# Seleção numérica de uPlayer em duas fases

`player_selection` reproduz o prefixo de `0xb3fc50` para argumentos inteiros
**exatamente 1 e 3**, conserva o pedido numérico de animação e executa a
continuação usando o estado que o chamador fornece depois desse pedido.
O ramo 3 termina no próprio prefixo. No ramo 1, a API não chama um setter de
animação e não retoma automaticamente com o estado anterior.

O ensaio comparou todos os campos declarados: 12 retornos completos do ramo 3,
108 pares prefixo/continuação e 72 chamadas completas do ramo 1 com o router
original em modos 3, 4 ou `0xffffffff`. Esses modos realmente retornam sem
pedir animação. Os 108 pares **não são chamadas completas com animação**:
são duas execuções ARM separadas, com a memória posterior explicitamente
fornecida no limite. Nenhum helper foi substituído por retorno ou no-op.

## Fonte e observação

A fonte é o ELF ARMv6 LE identificado pelo SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
A retenção nova está em `research/private/player-selection-runtime-v1/`.
`source-readback.json` compara novamente **1.564 bytes** de três respostas
REA já retidas, sem abrir outra sessão:

| Fonte | Evidence |
| --- | --- |
| Wrapper `0xb1d114`, 40 bytes | `ev_6947ff46f28389ba1619d5a83b361d632906a9cd15962d77bc328433b7c8a393` |
| Wrapper `0xb23b74`, 36 bytes | `ev_b7476a0339e1eaea300a53d3a64d68eec404c773b04ef5e6d4a1641c85f216a6` |
| `0xb3fc50`, 1.488 bytes | `ev_d7a8fd5b06fa180e1abf568188a9ddb6538c2b9befc77059a883ca2dab7a1f74` |

O router `0xb27c1c` também é lido do ELF fixado e desassemblado com LLVM;
não houve nova consulta REA sobre ele. As quatro desassemblagens, argumentos
e identidade da ferramenta estão no mesmo relatório. Os bytes e helpers de
`player_transition` vêm de seu dono já verificado, descrito em
[player-transition-runtime.md](player-transition-runtime.md).

## Perfil e ordem dos efeitos

Os wrappers conservam r0–r3 e copiam o quinto argumento como bits. `0xb1d114`
fixa os argumentos de stack `[flags=0, modo=0, quinto_original]`;
`0xb23b74` fixa `[0,2,quinto_original]`. O terceiro argumento de stack não é
consultado nos ramos 1 e 3. O contrato o recebe como `extra_bits`, sem tratá-lo
como float ou atribuir significado.

O prefixo limpa, nesta ordem, `pw+176` (u8), `pw+5b4`/`5b2` (u16) e
`pw+1b0` (bits f32 zero). Se `pbw+31==2`, limpa `pw+170`. Grava o byte baixo
de r1 em `pbw+370` e limpa `pw+a10`. Embora o store seja u8, os testes de
seleção usam o argumento inteiro: 257 não é 1 e 259 não é 3. Ambos são
rejeitados pelo perfil nativo, sem publicação de um prefixo parcial.

Para r1=3, isso encerra a função. Para r1=1, r2/r3 são convertidos de signed32
para binary32 com `VCVT.F32.S32`, formando s0/s1. A API exige
`FE_TONEAREST` nessa fase e o build rejeita `__FAST_MATH__`. O ensaio cobre
INT_MIN/MAX, zero, sinais e arredondamento próximo a 2^24; mantém os bits do
resultado, inclusive o arredondamento de `INT_MAX` para 2^31.

| Campo antes do pedido | Pedido em `0xb27c1c` | Continuação depois do retorno |
| --- | --- | --- |
| `pw+9a8 != 0` | selector 24, modo recebido, s0/s1 convertidos | transição `(0,0,flags)` |
| `pw+9a8 == 0` | selector 8, mesmos parâmetros | **lê então** `pw+594`: zero → `(0,24,flags)`; diferente de zero → `(10,14,flags)` |

A leitura de `pw+594` está em `0xb401c8`, depois do BL de `0xb401c0`.
Mudar esse campo entre as fases altera a transição. `pw+9a8` não é relido
para escolher a continuação: o pedido já escolheu esse caminho. Os fixtures
alteram ambos os campos deliberadamente e incluem 72 mudanças de verdade
lógica de `pw+594` no caminho selector 8.

A continuação marca `pbw+376=1`, chama `player_transition::apply` com os três
argumentos completos e só depois limpa `pbw+6fa`. O oráculo verifica essa
ordem nas escritas e entrada real de `0xb1c050`. Estado anterior, efeitos
globais e observação da chamada retornada de `0xb66f0c` pertencem ao dono
`player_transition`; a seleção não os reimplementa. Seu perfil e erros
continuam aplicáveis. Rejeitar a continuação não altera o estado fornecido;
o prefixo já produzido permanece uma observação separada.

## API e integração

Arquivos públicos: `src/player_selection.hpp`, `.cpp` e `_tests.cpp`.
Namespace `mh4u::player_selection`:

```cpp
BeginResult begin_common(const State&, const CommonArguments&);
BeginResult begin_wrapper(const State&, Wrapper, const WrapperArguments&);
Result<Completed> resume(const PendingAnimation&, const State& post_request,
                         const player_transition::Constants&);
```

`State` compõe `player_transition::State` com somente os campos adicionais
listados no header. Não há segunda interpretação dos campos compartilhados.
Os campos f32 de memória são u32 brutos, preservando representações que o
prefixo não consulta, inclusive NaNs. `CommonArguments` guarda o estado geral,
os dois signed32 e os três argumentos de stack; `WrapperArguments` guarda os
quatro valores variáveis. `Wrapper` admite somente os dois endereços conhecidos.

`BeginResult` é `variant<Completed,PendingAnimation,Error>`.
`PendingAnimation` só pode ser construído pelo produtor: oferece
`before_request()` e `request()` constantes, mantendo flags/continuação
privadas. O pedido contém selector, modo, `interpolation_bits` e `frame_bits`.
É um pedido ao router, **não uma ação glTF resolvida nem setter concluído**.
Não existe callback ou implicitamente um estado posterior.

O integrador deve executar a dependência apropriada e fornecer seu estado
posterior explicitamente a `resume`. Essa chamada também é útil como modelo
puro de continuação com estado declarado; fazê-la assim não prova que a
dependência foi executada. `Completed::transition` é ausente no ramo 3;
quando presente, retém argumentos, flags efetivos e a observação de dispatch
já retornado, nunca um novo comando para despachar.

A compilação depende de `player_transition`. Para fixtures originais, definir
`MH4U_PLAYER_SELECTION_GENERATED` e incluir os diretórios privados
`player-selection-runtime-v1/generated` e `player-transition-runtime-v1/generated`.
O primeiro contém `player_selection_fixtures.hpp`; o segundo fornece os
`Constants` fixados pelo dono anterior. Não se publica texto/tabelas da ROM
junto ao código C++.

## Verificação reproduzível

`verify_arm.py` usa Unicorn 2.1.4, CPU ARM11MPCORE, bytes originais com SHA
fixado, regiões sintéticas preenchidas com valores não zero e campos iniciais
projetados do oráculo de transição fixado. Acesso fora dos campos inicializados,
escrita fora das regiões declaradas e execução fora dos blocos permitidos
falham. Não pressupõe que esse estado sintético tenha ocorrido em uma partida.

Nos pares, o prefixo para **antes da primeira instrução de `0xb27c1c`**.
O segundo ensaio começa no endereço original de continuação com stack e
registradores preservados pelo prefixo, memória posterior declarada e
registradores caller-saved propositalmente diferentes. Isso testa o bloco
real sem fingir execução/retorno da função omitida. Nas 72 chains, o router
é de fato executado e retorna por seu `BX LR` para modo fora de 0..2.

O relatório `arm-final/arm-validation.json` tem SHA-256
`00723ef8ea894192576a5e2761907f79e78faa4835ad788f50fedea64ece7d3c`:
**94.171 instruções, 37.806 leituras e 24.823 escritas** nos casos positivos.
Quatro omissões de memória inicializada são rejeitadas; uma tentativa de
chain modo 0 para no helper ainda fora do perfil `0xb0c198`, em vez de
simular sucesso. Tempo medido: 3,944 s de parede, 3,970 s de CPU e
133.955.584 bytes de pico RSS deste processo no macOS. Cache frio, memória da
família de processos e tokens não foram medidos.

`generate_native.py` exige o hash independente do oráculo, reusa a projeção
dona da transição e só transforma bytes observados em agregados C++.
Não calcula resultados esperados. `validate_native.py` compila snapshots
privados das fontes com otimização normal e ASan/UBSan; ambos passaram
**1.892 assertivas, zero divergências de todos os campos declarados**.
O modo fast-math foi recusado em compilação. O relatório
`native-validation.json` tem SHA-256
`355e147eab0bdb39a5cc86883db707dcc40034764ff1ad2ba02bd1d64d6c625d`.

Para reproduzir, usar um novo destino, conservando os relatórios existentes:

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/player-selection-runtime-v1/verify_arm.py --output NOVO_DIRETORIO
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/player-selection-runtime-v1/generate_native.py \
  --oracle NOVO_DIRETORIO/arm-validation.json --sha256 SHA_OBSERVADO \
  --output NOVO_DIRETORIO_DE_FIXTURES
```

## Dependências restantes

Modos 0/1/2 do router seguem `0xb0c198`, `0xb0c060` ou `0xb0c170` e daí as
cadeias de pedido/associação de animação. Elas não foram executadas neste
recorte; a investigação do setter `0x9575dc` tem dono separado. Uma futura
composição deve preservar os efeitos dessas cadeias antes da leitura tardia
de `pw+594`, sem pressupor não interferência.

Os demais valores de r1, inclusive 2, têm gates adicionais (`0xb07e80`,
virtual +0xac e outros), seleção distinta e possíveis dependências globais.
O perfil rejeita esses ramos; os nomes numéricos não são renomeados como
andar/correr/ataque. Não se demonstraram a política de seleção vinda do
controle, associação selector→slot, transição visual, combate ou online.
A API não foi integrada automaticamente ao viewer/controlador neste recorte.
