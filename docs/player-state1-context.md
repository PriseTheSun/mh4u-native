# Estado1/ID2: consulta de objetos e fronteira sem cache

O manager não vazio deixou de ser uma fronteira absoluta: **112 chamadas completas do dispatcher original `b66a20` retornaram com objetos presentes, e76 delas executaram a transição `act_st1/act_no13`**. O resultado vem de instruções originais, incluindo disponibilidade, direção e `b1c050`; nenhum helper ou PC foi substituído. O perfil usa estruturas de entrada sintéticas explicitamente inicializadas, LMTs reais e preparação original9577ec. Não demonstra a produção/lifetime desses objetos no jogo.

Na segunda pergunta, o ensaio avançou a rota sem cache por24 prefixos originais até a entrada da consulta `1774e8`. **Não executou a busca completa nem produziu um resultado de colisão**. A família de containers/árvores consumida posteriormente por `188850` continua sendo o próximo limite; ausência de dados não foi convertida em miss.

Os históricos permanecem em [player-state1-continuation.md](player-state1-continuation.md) e [player-spatial-context-v2.md](player-spatial-context-v2.md). Novos artefatos: `research/private/player-state1-context-v1/`. ELF fonte SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Nenhum runtime, CMake, launcher, ROM ou ELF foi alterado.

## Manager com objetos: cadeia e contrato

O caller ID2 preserva sua ordem original: custo em354, controle local, gates de input/PW, orientação, posição, `b3dfa8`, comparação150 e finalmente `b34268`. O perfil desta bateria declara cache presente e altura100; o custo e o deslocamento executam de verdade, com delta0 explícito para isolar as novas decisões direcionais. Não é uma simulação de tempo normal.

`b34268` retornafalse quando `pw.byte9a8!=0`; caso contrário encaminha o Player a `6823a8`. Este percorre os32slots em `(*(f32004))+0x18+4*i`, em ordem crescente. Slot nulo é ignorado. Um slot presente chama `911e48`; somente resultado verdadeiro permite `91a2a8(object,player)`. O primeiro resultado verdadeiro encerra a busca. Não há seleção do primeiro objeto não nulo por conveniência.

`911e48` tem20bytes e apenas retorna se `(*(object+e28)).byte140 != 0`. Chamamos esse subobjeto de **D**, sem atribuir classe/nome de gameplay não demonstrado.

`91a2a8` verifica, em ordem:

1. Player não nulo; D.byte140 não zero.
2. `D.word1b4 & 1` e `D.byte34c == 1`.
3. `D.byte290 & (1 << pbw.byte33)`, usando o shift ARM pelo byte do índice. Não restringe o índice a0..7 no código; valores fora desse domínio não podem ativar um bit do byte de máscara. A disponibilidade seguinte só é lida se esse teste passar.
4. `91f214(object,player,0)` deve retornartrue.
5. Escolha por `D.byte1c0`, seguida de direção.

`91f214` chama `682c4c` com o índice `pbw.byte33`; exige retornozero, `pbw.byte371!=8` para o argumento0 deste caller e `D.byte14c==pbw.byte9`. São bytes de área/índice consumidos pelo código, sem bootstrap inferido. No perfil executado, `global.byte5d=0`: `682c4c` executa `2b5998` real e retornazero quando `signed8(global.byte2f)==pbw.byte33`. Se5d não zero, segue69a870/2b42cc: essa dependência permanece fora do ensaio, com falha guardada.

Depois da disponibilidade, `D.byte1c0` seleciona:

| Tipo bruto | Consumo observado |
| --- | --- |
| `0x53` (83) ou `0x6f` (111) | Retornafalse sem consulta angular. Ambos foram executados. |
| `0x18` (24) ou `0x6e` (110) | Chama `930294(object,4,outPosition)`. O guard retém esses dois limites; não foi fornecida uma posição substituta. |
| `0x2e` (46), com D.byte14c==1 | Usa D.float198/19c/1a0 e uma classificação especial do heading abaixo. |
| Demais valores, incluindo46 quando byte14c!=1 | Usa D.float198/19c/1a0 diretamente para a direção ordinária. |

A direção usa `568b54(D.x−pbw.x,D.z−pbw.z)`, com a tabela original já recuperada. No ramo ordinário: `d=low16(heading−pbw.word24)`; aceita quando `0x2000 <= d <= 0xe000`. Isso vem de subtração u32 e comparação unsigned, não de uma redução modular após subtrair o limite inferior.

No ramo especial46/área1, interpreta o heading como signed16: negativo escolhe0xc000; não negativo escolhe0x4000. Calcula `d=low16(target−pbw.word24)` e aceita quando `14564 <= d <= 50973` (`0x38e4..0xc71d`). Não foi atribuído um nome físico ao cone ou ao objeto. A posição Y é carregada no vetor, mas não participa da diferença angular XZ desse helper.

