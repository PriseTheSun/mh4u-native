# Contexto antes do primeiro pedido do caçador

A criação observada passa por `af8d68 → b3b454 → b085e4`. `b3b454` já grava `pl_st=0` e pede o selector `1` **antes do registro do objeto**; posteriormente outro caminho ajusta posição/contexto e atualiza `pw+7fc`. Isso não estabelece que o pedido pré-registro foi aceito, nem autoriza inicializar o driver em selector `8` ou `203`.

A nova prova executável cobre o construtor completo do gerenciador de contexto, três inicializadores de contexto com tabelas reais e o produtor completo de `pw+7fc`, com seus helpers originais. Não cobre a construção integral do caçador, alocadores, loader, scheduler ou consulta espacial. O driver ainda precisa receber os fatos contextuais ausentes explicitamente.

O alvo permanece o ELF privado SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Artefatos, bytes, relatórios e scripts estão em `research/private/player-gameplay-context-v1/`. Não houve alteração de runtime, ROM, parser, CMake, pacote ou launcher.

## Ordem recuperada estaticamente

`6a9c48` cria os jogadores por `af8d68` num loop limitado pelo byte com sinal `global(fb6b7c)+2d`. Depois chama `6c196c` para obter posição/ângulo e `af9440`, que chama `b26f50` e `b6b37c`. Essa ordem é factual dentro da função examinada; não é uma execução completa dela.

`af8d68` consulta configuração do equipamento e escolhe um construtor derivado. No caminho padrão, `c9d050 → b6d588` instala a vtable `e6ed8c`, grava `pbw.byte31=0` e aponta duas tabelas em `pw+164/+168`. Os slots observados nessa vtable são `+18=b5bec8`, `+24=b48230`, `+70=9575dc` e `+118=b431e8`. Isso vincula um caminho de construção concreto ao loader, update e setter já recuperados. Os demais construtores do switch não foram executados ou presumidos equivalentes.

Na criação nova, o trecho comum de `af8d68` chama:

1. `af9318 → b3b454`;
2. `af9324 → b085e4`, registro já estudado pelo dono dos bancos;
3. `af9338 → 69a188`.

O caminho que reutiliza um jogador já existente salta para o último trecho; não refaz toda a inicialização. `b3b454` escreve `pbw.byte9` a partir do low8 de `global+ c90`, `pl_st=0` e chama `b0c060(player,1)` no prefixo. Mais tarde zera `pw+7f1/+7f2/+7f4/+7f5/+7fc/+7fd`, entre outros campos. O reset de `pw+7fc` é em `b3b9f8`, posterior ao pedido.

`b3b454` também pode mudar `pbw+354/+358` para os bits `43160000`, sob `d047b4(player,0x40)` e decisões de `2b555c`. Portanto os `44160000` produzidos pelo reset base são uma etapa anterior, não um valor final universal. A função completa e seus literais foram retidos; seus helpers de itens, RNG e configuração não foram substituídos por retornos fictícios, nem a função foi contada como executada neste ensaio.

`b085e4` e o despacho posterior de virtual `+18` reutilizam [a investigação de bancos](player-motion-banks.md). A existência desses elos não prova que o carregamento foi concluído antes de cada pedido anterior. O primeiro update condicionado por readiness continua sendo o ramo `b48230 → b312dc` do [recorte anterior](player-gameplay-bootstrap.md). Esse ramo pede estado0, flags2 e modo1; seus campos contextuais não são inferidos apenas pela ordem de construção.

## O que `bd9290` realmente faz

`bd9290` é um wrapper de oito bytes: coloca `r3=0` e salta para `bd9804`. Este recebe o gerenciador, jogador e índice `pbw.byte33`. Escreve primeiro `player+fc=1`; para índice fora de `0..7`, retorna0. Para índice válido seleciona um registro em `manager+14+16*index`, remove a entrada anterior se ativa e tenta construir dois objetos:

- alocação `362858(0xf0,0x10)` seguida de `362744`;
- alocação `3a22e8(0x130,0x10)` seguida de `3a21dc`.

Cada objeto é registrado por `645ad4` no grupo `0x10`, recebendo a máscara `0x100 << index` em `+34`. Com sucesso, a entrada recebe active1, byte2=0, ponteiro do jogador e os dois objetos; `player+fc` recebe essa máscara. A falha limpa a entrada e pode chamar virtual `+40` dos objetos criados. Não há escrita direta dos campos de seleção no corpo, mas os efeitos transitivos dos construtores/registro/cleanup não foram excluídos.

