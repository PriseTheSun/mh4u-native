# Interpretador nativo de política de ações

`player_policy_interpreter` percorre a tabela original em ordem, aplica os
predicados e janelas de tempo, mantém as entradas retidas e executa a transição
nativa quando a ação é aceita. Não transforma a tabela em uma busca pelo primeiro
predicado verdadeiro: os contextos, efeitos intermediários, retorno antecipado e
o bloqueio tardio `pw+148` continuam fazendo parte da decisão.

A fonte é `b0d0e4` na ROM EU fornecida, ELF SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
O perfil foi exercitado com todos os 35 registros da tabela `f2efb8`, SHA-256
`688f58d2943eea064fbbd3b1063a9a49183128b30080523034f614c6f912a83a`.
O oracle executa o corpo original inteiro e os helpers reais até o retorno.
Unicorn é uma ferramenta privada de comparação; não é ligado ao executável nativo.

## Estado e ordem dos efeitos

O bloco `pw+100..148` agora pertence a `player_policy_table::State`, dentro de
`player_transition::PwFields::policy`. Foram removidas as representações raw
paralelas. A transição usa o instalador nativo com tabela ausente e atraso zero;
a referência à tabela é um valor imutável com identidade, sem endereço ARM.
`Unknown`, `Absent` e tabela conhecida continuam distintos.

O intérprete recebe a projeção temporária já usada pelo controlador, que inclui
o peso da segunda camada a partir de seu dono. A primeira camada e os campos de
comando de animação são emprestados para a consulta temporal. Os campos extras
de contexto contêm somente offsets ainda não pertencentes aos outros módulos.
A palavra inicial de flags vem de `transition.pbw.field3d4`; os 19 campos seguintes
começam em `3d8`. O tipo, as duas entradas, a orientação, `6f9` e `9a8` também usam
seus donos existentes.

A operação trabalha sobre um candidato e publica somente um resultado completo.
Erros não modificam o estado fornecido. Valores desconhecidos são exigidos apenas
quando o caminho original os consome. Uma tabela ausente, o bloqueio inicial e a
negação de controle local retornam antes de consultar os outros contextos.

A migração das fixtures anteriores preservou os cinco headers originais e gerou
3.250 projeções em uma nova pasta, com hashes e contagem das referências raw.
Endereços sem ligação nativa viram `Unknown`; seus bytes permanecem nas evidências
originais e no adapter de comparação. O serializer conserva esses bytes opacos.
Os 92 testes anteriores passaram após essa mudança (43,27 s de parede no Mac).
O primeiro build identificou um consumidor de fixture sem o novo include; o
include do teste de recursos foi corrigido, sem alterar as evidências históricas.

## Perfil de comportamento

Estão compostos os donos de [predicados](player-policy-predicate.md),
[tempo](player-policy-timing.md), [tabela](player-policy-table-runtime.md) e
[transição](player-transition-runtime.md). O percurso conserva a ordem de
avaliação, a limpeza e retenção dos latches, as comparações ordenadas dos dois
temporizadores e as comparações inteiras dos bits das janelas do original.

Os contextos implementados incluem os testes numéricos das primeiras linhas,
a gravação `pbw+6fc` antes da rejeição da linha31, o helper que grava `pw+158`
mesmo quando retorna falso, as ações já correntes e os três contextos de altura
quando existe valor retido. A altura usa as duas somas na ordem VFP original;
reassociá-las muda o resultado em coordenadas grandes. A aquisição espacial da
altura permanece uma entrada explicitamente não suportada.

As linhas32/33/34 podem produzir `(estado,ação,flags)` `(1,2,1)`, `(1,1,1)` e
`(1,9,0)`. O sufixo original pode escolher `(1,4,0)` segundo a direção relativa.
São IDs observados, sem atribuir nomes de movimentos ainda não comprovados.
A transição limpa a tabela; o caller não deve reinstalá-la a cada frame para
simular um scheduler que ainda não foi reconstruído.

## Limites

Este perfil não implementa todos os contextos e dispatches especiais de
`b0d0e4`. Um caminho não implementado produz erro explícito quando alcançado;
não é tratado como predicado falso. Continuam pendentes os contextos com chamadas
externas, a consulta espacial sem valor retido, o wrapper de seleção `(0,0)`,
vários sufixos de orientação, o gate anterior `b17f78`, o scheduler e o bootstrap.
O perfil de transição existente também conserva suas limitações.

A chamada `b1c050` muda estado e flags; ela não é um pedido automático de animação.
Não há conversão inventada para `request()`, mapeamento físico de joystick,
caminhada original, combate, missões ou cooperação jogável nesta biblioteca.

## Comparação executada

O corpus autoritativo `arm-source-order` contém **341 execuções completas**,
2.339.950 instruções e 17 controles de guarda/perfil. A fixture tem SHA-256
`48b82ec4e7d9bc2a4a2e0450833349bbe772ec217eec18f33efcee66c3081798`.
Veja [o contrato do oracle](player-policy-interpreter-oracle.md) para proveniência,
limites dos contextos sintéticos e retenção dos bytes consultados pelo REA.

O consumidor C++ compara retorno, ordem de visita, presença/argumentos da
transição e os snapshots completos antes/depois de sete arenas. Passaram
**119.642 verificações** por execução normal e ASan/UBSan estrita. O relatório
`research/private/player-policy-interpreter-v1/native-integrated/validation.json`
conserva fontes, dependências reais do compilador, comandos, hashes, stdout e
stderr. Sete processos negativos verificam identidade, ausência, truncamento,
magic, contagem, tamanho de arena e tamanho de nome. As execuções bem-sucedidas
exigem stderr vazio e resultados estruturados idênticos.

A revisão encontrou dois problemas antes da integração: ordem das somas de
altura e perda do modo de referência quando o bit4000 estava desligado. O primeiro
ganhou um caso ARM completo que muda a precedência entre as linhas24 e25. O
segundo foi corrigido no dono `player_reference::State`: modo e heading agora
são opcionais, consultados somente quando necessários. Seis execuções ARM dos
predicados34–36 e regressões nos adapters verificam que `pw+150==1` seleciona
`pw+154` independentemente daquela flag. O intérprete oferece uma janela explícita
de20 palavras de flags; leituras além dessa janela são erros, sem fabricar zeros.

Essas verificações são de semântica numérica e composição nativa. Não são uma
captura de partida, teste de controle físico ou validação de rede pública.

## Integração ao Player

O [consumidor persistente](player-policy-actor.md) agora oferece
`Player::apply_policy`, com patches explícitos de entrada e contexto emprestado.
O controlador e a segunda layer fornecem seus próprios campos à projeção
temporária. O resultado volta pelo mesmo dono, `store_selection`, usado pelo
controlador anterior. O Player publica a decisão somente depois de gerar o Frame
com sucesso, preservando pose, cursors, bindings e contador de steps.

Essa operação continua separada de `request()` e `step()`. A factory de preview
com política exige uma receita explícita de estado e campos extras; reset a
reaplica, chamadas comuns não. O launcher mantém o perfil de inspeção anterior
até que o consumidor posterior das decisões tenha sido reconstruído.