Quando o manager retornatrue, ID2 **repete** `b24204(player,0)`; se o bit ainda está ligado, chama `b235f8(player,1,13,0)` e, após o retorno de `b1c050`, faz `pw.byte17c |= 1`. Esse write posterior foi observado e auditado. Os resultados preservam também os efeitos globais da transição, inclusive global+22; não se afirma que somente PBW muda.

Evidência REA nova: `009`/`012` para911e48; `010`/`013`/`023` para91a2a8; `014`/`021` para91f214; `015`/`022` para682c4c. IDs principais: `ev_db1dc4ab4eaad9f4c16b8d9dc179cf1ac6d456e0bf08edce43d94345a6cb5fe6` (91a2a8), `ev_9d1d3189d68802394f79e1631863d18075b4df37623c70b87502341a608cc5be` (91f214). Os readbacks foram comparados ao ELF.

## Perfil executado, identidades e limites

`manager-final/validation.json`:112 retornos,176.978instruções,55.420leituras,22.646stores,105calls91a2a8 e76transições1/13. Os casos incluem slots0/15/31, objeto desabilitado antes de um candidato válido, diversos gates/máscaras/índices, ambos resultados angulares, fronteiras inclusivas e o ramo46. Pontos angulares foram escolhidos nos eixos; a tabela original continua sendo executada. Contexto de cache, bytes de controle e todos os dados lidos estão declarados nos casos e intervalos inicializados.

Os recursos reais são os mesmos owners co0/lco0/co2 da entrega anterior; suas identidades/cópias relocadas constam do relatório. O resultado binário `manager-final/state.bin`, formato **MH4US2M1**, tem SHA-256 `10da455caea5491e0953b2e8cc11836028470344d1ddfa22479126c94c461786`. Mantém as sete arenas before/after, inclusive8slots de camada/2ativos. Não é o formato MH4US1C1 anterior nem uma fixture produzida pelo runtime C++.

Há seis controles negativos: byte de objeto desconhecido, posição desconhecida, helper de disponibilidade removido, tipos24 e110, e contexto5d não zero. Eles param nos guards reais. Os dois últimos não provam resultados false; indicam código/contexto ainda não coberto. Não foi executado o produtor do manager nem o helper930294.

`audit_results.py` lê a fixture e os traces sem reexecutar ARM. Verifica2.864relações, incluindo hashes/arenas, gates, estado1/13, ordem da transição e write17c posterior. A referência angular independente usa `atan2` da biblioteca host **apenas no auditor**, em pontos nos eixos onde os headings exatos são conhecidos; nenhum resultado do helper original é substituído por ela. Essa verificação não generaliza o erro de aproximação da tabela a vetores arbitrários. Os testes do helper angular permanecem autoridade separada.

## Cache ausente: preparação comprovada, busca ainda aberta

`b40b88(player,0,−60)` com `pw.byte6a8==0` prepara `(0,300,60)`, gira o vetor pelo low16 de `pbw.word24` com `aacf0c` real e soma a `pbw.float10/14/18`. Prepara também um segundo ponto com `pbw.float6e8/6ec/6f0`. Então chama:

```
2ba230(manager,player,currentOffset,previousOffset,
        pbw.byte9,&resultByte,0,0,0)
  → b4620c(player)                 máscara derivada de flags/estado
  → b43194(player)                 máscara de área/controle
  → 2bb178(...)
      → 198418                    filtro original
      → 171860                    carrier original
      → 198154                    filtros finais
      → 1774e8                    limite guardado de execução
```

`b4620c` começa com2, acrescenta0x800 quando `pbw.word478&8`, consulta `d05694`, PW5a8/f0 e `d051cc`, e considera byte370. Esses helpers foram executados, não resumidos por valores inventados. `b43194` usa `(*(fcf45c)+18)` e bit selecionado por `pbw.byte9`, byte370 e `d04924`. Os dois masks são argumentos distintos de2bb178; não devem ser fundidos ou associados a piso/parede sem prova.

`2bb178` produz a forma `(x, y + max(previousY−y, h), z, −1)`, onde `h=0` quando a máscara `0x2` do argumento de flags está ligada; caso contrário, 50 quando a máscara `0x8` está ligada, senão 100. A ordem float32 e a origem dos dois pontos estão no assembly; a fórmula não substitui a execução. Chama198154 com tipo1, máscara derivada e `1 << (area & 255)`. Os24prefixos variam heading, previousY e máscara `0x8` de PBW478. Sob o contexto particular deles, h100, Y inicial400 e a forma final tem Y=`max(500,previousY+300)`; os masks finais são0x2 ou0x802, e máscara de área8. Nenhum desses valores é default de bootstrap.

