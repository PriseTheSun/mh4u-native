# Consulta nativa de movimento do modelo

`motion_query::query`, em `src/motion_query.hpp/.cpp`, reproduz `0x958884`:
amostra os canais de translação e rotação das layers em um frame solicitado e
combina seus valores. Essa consulta participa dos wrappers do personagem;
não é a atualização de seus ossos. A [investigação REA](joint-update.md)
registra funções, offsets, ordem dos callers e proveniência.

## Propriedade e resultado

`LayerView` empresta o `motion_request::Layer` e os dois
`motion_binding::Channel` existentes. A quantidade de views representa a
contagem ativa explícita do modelo. Não há nova representação de relógios,
headers LMT, joints ou bancos. O resolver existente da inicialização fornece
views de tracks do mesmo recurso imutável ao sampler canônico.

O resultado contém os dois channels de cada layer, na ordem original,
inclusive as inativas, e as saídas T/Q solicitadas. O chamador só aplica os
canais após sucesso; uma falha tardia não publica cursores parcialmente
avançados. Falhas de resolução e sampling conservam identidade, layer,
componente e diagnóstico do dono. Efeitos externos de um callback pertencem
ao chamador e não são desfeitos pelo módulo.

Views e recursos devem sobreviver à chamada. O limite padrão de oito layers
é política de alocação/trabalho, ajustável pelo caller, e não um máximo
inferido do jogo. O limite é conferido antes de reservar o vetor de resultado.
O orçamento de percurso de chaves pertence ao sampler.

## Ordem e matemática

A entrada lê os snapshots globais T `10e62c0` e Q `10e6560`, mesmo com zero
layers ou ambas saídas nulas. Os demais templates da inicialização não são
consumidos. Sem layers aceitas, esses snapshots são as saídas, com T.w=+0.

As layers são percorridas em ordem crescente; selector `ffff` e flags&2
pulam a layer. O frame é limitado primeiro pelo endpoint e depois por zero,
usando comparações IEEE ordenadas: NaN é unordered, e −0 preserva seu sinal.
Attr800 aplica o helper `d12f8c`, que arredonda para baixo. Sua implementação
por expoente/mantissa preserva os caminhos de retorno raw para NaN/Inf e
valores de expoente pelo menos23. A soma com a constante positiva `7149f2ca`
do helper apenas controla um ramo que é sempre positivo no domínio menor;
não introduz um novo arredondamento no frame de saída.

O sampler é chamado primeiro para T e depois para Q, inclusive com tracks
nulos. O retorno nulo é `[0,0,0,1]`, conforme seu dono; não usa templates de
referência dos joints. Attr100 inverte o sinal de Tx, Qy e Qz. A primeira
layer aceita copia os valores raw: não normaliza Q nem aplica os pesos.
Os produtos de pesos calculados e descartados nesse caminho não restringem
o domínio das cópias.

Nas layers posteriores, os pesos são `layer.weight * channel.weight`:

- Mistura normal: T usa `atual*(1-peso)+amostra*peso`; Q usa lerp com ajuste
  de hemisfério e normalização do resultado.
- Attr40000: T soma a amostra ponderada. Q mistura o template global com a
  amostra e multiplica esse delta à esquerda do Q corrente, sem normalização
  adicional do produto.

`motion_transform_math` é o dono dessas operações também usadas nas folhas
de joints. Cada multiplicação, soma e subtração arredonda separadamente em
binary32, sem FMA. O produto escalar e a norma percorrem X,Y,Z,W na ordem
ARM. Não há clamp dos pesos. Aritmética consumida requer operandos e
intermediários finitos, nearest-even e gradual underflow; norma zero é erro
de domínio, sem fabricar uma rotação identidade. Cópias e predicados de bits
preservam valores não finitos. Flags de exceção acumuladas da FPU não fazem
parte do estado de saída modelado.

Saída nula suprime somente a escrita final. Mesmo com T e Q ausentes, todos
os samples e blends acontecem, seus cursores são retornados e seus erros
continuam visíveis. T.w é sempre +0; Q tem os quatro words calculados.

## Verificação e fronteiras

Os testes públicos cobrem templates ausentes, layers ignoradas, saídas nulas,
ordem T→Q, identidades incorretas, estados raw, mistura normal/aditiva,
hemisfério, domínio aritmético e erro transacional. O
[oracle composto](motion-query-oracle.md) compara execuções ARM originais,
incluindo tracks reais, várias layers e consultas sucessivas que mudam
cursores. Os resultados normais/sanitizados e hashes de fontes ficam em
`research/private/motion-query-native-v1/`.

O snapshot final `arm-complete` cobre 5.002 consultas em 860 sequências:
4.998 resultados bit-exatos e quatro rejeições explícitas por norma zero.
Cada variante normal/ASan+UBSan passou em 171.074 verificações de recursos,
comparando 41.103.552 bytes de estado e saídas, além de 91 verificações da
API. Há 189 consultas sem ambas as saídas que ainda modificam cursores. O
teste de falha tardia resolve T comprimido antes de rejeitar Q e confirma
que o estado emprestado permanece intacto.

A primeira compilação e validação, sobre `arm-final`, foi conservada. O
replay `cursor-coverage-validation.json` reutiliza os mesmos binários e
fontes, conferidos por hash, contra a cobertura ampliada de saídas ausentes.
O SHA-256 do fixture final é
`699799261bf45dd60445e7fa603f9e9c42390d55368527ab2083c6b035ddb603`.
Os frames internos do sampler permanecem evidência ARM retida; não foi
instrumentado um trace nativo para comparação direta desses frames.

Mapas, bancos, estados e templates de ensaio são explícitos; não estabelecem
a montagem real do caçador. O módulo ainda precisa ser composto com os
wrappers que consultam o deslocamento, com o relógio e com a atualização dos
joints. Não altera a cena de diagnóstico aberta, não define FPS e não
comprova combate, scheduler completo, execução Windows ou coop jogável.