Não foi executado um caminho válido completo de `bd9804`: isso exigiria o estado real dos alocadores e registradores. Um allocator stub ou um retorno artificial não seria prova do primeiro update. Essa fronteira continua explícita.

## Categoria: construtor, tabela e atualização contextual

O singleton em `fcf45c` é escrito pelo próprio construtor `6c7bcc`. Ele chama os originais `10724c → 10bd74 → 10bd3c`; a última folha usa `LDREX/STREX` e também foi executada. Não houve substituição de alocador: a memória de destino é uma arena explícita fornecida ao construtor, como argumento.

O construtor configura 21 registros em `manager+154+24*i`:

| Offset dentro do registro | Escrita observada |
| --- | --- |
| `+0`, `+4` | u32 zero |
| `+8` | categoria u8 zero |
| `+9..11` | preservados |
| `+12` | binary32 `461c4000` |
| `+16` | u8 zero |
| `+17` | preservado |
| `+18`, `+20`, `+22` | u16 zero |

O getter `6c33d4`, já retido, consulta essa categoria por `manager+15c+24*id` após remapeamento por `69d568`. O consumidor de seleção e seus ensaios de área pertencem ao dono de `player_selection`; não se atribui sentido físico novo à categoria aqui.

O setter completo `6c2440(manager,value,records,count)` escreve `manager+150=value`, `manager.byte710=low8(count)` e copia `min(count,21)` registros completos de24bytes, quando `count>0`. Os registros restantes são preservados. A categoria do construtor pode ser substituída por dados do contexto.

Foram identificados 17 callers diretos com ponteiros/tamanhos constantes. `category-tables.json` retém os argumentos e categorias derivadas do ELF, sem rotular automaticamente cada contexto como uma área. Três inicializadores completos foram executados, sempre depois do construtor e por escolha explícita do ensaio:

| Função | Tabela fonte | Contagem | Categorias carregadas |
| --- | --- | ---: | --- |
| `655694` | `ded78c` | 11 | onze zeros |
| `655e3c` | `dedaa4` | 10 | `0,3,3,3,4,3,3,3,3,4` |
| `6583a0` | `dedf3c` | 11 | `0,0,1,1,0,1,0,0,2,2,0` |

Eles também escrevem ponteiros auxiliares em outros campos do gerenciador, preservados nos snapshots completos. Não se leu conteúdo apontado por esses ponteiros fora do contrato executado. A seleção de qual vtable/contexto corresponde à missão ou área efetiva continua desconhecida; não se aplica a primeira tabela a todos os cenários.

## Produtor de `pw+7fc`

A função completa `b3aa84` usa `player+e30` como pbw e `player+a29c` como pw. O fluxo recuperado é:

- Se `(pbw370==2 && pw7f4==7)`, `pw9a8!=0` ou `pbw473==0`, zera `pw7f4`, `pw7f5` e `pw7fc`.
- Caso contrário, `6c4edc` e `6c1958` consultam `6c2194` e obtêm, respectivamente, os u32 do objeto `+18c` e `+190`. São estreitados em `pw7f4` e `pw.u16(7f2)`.
- `6c2194` percorre `manager+3f4` com count em `+454`, ignora ponteiros nulos e retorna o primeiro objeto cujo byte `+a8` corresponde a `pbw9` e byte `+4d` a `pbw473`. Pedido zero retorna nulo.
- `6c4a08` usa a mesma busca. Para objeto presente, a folha `1cc224` retorna1 se byte `+b1==6`, ou u32 `+18c` está em `4..6`, ou é10. Esse resultado decide `pw7fc`.
- Se a classe estreitada em `pw7f4` for zero, `6c5c18` pode encontrar um registro alternativo para `pbw475>=20` em outro recurso (`manager+3a0+4*pbw9`). Um registro encontrado define a classe de `u16+8`, liga `pw7fc=1` e, para classe9, aplica as conversões VFP originais ao valor recebido. Valores auxiliares sem uso pelo caller não foram inventados.
- `pw7f1` registra a mudança de classe sob as condições originais; `pw7f5` recebe a classe atual ao sair. Campos não escritos permanecem anteriores.

