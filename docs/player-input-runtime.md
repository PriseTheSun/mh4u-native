# Classificadores numéricos de entrada do caçador

O componente `src/player_input.hpp/.cpp` reproduz dois predicados pequenos do
código original: seleção de setor angular por ID exato e classificação de
magnitude. **131.980 execuções isoladas do ARM original** passaram antes da
implementação C++, que reproduziu todos os resultados observados. Não há
mapeamento de teclado/joystick, nomes de movimento ou transição de ação neste
componente.

Esta entrega executa a proposta de [player-input-state.md](player-input-state.md).
A proposta antiga permanece intacta como registro histórico; seus campos
`executed:false` não foram reescritos. A execução e suas evidências têm dono
separado em `research/private/player-input-runtime-v1/`.

## API e domínio

```cpp
#include "player_input.hpp"
namespace input = mh4u::player_input;

const input::Channels channels{{0xe000, 0x8000}, {50, 110}};
const input::SectorScope scope{0, 0};
auto sector = input::in_sector(channels, 0x400, scope); // Result<bool>: true
auto category = input::magnitude_class(channels, 1);   // Result<u32>: 3
```

`Channels` conserva dois ângulos u16 de `pbw+0x3c0/+0x3c2` e duas magnitudes
u16 de `pbw+0x3c4/+0x3c6`. São **campos de estado da fonte**; a identidade dos
dispositivos, a escala física e os produtores desses campos permanecem
pendentes. `SectorScope` exige os bits u32 do modo e o byte de referência em
`pbw+0x451`, sem assumir seus valores silenciosamente.

`Result<T>` é `std::variant<T, Error>`. O erro preserva `ErrorCode` e uma mensagem
estática; `error_name` fornece seu identificador. Nenhuma função aloca memória,
retém referências, modifica os argumentos ou depende de recursos externos. O
trabalho de seleção tem no máximo oito comparações de ID; a classificação de
magnitude tem custo constante.

A precedência observada no dispatcher 0xb23f94 é preservada:

1. Um ID não reconhecido retorna `false` sem consultar canais ou scope. Isso
   vale também quando o modo é 1 ou o byte de referência é não zero.
2. Para ID reconhecido, modo 1 retorna `unsupported_mode`. Esse ramo original
   usa uma virtual e gates ainda fora deste perfil.
3. Para ID reconhecido nos demais modos, referência não zero retorna
   `unsupported_reference`. Não é substituída uma consulta de orientação por
   um resultado inventado. Mesmo com magnitude abaixo de 50, a rejeição ocorre
   antes do teste de magnitude, seguindo a ordem da fonte.
4. No perfil aceito, magnitude menor que 50 retorna `false`. Caso contrário,
   compara `u16(ângulo−L) <= u16(U−L)`, com redução módulo 65.536 e limites
   inclusivos. Os setores adjacentes continuam sobrepostos em suas bordas;
   a prioridade de um caller não foi inventada.

| ID exato | Canal | L | U |
| --- | --- | --- | --- |
| 0x400 / 4 | 0 / 1 | 0xe000 | 0x2000 |
| 0x800 / 8 | 0 / 1 | 0x6000 | 0xa000 |
| 0x1000 / 0x10 | 0 / 1 | 0xa000 | 0xe000 |
| 0x2000 / 0x20 | 0 / 1 | 0x2000 | 0x6000 |

`magnitude_class` implementa apenas a folha 0xb0a41c: 0..49 produz 0,
50..109 produz 1 e 110..65535 produz 3. Canais além de 0/1 retornam
`invalid_channel`. A folha não produz categoria 2. Ela **não equivale** ao
resultado de 0xb417a4/virtual +0x98, que pode retornar zero pelos próprios gates.

## Execução original e guardas

O ensaio `verify_arm.py` usa Unicorn 2.1.4, CPU ARM11 MPCore, modo ARM
little-endian e o ELF com SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Nenhum byte executável foi alterado, nenhuma chamada foi substituída e nenhuma
sessão REA adicional foi aberta. As leituras REA anteriormente conferidas com
o ELF continuam identificadas em `player-input-state/source-readback.json`.

Foram carregados somente três intervalos de código, extraídos de mapeamentos
inicializados `PT_LOAD` únicos do ELF:

| Intervalo, fim exclusivo | Tamanho | Papel |
| --- | --- | --- |
| 0xb23f94..0xb2418c | 504 bytes | Dispatcher de IDs/modos |
| 0xb2ab7c..0xb2ac28 | 172 bytes | Predicado angular e magnitude |
| 0xb0a41c..0xb0a44c | 48 bytes | Classificação de magnitude |

