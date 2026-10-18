# Consumo CPU do skinning VS18

`vertex_skinning::skin` aplica a álgebra recuperada do shader VS18 a vértices
fornecidos pelo dono de MOD230. O resultado é posição no espaço do modelo e
normal após skinning, após a view explícita e normalizada. A implementação usa
um perfil CPU binary32 declarado. A igualdade medida é com esse perfil, derivado
da fonte; não houve execução PICA nem medição da precisão gráfica do 3DS.

## Fonte e limites de atribuição

O contrato vem de [joint-shader-consumption.md](joint-shader-consumption.md), do
blob `018.shbin` (SHA-256
`89e6141929dd866282b0c42b066330ccf6fce97f54953001f58667fac3441383`) e do
produtor privado `joint-shader-consumption-v1/inspect_contract.py`. São o programa
32, VS_MaterialStdObj.asm e a variante sem morph estudada. Não se presume que
essa variante descreva todos os materiais, shaders ou formatos do jogo.

O literal float24 `370100` representa exatamente `0.0039215087890625`, bits
binary32 `0x3b808000`. Cada peso u8 é multiplicado por esse valor; a soma não é
renormalizada. Portanto soma de pesos 255 produz fator `0.9999847412109375`.
Todos os índices são consumidos, inclusive os de peso zero. A mistura segue a
ordem das duas ou quatro influências da fonte: primeiro MUL, depois MADs.

Cada entrada de paleta contém três linhas de quatro componentes. A posição
é o produto dessas linhas pela posição bruta com componente homogênea 1. A
normal signed XYZ usa a parte 3×3, seguida de outra 3×3 fornecida pelo chamador;
não há divisão por 127 nem inversa transposta. A transformação Q já participa
da construção da paleta a partir das matrizes B. O consumidor não a reaplica à
posição bruta.

O perfil CPU usa operações binary32, arredondamento para o mais próximo com
empates para par e subnormais preservados. Cada MUL e ADD arredonda separadamente;
os produtos escalares acumulam da esquerda para a direita. A normalização usa
`sqrtf`, recíproco binary32 e multiplicação. Precisão float24, fusão MAD,
acumulação DP e aproximação RSQ da PICA continuam não medidas. O caminho
contrafactual com MAD fundido diferiu em 950 consultas do conjunto: isso distingue
os perfis de teste, não identifica qual resultado o hardware produz.

## API e propriedade

```cpp
vertex_skinning::Result skin(
    const model_resource::Vertex& vertex,
    std::span<const vertex_skinning::Palette> palette,
    const vertex_skinning::View3& view,
    vertex_skinning::Limits limits = {});
```

`Vertex` é o tipo canônico de `model_resource`; este consumidor não lê bytes MOD.
`Palette` contém 12 palavras raw binary32 e `View3` contém nove, em linhas. Todos
os argumentos são emprestados e permanecem inalterados. Não há alocação dinâmica
no consumidor. O limite padrão é 24 entradas, correspondente às 72 linhas do
upload VS18 investigado; o chamador pode declarar outro limite CPU, sem ampliar
por isso o contrato do shader original.

O `Result` é `variant<Output, Error>`. `Output` contém quatro `Bits3`:
`position_model`, `normal_skin`, `normal_view` e `normal_unit`. A falha não
publica saída parcial. `Error` preserva código, estágio, influência quando
aplicável, componente e índice de paleta. Os códigos distinguem contagem de
influências não suportada, limite de paleta, índice inválido, ambiente de ponto
flutuante, operando não finito, overflow intermediário e normal degenerada.

Somente duas ou quatro influências são aceitas. Os slots 2/3 da variante com
duas influências são ausentes e não consumidos. UV, normal W e auxiliar signed
também não participam deste recorte. Entradas de paleta não referidas podem
conter dados opacos/não finitos. Todo componente de uma matriz referida é
consumido mesmo com peso zero. Operandos e intermediários consumidos devem ser
finitos; comprimento quadrado zero, inclusive por underflow binary32, é erro.

O guard de ambiente reutiliza o dono `motion_transform_math`, verifica o modo de
arredondamento e gradual underflow, e a compilação rejeita `__FAST_MATH__`.
O perfil não inclui câmera, projeção, UV, iluminação, materiais, morph ou
recuperação de gameplay. View identidade é uma decisão explícita do chamador,
não uma câmera original descoberta.

