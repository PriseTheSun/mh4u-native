# Orientação Euler original, perfil uint16

`player_orientation::calculate(Angles, optional<uint16_t>)` implementa a composição numérica de `b05350 → 6d4080` com o virtual `+80` concreto `b05328` (retorno zero). Recebe três ângulos já estreitados a 16 bits e modo Euler explícito de 0 a 5; devolve quatro palavras binary32 do quaternion, sem mutar Player. Modo ausente é `unknown_mode`, e modo fora de 0–5 é `unsupported_mode`. Nenhum deles vira modo zero. Não descobre o modo, a vtable ou a orientação inicial do jogador.

A fonte é o ELF SHA256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Os readbacks REA 071–075 da sessão `1792310616701` confirmam a orientação, a região contínua de Euler e os virtuais concretos; a prova anterior está em `research/private/player-position-bridge-v1/source-complete/source-facts.json`. A região contínua inclui lacunas que o corpo parcial reconhecido pelo Ghidra não cobria. As funções trigonométricas `d13f14`, `d12be0` e a classificação `d119d4` foram lidas diretamente do mesmo ELF, com intervalos e digests no relatório do novo oracle. Isso distingue readback REA de leitura estática local.

## Cálculo e domínio

A conversão de uint16 para float é exata. A fonte multiplica, com arredondamentos separados, por `0x40c90fdb` e `0x37800000`. A implementação reproduz a redução e os polinômios da fonte; não chama seno/cosseno da biblioteca do host. MUL e ADD permanecem separados, inclusive nas instruções VMLA originais. Os seis modos constroem a matriz na ordem observada e usam a conversão matriz→quaternion, raiz e divisão da fonte. Se W é negativo, os quatro componentes recebem o sinal invertido. Não há normalização final adicionada.

A enumeração estática de todos os 65.536 argumentos fica em `source-final/source-contract.json`. Os ângulos 0–2 seguem o caminho minúsculo; 3–8191 usam os polinômios diretos; 8192–65535 usam redução média com inteiro 0–4. A redução geral de argumentos grandes, entradas negativas, NaN/Inf e o caminho de argumento subnormal não são alcançáveis após a conversão uint16 deste contrato. Essa enumeração é derivada da fonte e não substitui a execução ARM.

O perfil nativo requer binary32, arredondamento nearest-even e underflow gradual, verificados pelo dono `motion_transform_math`. `__FAST_MATH__` é recusado. Operações consumidas podem produzir erro aritmético tipado; o módulo não preenche quaternion zero em falha. O uso de `sqrt(float)` e divisão preserva a ordem da fonte e foi comparado bit a bit no corpus executado, sem alegar prova de todos os triplos possíveis.

## Verificação executada

O oracle independente executou os corpos originais completos, inclusive as trigonométricas e o virtual concreto, sob Unicorn 2.1.4/ARM11MPCORE. Não substitui funções nem reescreve PC. Guardas restringem fetch, leituras inicializadas, escritas e orçamento; verifica preservação dos registradores callee-saved e de todos os bytes do modelo fora do quaternion. Unicorn é ferramenta privada de análise; o módulo C++ não contém runtime ARM ou interpretador de instruções.

O snapshot autoritativo `arm-complete` contém 69.076 casos: varredura dos 65.536 ângulos no eixo X com modos distribuídos, 468 casos de limites nos três eixos/seis modos e 3.072 triplos mistos determinísticos. Ele não é exaustivo no domínio de três ângulos. Outros 144 checkpoints de orientação (26 entradas distintas) foram extraídos das 150 execuções anteriores da ponte, sem repetir ARM. O piloto de 1.024 casos é histórico e não é somado à cobertura.

| Verificação | Resultado por perfil nativo |
|---|---:|
| Oracle novo | 69.076 casos; 1.105.216 bytes de quaternion exatos |
| Checkpoints preservados da ponte | 144 casos; 2.304 bytes exatos |
| Fronteiras | 23 checks |
| Total | 1.038.333 checks |

Normal e ASan/UBSan/float-cast-overflow passaram com recuperação desativada, stderr vazio e contagens verificadas. Onze processos negativos por perfil recusam hash, comprimento, schema, contagem, modo e saída adulterados. O oracle ARM também contém seis controles de guarda. Seus contadores de instruções/leitura/escrita incluem esses controles, portanto não são atribuídos apenas às consultas positivas.

A execução ARM completa mediu 34,559 s de parede e 38,877 s de CPU; o relatório preserva RSS e contadores. Não medimos desempenho do jogo nem execução Windows. Os comandos, fontes transitivas via `clang++ -MM`, snapshots e digests antes/depois estão em `native-final/validation.json`. Hash do relatório: `a94f4bdd7c5626acaaa3f0a2b5373bf5085644a0fad9d7be59facf7e059017e9`.

## Reprodução e integração

O teste standalone aceita `fixture sha256 contagem`; sem argumentos executa as fronteiras. O fixture principal é `research/private/player-orientation-native-v1/arm-complete/oracle.bin`, SHA256 `89bdc81facfb3eded4e01cf546c8b2c373e235c7ad5e556881586e4ec988ede3`, contagem `69076`. O teste retido está em `retained-bridge/oracle.bin`, SHA256 `524941f1f23a67ee0b663637779f8d528aa90ce8e47b98e0fd0a749819606f95`, contagem `144`. O alvo precisa do módulo de orientação, `motion_transform_math` e include de teste `vendor/picosha2`.

Esta entrega fecha a orientação numérica no perfil declarado. A composição PBW/modelo, os gates de direção e callbacks adicionais, a ordem do scheduler e a descoberta de estado inicial permanecem trabalhos separados. As fontes e resultados anteriores da ponte permanecem intactos.
