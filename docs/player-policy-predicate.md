# Predicados da tabela de comandos do jogador

`player_policy_predicate` reproduz o dispatcher original `b24354` e seus
helpers numéricos, incluindo a virtual concreta `uPlayer+98`. São os testes
de entrada que a tabela de ações consulta. O módulo ainda não executa a ordem,
os tempos, os latches ou os gates de contexto do interpretador `b0d0e4`, e
não escolhe uma ação pelo primeiro predicado verdadeiro.

## Contrato e donos

Um registro original tem 22 bytes. `decode` lê apenas `u16+6` e os três
argumentos `u16+8/+a/+c`, sem atribuir significado aos outros campos. O
dispatcher usa `word & 0x3fff`, respeita os casos constantes 0/31 e devolve
falso para IDs acima de 38 antes de consultar o jogador.

O contexto é emprestado e preserva campos ausentes como desconhecidos. Os
words iniciados em `pbw+3d4` têm um único dono: a consulta com base `3e4` usa
o deslocamento de quatro words na mesma view. Extensão insuficiente e word
desconhecido são erros distintos. Campos ignorados por curto-circuito não são
exigidos. Não há interpretação de IDs de input como nomes de botões físicos.

O dono `player_input` agora atende o gate `b417a4`: tipos `{4,5,6,15}` podem
suprimir a magnitude somente quando `pbw+6f9==1` e `pw+50d==1`. A comparação
é com o byte 1, não com qualquer valor verdadeiro. A supressão ocorre antes
da leitura do canal. Fora desse caminho, a classificação original 0/1/3
continua tendo limites 50 e 110.

O mesmo dono recebe arcos genéricos. O modo 1 consulta a virtual de magnitude
e ignora referência; outros modos podem ajustar a orientação antes de testar
magnitude. O intervalo angular é inclusivo e modular em 16 bits. Os callers
anteriores de `in_sector` conservam o contrato e os erros de contexto.

## Rotas do dispatcher

| IDs | Consumo original |
| --- | --- |
| 0 / 31 / >=39 | verdadeiro / falso / falso, sem campos do jogador |
| 1, 2, 19 | máscara `3bc`, magnitude virtual e arco; 19 acrescenta máscara `3b8` |
| 3, 5, 18 | máscaras `3ac`, `3bc`, `3a8` |
| 4, 6, 7 | magnitude virtual e máscara, com curto-circuito original |
| 8, 9, 12, 13, 20, 21 | magnitude virtual dos canais 0/1, zero/não zero/limiar 3 |
| 10, 11 | duas máscaras, ou arco virtual seguido de máscara |
| 14–17 | arcos virtuais dos dois canais, positivos ou negados |
| 22–30 | bits em `3d4`/`3e4`, magnitude e arcos, na ordem original |
| 32 | arco relativo a `pbw+24` e à referência resolvida |
| 33 | dispatcher de setores, com modo explícito |
| 34–36 | combinação de bit/setor e correção direcional por `b2a6c0` |
| 37, 38 | bit `3e4` seguido de arco comum, ou magnitude virtual zero |

`b2a6c0` seleciona orientação de `pw+154` quando `pw+150==1`, ou `pw+b0`
caso contrário; resolve a referência, reduz a diferença a u16 e consulta
dois limites de `100c250`. Dentro de **[inferior, superior)** troca somente
IDs `0x400/0x800` antes de consultar o setor. Os limites são um contexto
runtime explícito; o conteúdo do ELF anterior ao startup não vira default.
Este intervalo não é o arco modular inclusivo dos demais helpers.

## Evidência e validação

O ELF permanece fixado em
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
REA fornece os readbacks e a identificação da virtual; os relatórios privados
ficam em `research/private/player-policy-predicate-v1/`. A execução usa
Unicorn 2.1.4/ARM11MPCore, FPSCR 0, todos os helpers originais, sem substituir
calls, retornos ou PC. Apenas writes de stack são permitidos; leituras exigem
bytes inicializados no caso atual. Saltos para a tabela e orçamento insuficiente
são rejeitados.

O oracle completo contém **5.555 retornos originais**, 301.487 instruções,
os 39 IDs, os dois resultados de cada ID não constante, flags superiores,
gates de magnitude, limites angulares e referências. Também executa cada um
dos 35 registros reais de `f2efb8` em 12 contextos explícitos, separadamente.
Isso não ignora nem valida a ordem desses registros no interpretador completo.
Há dez controles negativos ARM e quatro verificações de contexto não consumido.

Fixture: `arm-complete/oracle.bin`, SHA-256
`f261dee0c966fb81366ff611ab06d2903fed6f21956d8ad0e20d211e0786387e`.
O adapter nativo lê os estados observados e compara o resultado; não usa
uma fórmula nativa para fabricar expectativas. Casos de campos ausentes,
extent, referência inválida e curto-circuito recebem verificações separadas.

Os testes medem predicados numéricos sobre estado explícito. Não demonstram
bootstrap, aquisição do contexto de uma missão, mapeamento SDL, interpretação
completa da política, combate ou sincronização de ações online.