Os prefixos param na entrada real1774e8 por acesso a código deliberadamente não admitido no ensaio. O relatório `spatial-prefix-final/validation.json` registra **zero retornos completos**,30.240instruções executadas,8.784leituras e5.664stores de preparação. Cache6a4/6a8 não mudou antes do limite. Os inputs adicionais incluíram explicitamente PBW6e8..6f0, PW5a8, o manager de área e o ponteiro do manager espacial. Não foi dada validade implícita a RAM poison.

A derivação estática nova fecha o próximo encaminhamento:

```
1774e8 → 1924b4
       → 180c5c(manager,form,carrier,context,188850,0)
                    → 17e4c8 → 176210         preparação adicional
                    → chamada real188850      busca por containers
```

O par fonte em `e36a70+4/+8` é `(188850,0)`, retido por readback034. `1924b4` associa manager e filtro e instala callbacks17fec8/1772bc/179080; o caminho não usa o callback2bb838 da antiga enumeração experimental de triângulos. Portanto não é correto reaproveitar aquela enumeração e chamá-la de busca original deste caller.

`180c5c` contém a chamada por esse par e consome o resultado/atributos para classificação posterior. `188850` seleciona famílias de containers do manager e a família espacial já descrita em [player-spatial-context-v2.md](player-spatial-context-v2.md), em vez de percorrer uma lista simples de todas as faces. Produção/readiness dessas estruturas, callbacks de busca e resultados múltiplos continuam necessários para um retorno integral sem cache. Não há oracle de miss, hit ou valor final de altura nesta rota.

O retorno de2bb178 não deve ser reduzido a bool por inferência: o caller escreve seu byte de classificação; `b40b88` faz tratamento especial quando esse byte é **exatamente1** (armazena a altura emPW6a4 se cache ainda era0 e incrementa byte6a8). Byte0 retornafalse; outros valores seguem a comparação com o limiar. Isso é uma derivação do corpo retido, não um cache atualizado observado nesta bateria.

Evidência da rota: `011`/`024`(2ba230), `018`/`025`(2bb178), `041`/`026`(1774e8), `020`/`036`(1924b4), `019`/`035`(180c5c), `034`(par188850), `027..033`(helpers e construtores) e `037..039`(dados fonte). O corpo180c5c possui dois intervalos de código separados por literais; o readback035 cobre o span completo. `source-facts.json` retém cada Evidence ID e coverage.

## Falhas preservadas e ciclo de vida

`manager-first` era um teste de1caso; o primeiro controle negativo ainda herdava objeto desabilitado e não alcançou a posição, então o auditor recusou aceitá-lo. `manager-second` revelou alias do setup anterior: dois singletons vazios compartilhavam o mesmo endereço, e preencher slot1 da lista sobrescrevia manager+1c da referência angular. A preparação nova usa dois objetos de manager distintos e explícitos; não muda código alvo. `manager-third` passou; `manager-final` acrescenta tipo83 e guard110. O primeiro prefixo já atingiu1774e8, mas a asserção experimental esperava code_bounds em vez do memory_bounds real; ambos são limite, nunca sucesso do helper. As falhas e correções estão em `failure-notes.json`.

O primeiro bridge sem PTY terminou sem abrir alvo. Na sessão ativa, open com snapshot inexistente foi rejeitado; a repetição removeu esse argumento após ler o erro. Não houve importação duplicada. Sessão focused3GiB **1792308162875**, resolução fria146,599s; bundle044, close045 com snapshot, bridge da execução 75364 terminou com código 0. O bundle e o snapshot retêm 38 Evidence, incluindo 20 novos readbacks completos. A inspeção de processos e as identidades estão em `lifecycle.json`. Nenhum launcher foi alterado.

`source-facts.json` selecionou111readbacks reutilizados/novos e cobre todos os bytes fonte efetivamente executados/lidos dos dois ensaios. Os30.352bytes mapeados incluem corpos herdados não exercitados; cobertura mapeada e execução não são equivalentes. Custo de execução CPU/parede/RSS do Python consta dos dois relatórios; tokens e pico da família REA não foram medidos.

Reprodução, sempre em destino novo:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-state1-context-v1/verify_manager.py --output research/private/player-state1-context-v1/recheck-manager
research/private/root-motion-runtime/venv/bin/python -B research/private/player-state1-context-v1/verify_spatial_prefix.py --output research/private/player-state1-context-v1/recheck-prefix
python3 research/private/player-state1-context-v1/audit_results.py
python3 research/private/player-state1-context-v1/collect_facts.py
```

Os dois últimos auditam as saídas finais fixadas, sem reexecutar o alvo. `dependencies.json` também registra os módulos carregados dinamicamente que não aparecem em `sys.modules`; as cópias exatas ficam em `inputs/` e o manifesto fixa seus hashes. A nova prova habilita futura implementação nativa do **perfil de manager local declarado**, preservando todos os outros gates. Não autoriza generalizar manager desconhecido/vazio, cache ausente, tipos24/110, rede, origem de coordenadas, tempo ou semântica dos estados de destino.