O código tem permissão de leitura/execução. A memória de objetos é sintética,
com bytes inicialmente indisponíveis: só campos explicitamente inicializados
podem ser lidos. Escritas ARM só são permitidas nos 128 bytes de stack abaixo
do SP inicial; elas tornam esses bytes legíveis naquele caso. Nenhuma escrita
no jogador, nos campos ou no código é aceita. Cada caso reinicia os registros
e o conjunto de bytes inicializados; conteúdo remanescente de casos anteriores
não vira memória válida. A função precisa retornar à sentinela de ABI, restaurar
SP e preservar r4–r11. Não se afirma equivalência dos registradores scratch ou
flags da CPU com a API C++.

Há limite de 256 instruções e 100.000 microssegundos por caso. O hook de código
rejeita instruções fora dos três intervalos, inclusive padding mapeado; guardas
de memória rejeitam leitura não inicializada, escrita proibida e acesso sem
mapeamento/permissão. Os casos positivos usaram no máximo 57 instruções.

| Cobertura executada | Casos |
| --- | ---: |
| Proposta anterior, integral | 352 |
| Novos discriminantes: canais divergentes, modos com bit de sinal, CPSR inicial variado e precedência de ID desconhecido | 556 |
| Todos os 65.536 valores de magnitude, em cada um dos dois canais | 131.072 |
| Total positivo | **131.980** |

Os casos conhecidos de setor incluem limiares 49/50, limites angulares ±1,
wrap, ambos canais e valores interiores determinísticos. Os 60 casos de ID
desconhecido passaram com **todos os campos de objeto marcados como não
inicializados**; só leram stack. Os resultados ARM são comparados também à
expressão estática anterior, sem consultar o C++.

Doze controles negativos confirmaram falha nos guardas: quatro leituras não
inicializadas, modo virtual, referência dependente, argumento de referência
forçado, escrita de stack proibida, execução em padding, orçamento de uma
instrução, terceiro canal sem campo inicializado e SHA de fonte incorreto.
Os caminhos excluídos pararam em 0xb24b60/0xb1061c; seus gates não foram
executados nem simulados.

O relatório registra zero divergências e zero acessos fora do escopo nos casos
positivos. Foram executadas 1.221.195 instruções, cobrindo 122 endereços distintos
(488 bytes de instruções), 273.661 leituras de dados e 7.608 escritas de stack.
Não se reivindica cobertura de todos os modos da rotina original: a parte do
código que chega aos gates excluídos continua fora do perfil.

## Validação nativa e reprodução

`arm-fixtures.json` preserva as entradas e saídas dos 908 casos pontuais. As
131.072 saídas da enumeração de magnitude são retidas sem perda como seis
intervalos de resultado constante, mais hashes dos vetores completos u32
little-endian. `generate_native_tests.py` incorpora somente esses resultados
numéricos observados em `src/player_input_tests.cpp`; não incorpora ARM ou ROM,
nem recalcula os valores esperados com o classificador nativo.

Builds standalone com Clang, `-Wall -Wextra -Werror -pedantic`, passaram em
modo normal `-O2` e com UndefinedBehaviorSanitizer
`-O1 -fsanitize=undefined -fno-sanitize-recover=all`.
Cada execução passou **131.980 comparações de observações ARM e 133.127 checks**,
incluindo rejeições e preservação dos argumentos. Os comandos, saídas, versões
e hashes ficam em `native-validation.json`. Não houve execução Windows nesta
etapa; a compilação integrada é conduzida separadamente.

```sh
clang++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic \
  src/player_input.cpp src/player_input_tests.cpp -o /tmp/mh4u-player-input-tests
/tmp/mh4u-player-input-tests
```

A regeneração nativa é determinística a partir do fixture verificado. O ensaio
ARM exige o ELF privado e o ambiente de pesquisa existente; ele recusa
sobrescrever `arm-validation.json`. Para nova execução, copie o harness para
um novo diretório de mesmo nível ou preserve e versione a execução anterior.
A proposta histórica em `player-input-state` não deve ser modificada.

O ensaio ARM mediu 9,72 s de wall time, 9,81 s de CPU e 75.546.624 bytes de pico
de RSS do processo no macOS. Esses valores cobrem o ensaio isolado, não o jogo,
nem uma comparação de desempenho entre ARM e C++. `manifest.json` fixa scripts,
resultados, fontes nativas, documento e artefatos privados por hash.

## Lacunas preservadas

Ainda faltam os produtores de ângulo/magnitude e dos bits de referência, a
resolução de orientação em 0xb1061c, a política da virtual +0x98 e o caminho
completo entre input, `pl_st`, `act_st/act_no` e animação. A classificação não
resolve a ordem entre setores sobrepostos nem constitui um controlador do
caçador. Nenhuma entrada SDL, cena, pacote, rede ou janela persistente foi
alterada nesta entrega.