A execução usa os helpers reais, inclusive buscas e conversões. Os objetos e entradas de lista do teste são estados sintéticos declarados, não registros fingidos como extraídos de uma sessão de gameplay. Foram cobertos gate de estado, gate9a8, pedidozero, lista vazia, ponteiro nulo, IDs divergentes, classes0/1/4/5/6/7/9/10/255, tipos0/6, classes anteriores6/7 e caminhos alternativos ausente/presente.

`b6b37c` zera inicialmente `pw7fc`; seu ramo normal chama `b30fa0` e só depois `b3aa84`. `b30fa0` reutiliza um valor em cache quando `pw6a9==1` e a posição coincide exatamente; caso contrário chama `2b9f6c` com jogador, posição, área e ponteiros de saída contextuais. Essa consulta é a próxima fronteira para produzir os inputs de `b3aa84` a partir do cenário. O outro caller de `b3aa84`, `b6becc`, mostra atualização posterior, sem garantir que o valor seja imutável depois da criação.

## Ensaios, evidência e limites

`verify_context.py` usa Unicorn2.1.4/ARM11MPCore. `arm-final/validation.json` registra **12 sequências de construtor/contexto, 32 chamadas completas do produtor e 5 controles negativos**, com **21.434 instruções originais** nas execuções positivas. Nenhum hook fornece retorno, altera código ou substitui helper. Guards limitam fetch, leitura inicializada/literal, escrita e orçamento.

Os três padrões anteriores de RAM/registradores `00/a5/ff` produzem os mesmos bytes escritos pelo construtor/contextos; todos os bytes não escritos permanecem com o padrão anterior. O construtor escreve1696bytes da arena de2048bytes. Os registros de tabela copiados são comparados byte a byte com o ELF, e as categorias esperadas são verificadas separadamente. No produtor, os campos de classe e `pw7fc` são conferidos contra os predicados do contrato; todas as demais escritas e acessos permanecem no trace. Não se alega comparação nativa desse novo produtor, pois nenhum módulo runtime foi implementado aqui.

Os controles negativos retiram a folha atômica, o ponteiro global, count da lista, global de lookup e helper de busca. Todos interrompem pelo guard esperado. `arm-initial` e `arm-complete` são estágios anteriores preservados; `arm-final` é o resultado autoritativo.

Os readbacks novos são comparados integralmente ao ELF por `retain_facts.py`. A leitura140 de `b30fa0` era parcial;141 completa o corpo e os dois literais. O corpo Ghidra de `af8d68` é não contíguo e não representa sozinho cobertura de cada ramo do switch;115 retém todo o span original para análise posterior. Nenhum sucesso de pseudocódigo é convertido em alegação de execução desses caminhos.

Sessão REA1792301774362, Ghidra12.1.4, ARMv6, perfil focused e heap3GiB. Importação/primeira resolução:147,266s. O ensaio final levou aproximadamente0,160s, sem incluir importação. CPU, pico de memória da família e tokens não foram medidos. Metadados por chamada, bundle, snapshot fechado e pins ficam no manifesto privado. A sessão foi exportada e fechada; nenhum launcher foi manipulado.

Reprodução em um diretório novo:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-gameplay-context-v1/verify_context.py --output research/private/player-gameplay-context-v1/recheck
python3 research/private/player-gameplay-context-v1/retain_facts.py
```

A próxima entrega útil deve fechar `2b9f6c` e os registros espaciais que alimentam `b30fa0`, depois ligar a conclusão do carregamento dos bancos ao readiness observado. A inicialização integral de `bd9804/af8d68` permanece separada. Até essas provas, contexto explícito continua sendo mais fiel do que escolher uma ação fixa e chamá-la de startup original.

Os principais readbacks da sessão são `037/038` (bd9804/b3aa84), `070..072/084/093` (construtor e helpers), `091/094/095/135..138` (contextos e tabelas), `115/116/123..125` (criação, reset contextual, caller e classe derivada), `140/141` (b30fa0 completo) e `144/145` (slots derivados restantes). `source-facts.json` confirma **46 readbacks novos,10.596bytes**, além de dois readbacks reutilizados. O bundle contém143Evidence únicas; a resposta repetida de callers é preservada sem inflar essa contagem. Export146/close147 e saída0 estão em `lifecycle.json`, sem processos REA/Ghidra remanescentes observados. A soma dos tempos individuais das chamadas foi151,582s; não é uma medição de CPU nem do tempo total de investigação.