## Expectativas independentes e validação

Os arquivos privados estão em `research/private/vertex-skinning-v1/`.
`cpu_algebra.py` implementa a álgebra CPU em Python sem ler o C++. O produtor
`make_fixture.py` importa `source_geometry.extract` do dono de geometria bruta;
reutiliza os donos históricos de skeleton e paleta para construir entradas.
Não instancia um harness ARM nem executa a ROM. A construção das paletas é
entrada do teste; a álgebra de skinning esperada fica separada dela e do C++.

Foram usados em062, body001 e one133: 3.659 vértices únicos de oito primitivas,
com layouts de duas e quatro influências. Cada vértice foi consultado em duas
poses declaradas, uma de referência e outra deformação sintética, totalizando
7.318 consultas reais. Outros 44 casos discriminam soma de pesos 255/510,
índice inválido de peso zero, entradas não consumidas, normal direta versus
inversa transposta, Q duplicada, underflow, overflow e misturas de quatro pesos.
As poses e views são dados de teste, não estados capturados de gameplay.

O adapter recebe o vértice diretamente por referência de `Geometry.vertices`,
confere sua identidade e todos os campos com a fonte, e compara os quatro
vetores resultantes. Verifica também imutabilidade do vértice, paleta e view,
inclusive nos caminhos de erro.

| Medida por compilação | Resultado |
| --- | ---: |
| Consultas | 7.362 |
| Saídas numéricas válidas | 7.355 |
| Erros de domínio tipados | 7 |
| Palavras numéricas comparadas exatamente | 88.260 |
| Bytes numéricos comparados | 353.040 |
| Checagens totais, incluindo leitor/identidade/metadados | 967.897 |
| Fronteiras públicas executadas separadamente | 29 |

Normal `-O2` e ASan/UBSan `-O1 -fno-sanitize-recover=all` passaram no macOS arm64
com Apple Clang 21, exit 0 e stderr vazio em todas as execuções positivas. O
validador exige os resultados/contagens exatos. Ele enumera dependências
transitivas pelo compilador, fixa fontes/headers/inputs antes e depois, e guarda
cópias das fontes compiladas e hashes dos binários. Não houve repetição em
Windows nesta entrega.

Foram executados 28 processos negativos, 14 por compilação: digest incorreto,
truncamento, bytes excedentes, magic, contagens, identidade MOD, limite de
paleta, vértice adulterado, resultado esperado adulterado, grupo/erro inválido
e MOD modificado. Para negativos estruturais, o hash do fixture adulterado foi
recalculado; portanto eles atravessaram o guard de identidade e alcançaram a
checagem relevante. Cada processo falhou pela razão esperada, sem diagnóstico
de sanitizer. O container de teste é limitado/versionado e não é um formato de
asset necessário ao motor.

O fixture autoritativo é `source-final/oracle.bin`, SHA-256
`7bb3042c91254d3ce41f0f7e9cd019a2dcfa112b53decee6558080ded644f43b`.
O relatório final é `native-strict/native-validation.json`, SHA-256
`60ab6c21168ebaa7ad9064d3f6467e9ecd727caa1e54236e08e73675618a050a`.
`source-first/` e `incidents.json` preservam o primeiro mapeamento de códigos
de erro do produtor, corrigido antes do fixture autoritativo, e uma correção de
teste de `nodiscard`. Não houve correção numérica do runtime durante essa etapa.

Para repetir a validação, use uma pasta nova para preservar o relatório:

```sh
python3 -B research/private/vertex-skinning-v1/validate_native.py \
  --output research/private/vertex-skinning-v1/native-replay
```

O executável de teste aceita, além do modo sem argumentos para fronteiras:

```text
tests-normal FIXTURE SHA256 EM062_MOD BODY001_MOD ONE133_MOD
```

O relatório retém argv e tempos por fase. O replay numérico levou cerca de
0,022 s normal e 0,064 s sanitizado, sem alegação de benchmark de renderização.
Tempo de CPU, pico da família de processos e tokens não foram medidos.
