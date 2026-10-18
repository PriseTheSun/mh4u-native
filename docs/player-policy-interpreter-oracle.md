# Execução integral do interpretador da tabela de política

O corpus `research/private/player-policy-interpreter-v1/arm-source-order` executa o ARM original de `0xb0d0e4` até seu retorno, usando a tabela inteira em `0xf2efb8`: 35 registros de 22 bytes e o sentinela original. Há 341 contextos explícitos, 2.339.950 instruções e nenhuma chamada substituída. O teste não começa em um registro posterior e não escolhe o primeiro predicado verdadeiro como resultado.

A identidade do ELF é `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. O arquivo de 792 bytes da tabela tem SHA256 `688f58d2943eea064fbbd3b1063a9a49183128b30080523034f614c6f912a83a`. A fixture final tem SHA256 `48b82ec4e7d9bc2a4a2e0450833349bbe772ec217eec18f33efcee66c3081798` e 109.004.717 bytes. O readback independente verificou 4.774 snapshots de arenas e 6.479 condições do formato.

## Evidência e execução

`source-observed/evidence-index.json` liga 17 readbacks REA, totalizando 17.476 bytes conferidos contra o ELF, aos respectivos Evidence IDs. A fonte principal está na sessão `1792300233949/054-read_bytes.json`; a tabela em `068-read_bytes.json`. Predicados, timing e transição usam os mesmos corpos originais e limites dos oráculos anteriores. A nova sessão `1792304841481` fornece o tail de `6b398c` e os helpers `b2a7ec`/`d0506c`.

A Listing do Ghidra para `6b398c` omite braços não contíguos. A leitura anterior terminava em `6b3a48`, embora a tabela de saltos apontasse a `6b3a44` e `6b3a64`. A nova leitura `007-read_bytes.json` fecha esses braços e o retorno em `6b3a84`; a desmontagem LLVM retida documenta os saltos. Não foi inferido um retorno para preencher a lacuna.

O harness usa Unicorn 2.1.4, ARM11MPCore e FPSCR inicial zero. Todos os opcodes vêm do ELF fixado. Os hooks apenas observam execução, leituras e stores, ou interrompem uma violação de guarda. Não reescrevem PC, argumentos ou resultados durante a execução. Os contextos e os registradores são inicializados antes da entrada. Aritmética esperada e decisões não são calculadas por uma réplica Python.

O corpo completo fica carregado; as permissões de execução excluem pools literais e tabelas de salto. Os helpers permitidos têm limites explícitos. Leituras exigem bytes-fonte retidos ou campos inicializados nesta execução; mapear uma arena não a torna toda legível. Arenas começam com padrões não zero, e somente offsets declarados ficam disponíveis. Stores são limitados à stack e aos campos demonstrados. São comparados todos os bytes das arenas, incluindo os não alterados. O limite por chamada é 30.000 instruções, dois segundos e stack de 2.048 bytes consumíveis.

## Resultados observados

No contexto neutro sem input, a visita é exatamente `0..34`, seguida pelo sentinela e retorno zero. Os predicados constantes das linhas 0, 1, 2, 3, 4, 6, 7 e 31 passam, mas seus contextos reais rejeitam a execução. A linha 31 escreve `PBW+6fc=1` antes da rejeição. A linha 4 pode manter seu latch mesmo quando o helper retorna zero: não existe uma regra geral de limpar todo registro rejeitado.

Com os contextos anteriores rejeitados e direção zero:

| Estado de input explícito | Registro alcançado | Chamada original `b1c050(state,action,flags)` |
|---|---:|---|
| magnitude abaixo de 50 | nenhum | nenhuma |
| magnitude 50..109, bit0 ausente | 34 | `(1,9,0)` |
| magnitude pelo menos 110, bit0 ausente | 33 | `(1,1,1)` |
| magnitude pelo menos 50, bit0 presente | 32 | `(1,2,1)` |

Esses valores são campos-fonte, sem correspondência atribuída a teclas ou dispositivos. A direção participa do sufixo real: determinados valores, incluindo 49152 no corpus, convertem as ações 1/2 para ação 4 com flags zero. A seleção de um registro não basta para prever a chamada final.

O corpus também demonstra:

- precedência das linhas 1/2/3 quando os campos `PW+188`, `+224` ou `+238` passam seus testes, além das linhas 6/7;
- `PW+148` bloqueando a chamada final, sem desfazer stores anteriores do interpretador;
- timers `+140/+144` com zeros de ambos os sinais, subnormais, infinidades e NaNs: a comparação ordenada permite NaN seguir o fluxo; isso não é uma afirmação de que esses valores ocorrem no jogo;
- latches 1 e 255 preservados por `PW+106>=1` disparando a transição sem input novo, enquanto `+106==0` os limpa antes do predicado;
- retorno 1 sem nova chamada de transição para certas combinações de estado/ação já correntes;
- `b242e8` escrevendo `PW+158` antes de retornar zero;
- timing original por `b09da8→b064fc`, incluindo limites substituídos pelos campos 104/106/108 e diferentes estados da primeira layer.

## Fronteira do contexto espacial

As direções no arco das linhas 24..26 fazem o predicado real 32 passar. O oracle executa `b2a7ec` com `PW+6a8` não zero, portanto consome o resultado armazenado em `+6a4`, compara com `PBW+14` e os limites da linha. Foram exercitados valores abaixo, dentro e acima dos intervalos e o gate posterior `d0506c`, baseado em `PW+9a8`.

Um discriminante adicional fixa `y=0x4c800000` e `h=0x4c800017`. O ARM calcula o limite superior como `f32(y + f32(offset + width))`: rejeita a linha 24, aceita a linha 25 e chama `(0,5,12)`. A associação alternativa `f32(f32(y + offset) + width)` mudaria a primeira decisão.

A produção espacial desse valor não é substituída. O controle negativo com `+6a8==0` chega à chamada real `aacf0c` e é recusado pelo limite de código do perfil; não retorna um sucesso fabricado. Contextos adicionais, handlers de ações especiais e combinações de inputs não presentes no corpus continuam sem equivalência integral demonstrada. A tabela não foi alterada para evitar esses caminhos. O limite de 48 registros do interpretador não é atingível pela tabela real de 35; seu formato e cobertura são tratados separadamente pelo dono `player_policy_table`.

## Contextos externos e propriedade

As sete arenas são `player`, `pbw`, `pw`, `global`, `context`, `manager`, `object`, com tamanhos respectivos `b000`, `a000`, `1000`, `1000`, `e000`, `1000`, `1000`. O PBW ampliado cobre a leitura real `+93ee` e o store `+93f1`. Os ponteiros globais `fb6b7c`, `fbeffc` e `1057294` apontam para as arenas correspondentes. `manager+1c` é nulo no corpus. A tabela de direção em `100c250` recebe o snapshot explícito `[4000,c000]`; os bytes ELF iniciais desse global não são apresentados como estado pós-inicialização.

Os endereços pertencem apenas ao oracle. O runtime nativo deve mapear presença, ausência e desconhecido para seus tipos canônicos. O estado de política em `PW+100..148` pertence a `player_policy_table::State`; qualquer projeção para testes deve evitar uma segunda cópia mutável no jogador. A decisão de `b0d0e4` chama a transição `b1c050`, não diretamente um pedido de animação Common. Ligar essa decisão a uma animação exige o consumidor seguinte demonstrado.

## Arquivos e reprodução

`verify_arm.py` executa; `generate_cases.py` fornece somente os contextos; `collect_evidence.py` confere a fonte; `verify_fixture.py` relê o binário por streaming. `arm-source-order/cases.json` mantém chamadas/retornos, branches e stores fora da stack. `execution-traces.jsonl.gz` preserva a sequência completa de PCs, leituras e stores. Há 17 controles de guarda/perfil, incluindo target errado, dados ausentes, endereço de tabela inválido, código, stack, orçamento e campos legitimamente não consumidos.

O binário começa com `MH4UPI01`, `u32 arena_count`, `u32 case_count` e pares `u32 address,size`. Cada caso contém nome (`u32 comprimento` e UTF-8), retorno, contagem/índices visitados, contagem/triplas de argumentos de `b1c050`, depois `before` e `after` de cada arena na ordem do cabeçalho. Não há JSON no binário. Os metadados ricos ficam nos sidecars. Produtor e leitor impõem 512 casos e 256 MiB antes da expansão/alocação correspondente; nomes têm limite de 1.024 bytes no leitor.

Para repetir, use o Python de `research/private/root-motion-runtime/venv/bin/python` e um diretório novo como `--output`. As saídas `first`, `expanded-first`, `expanded-second` e `arm-final` foram preservadas. Nenhum teste do projeto inteiro, janela persistente ou pacote foi alterado por esta investigação. A comparação C++ e sanitários pertence à integração do root, não à execução ARM aqui descrita.
