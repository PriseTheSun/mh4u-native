# Continuação nativa do estado1/ID2

`player_state1::apply` agora executa também os subestados1/2 do ID2 no perfil
recuperado. Compõe custo, orientação, deslocamento, gates de contexto, consulta
de objetos, transição e possível pedido da animação4, na ordem do original.
O Player publica esses resultados junto com seu estado persistente, depois
da renderização. O relógio e o decremento do timer permanecem explícitos.

## Propriedade dos dados

`player_fields::Fields` passou a ser o único dono dos extras compartilhados,
incluindo a posição PBW10/14/18, custo354, flags478 e cache PW6a4/6a8. Os dados
que antes estavam declarados no intérprete de política foram movidos para esse
dono, sem manter uma segunda representação. `PlayerInitial::shared_fields` e
`Player::shared_fields()` fornecem esse estado; a política e o despacho
devolvem candidatos do mesmo tipo. Controlador, seleção, camada1 e timer50
continuam em seus donos existentes.

`ContinuationContext` recebe uma visão emprestada dos extras, delta bruto,
contexto ad50, referência angular e manager de objetos. As vistas não são
armazenadas no Player. Um campo desconhecido só falha quando seu ramo o consome.
O manager angular e o manager de objetos são identidades diferentes.

## Ordem e dependências

| Etapa | Dono nativo e resultado |
| --- | --- |
| Custo | `player_state1_dynamics::cost`: gates locais, lista de IDs, multiplicadores na ordem original, delta e clamps de354. A palavra358 continua no dono da seleção. |
| Posição | `player_state1_dynamics::advance`: tabela original e36e98, coeficientes e operações binary32 separados; grava as três palavras PBW. |
| Contexto espacial | Gate7f0, flags478,9a8 e cache explícito; cache presente compara altura com Y−60. A aquisição sem cache ainda retorna erro de contexto não implementado. |
| Objetos | `player_object_query::query`: percorre32slots na ordem fonte, disponibilidade, tipo e direção; distingue Unknown/Absent e encerra no primeiro candidato aceito. |
| Destino | Transições2/0,1/11,1/13,1/9 ou1/3 conforme os gates originais; em1/13 preserva o write17c posterior à transição. |
| Espera | Quando o helper não transita, subestado1 consulta o gate real da camada0 e pode reiniciar selector4; subestado2 omite esse pedido. |

A busca de IDs da lista PBW2fc agora pertence a
`player_selection::contains_id`, reutilizada por seleção de área e custo.
Erros mantêm sua causa tipada. As operações finitas exigem round-to-nearest e
underflow gradual; dados não consumidos preservam seus bits, inclusive NaN.
A posição PBW ainda não é tratada como posição do modelo renderizado: essa
ligação precisa ser recuperada no fluxo original de atualização.

A tabela numérica tem256 registros e4096 bytes, SHA256
`f2433a2419e313eb7d18fc5f2682c7be465840bf0f120094ab2b00c79dcca7a8`.
O gerador conferiu o readback REA com o ELF fixado. Não usa seno/cosseno da
biblioteca host para substituir a tabela. Fonte e geração ficam em
`research/private/player-state1-id2-native-v1/dynamics-owner/`.
A [documentação da dinâmica](player-state1-dynamics.md) detalha os campos
consumidos, a ordem aritmética e os limites desse perfil.

## Comparação com o original

O consumidor independente reúne140 chamadas ID2 do
[primeiro corpus](player-state1-continuation.md) e112 chamadas com
[manager presente](player-state1-context.md). A fixture MH4US2D2 preserva
before/after das sete arenas, índices originais, cobertura inicializada e
observações derivadas de instruções/stores. SHA256:
`84c5ccae6be34807edc4d7764d1e1cfb45967e0c89cafb49f57f22b43eb0ea06`.

São252 retornos completos,114 transições,265 consultas de magnitude,44 setters,
22 aceitações e22 wrappers. Também coincidem216 chamadas de custo,191 escritas
de354,178 atualizações de posição,137 chamadas de manager,1492 slots
examinados,105 candidatos,88 direções e76 resultados verdadeiros. O teste
compara40.255.488 bytes de saída e as identidades lógicas dos recursos.
Seus115.743 checks incluem18 erros nativos e um no-op com contexto desconhecido;
esses controles não são novas execuções ARM. Os252 casos mantêm a troca de
animação pendente e não exercitam consulta viva de pose.

O helper de teste compartilhado `test_support/player_continuation_arena.hpp`
centraliza formato, campos e projeção usados pelos consumidores de235 e252
casos. Expected continua vindo exclusivamente dos bytes/calls originais.
As384 entradas e235 continuações anteriores permanecem como regressões
separadas; somente a antiga asserção de que ID2 era sempre unsupported foi
removida, pois deixou de representar o contrato.

O teste do personagem passou227.467 checks. Exercita os três destinos do ramo
ativo, custo/posição canônicos, manager com objeto desabilitado antes do válido,
gate de animação obtido por passos reais e ligação da animação4 no passo
seguinte. Erros tardios de manager/consulta espacial preservam todos os donos
vivos, e uma repetição válida aplica o custo uma única vez. Isso é prova de
composição nativa com recursos reais, separada da execução ARM.

## Validação integrada

O comparador ID2 e as regressões de235/384 casos passaram tanto no perfil
normal quanto com ASan/UBSan estritos. Os controles de processo passaram:
18 por perfil no ID2 e21 na entrada. A dinâmica tem91 checks de fronteira;
seleção, seleção de área e intérprete de política mantiveram suas regressões.
O personagem passou os227.467 checks nos dois perfis, mais12 controles
negativos por perfil e as regressões de política, ator anterior e driver.

O build oficial macOS passou100/100 testes CTest em48,17s. Os comandos
completos de build/teste macOS e compilação Windows levaram75,074s e20,738s,
respectivamente, sem warnings. O executável Windows foi compilado, mas não
executado. CPU, pico de memória da família de processos e tokens não foram
medidos nesta validação; não há comparação de performance antes/depois.

Relatórios, snapshots de fontes e hashes ficam em
`research/private/player-state1-id2-native-v1/`. Evidências anteriores
permanecem preservadas com seus próprios snapshots e resultados históricos.

## Trabalho restante

O perfil de objetos ainda não admite a disponibilidade de rede nem a posição
especial dos tipos24/110. A pesquisa espacial continua em
`research/private/player-spatial-context-v3/`: a [nova prova original](player-spatial-context-v3.md)
completa15 consultas em containers construídos vazios,18 em listas com nós
rejeitados e48 chamadas do wrapper. Quatro outros ensaios param no callback
`18dc48`; não são resultados de colisão. Esses estados preparados não provam
a população pelo cenário, e a rota nativa sem cache continua não implementada.
Origem/lifetime dos objetos, validade do
cache, bootstrap, scheduler, ligação ao espaço renderizado e handlers de
destino continuam necessários para gameplay. Não há nova interface ou pacote
publicado nesta etapa; o launcher0.17 continua aberto para inspeção.
