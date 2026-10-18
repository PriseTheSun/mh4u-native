# Oracle independente da paleta nativa

O adapter `src/joint_palette_resource_tests.cpp` compara `joint_palette::prepare` e `bind` com execuções completas do original `959d00`, seus helpers reais e chamadas explícitas de `6bdc44`. São **94 casos: 89 cadeias integralmente comparáveis, dois erros do domínio aritmético nativo e três bindings fora da capacidade nativa declarada**. Os 43 casos históricos permanecem preservados; seus hashes de arenas finais coincidem com a fonte histórica. Unicorn é ferramenta privada de análise, sem participação no runtime do porte.

A comparação verifica também cada valor escrito antes de uma paleta posterior sobrescrevê-lo. Isso evita que destinos sobrepostos escondam uma divergência na primeira paleta. A expectativa vem das escritas ARM retidas, sem incorporar a implementação C++ ao modelo esperado.

## Fonte e artefatos

A proveniência dos bytes e seus Evidence IDs está em [joint-render-binding.md](joint-render-binding.md). O ELF privado tem SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`; a sessão REA `1792292721003` já estava encerrada. Não houve nova importação nesta etapa.

| Artefato privado | Papel |
| --- | --- |
| `joint-render-binding-v1/manifest.json`, SHA `58061bf87aebb97fe6062abd5708aeb744219972595e118de4a45a50c716e52a` | Fonte histórica fixada |
| `joint-render-binding-v1/verify_palette.py`, SHA `02ef7db5f1de73a6f8c32b16f09e0a5d2f2600fca2cec6aba5bc40d99e0f4de8` | Harness original importado, guards e donos Python de MOD |
| `joint-palette-composition-v1/arm-observed/` | Execução canônica dos 94 casos, arenas, traces e relatório ARM |
| `joint-palette-composition-v1/arm-write-observed/` | Fixture v2 derivada dos mesmos traces, sem nova execução ARM |
| `joint-palette-composition-v1/native-layout-name/` | Builds, fontes copiadas, replay e negativos finais |
| `joint-palette-composition-v1/artifact-audit.json` | Auditoria das 282 arenas/snapshots e cobertura de leituras/escritas |

Os caminhos da tabela partem de `research/private/`. A fixture v1 preservada tem SHA `16b7aa350a0611227173ef5d8347e8aeae62cef92754646f133eb5a640800174`. A fixture **v2 autoritativa para o adapter** tem SHA `e4e33ecaad1e411566a9b5bb5a4628819640c24f12f89201c2523c596bc405b0` e 52.512.376 bytes. `extend_store_fixture.py` verifica os pins da v1 e de seus traces, retém todos os bytes anteriores e acrescenta 4.738 observações originais de endereço/tamanho/valor. `format-validation.json` registra a transformação. Snapshots anteriores, `native-strict` e `native-immutable` continuam históricos. O manifesto anterior é preservado byte a byte em `manifest-before-layout-name.json`. A revisão final renomeia somente o fato MOD `Primitive::shader_hash` para `input_layout_hash`, pois o campo +0x14 identifica o layout de entrada, não um programa shader. O diff e os pins da correção estão em `research/private/model-resource-v1/layout-name-change/`; o adapter, a fixture e o cálculo da paleta permanecem iguais. A validação foi repetida com os headers corrigidos, conservando as mesmas contagens e resultados.

## Execução e perfil

`verify_arm.py` importa o harness histórico e executa somente os intervalos fixados:

- `959d00..95a108` e `95a118..95a6f0`, com seus literais;
- `1a5c10..1a5d40`, command buffer com capacidade existente;
- `6be688..6be730`, reserva normal sem lock;
- `6bdc44..6bdd34`, binding normal.

Guards exigem código autorizado, leitura inicializada, escrita dentro da extensão declarada e retorno real. O orçamento e os registradores preservados pelo ABI também são verificados. Os hooks apenas observam PCs, leituras, stores e retornos reais de `6be688`; não substituem resultados nem reescrevem PC. Onze arenas totalizam 184.320 bytes por snapshot; há estado anterior, após preparação e após a sequência de bindings, além do contexto após cada binding. Bytes opacos começam com padrões determinísticos, não com uma suposta imagem original zerada.

Foram executadas **94 preparações, 115 bindings e 101 consultas/retornos de reserva**, das quais 81 retornaram destinos e produziram stores de dirty words. O corpus contém 60.291 instruções, 20.061 leituras e 9.396 escritas. A sequência de chamadas de binding é uma entrada explícita do harness; o dispatcher de desenho não foi executado.

O perfil declara history selector negativo, compartilhamento pelo pai desabilitado, auxiliar `R+214=0`, contexto `+1bc=0`, lock local/global zero, buffer de histórico ausente e capacidade suficiente de comando. Handles, regiões, stride, dirty words, contexto e poses são entradas. Não foi executado seu alocador/startup nem inferido reset por frame. O ramo `N=0` ainda executa o helper de comando e não consome matrizes.

O domínio numérico nativo exige operandos/resultados aritméticos finitos, arredondamento nearest-even e subnormais graduais. Os dois casos de não finito consumido e overflow retêm a saída original, mas esperam erros nativos explícitos sem publicação parcial: **não são equivalência de resultados**. NaNs nas colunas W não consumidas ou em tabelas puladas continuam admitidos. As três consultas de binding com endereço original além dos 65.536 bytes declarados comparam a preparação, mas exigem `buffer_extent` no binding; o original só calcula o endereço, enquanto a API nativa exige propriedade da extensão.

## Casos discriminantes e propriedade

O corpus amplia os 43 históricos com destinos sobrepostos (strides 0, 4, 12, 48 e 96), handles duplicados, índices inválidos pulados, valores não finitos não consumidos, count de paleta e descriptor com high16 distintos, descriptor low16 zero, tags extremos e preservação do tag quando o endereço coincide. O descriptor zero avança zero bytes, mas ainda grava o word do modelo: esse store é comparado.

Slots 0, 1, 31, 32, 33, 255, 256, 257, 511, 512 e 1023 distinguem o shift literal ARM por registrador: somente low8 do slot participa; shift de 32 ou mais gera máscara zero. Dirty stores com valor zero continuam observações de escrita, não ausência. A reserva seguinte com mesmo handle pode ser pulada por dirty bit, enquanto máscaras zero permitem novo destino. O adapter compara todos os retornos e dirty stores em ordem; bank/slot/mask são fatos decodificados do contrato fonte e a presença/endereço de retorno é observada diretamente.

Os seis casos de MOD usam dois recursos, cada qual em três poses declaradas:

| Recurso | SHA-256 | Juntas / paleta |
| --- | --- | --- |
| `assets/local/actor-probe/em062.mod` | `2a9e74fe29a24623a0aab96cfd71e918ada70a279355cd232c7799bac1cf2715` | 12 / índices 1..11 |
| `assets/local/player-probe/body001/source/entry-44780.bin` | `f63bc643e17f05156f77734e0797ee2e3f135f118fe29bed34e5112a339178a3` | 19 / índices 0..18 |

No lado ARM, os donos Python históricos fornecem a tabela B exata e uma pose global offline, com duas opções de vetor de contexto e uma perturbação diagnóstica. Não são poses capturadas do jogo. No lado C++, `model_resource::Resource` é o único parser MOD: o adapter compara os fatos da fixture, empresta suas tabelas/paletas e propaga `Resource::identity()` até cada `Write.input`. O token identifica o recurso vivo; o SHA fixa os bytes entre processos.

Para as seis entradas reais, `JointMatrices` empresta diretamente `joint_output::State.joints` (AoS), sem extrair outra tabela W. Cada endereço retornado pela view é conferido contra `joint.matrix`; as 93 juntas, inclusive campos que a paleta não consome, são comparadas antes/depois. Isso valida o empréstimo do dono, mas **não executa `joint_output::apply` neste oracle**. Os casos sintéticos usam a view de matrizes contíguas. Todos os casos relêem B, W, handles, índices e dirty words após `prepare`, inclusive nas rejeições, verificando 124.849 bytes de tabelas emprestadas. Callbacks/owner dos joints são explicitamente desconhecidos e não consumidos por esta etapa.

## O que o adapter compara

Os endereços ARM existem apenas na fixture e no adapter. A API recebe tokens de alocação estáveis e offsets. O adapter projeta `Location` de volta para as arenas para comparação; tags permanecem separados. Verifica também identidades de recurso/pose/contexto/paleta, índices fonte, campos de reservas, presença de destino e identidade/extensão/fim do command buffer. Assim, uma projeção de bytes não encobre esses metadados adicionais.

Cada patch é confrontado com a sequência de stores original correspondente, antes de aplicar o patch seguinte. Dentro de uma matriz, o ARM grava endereços em ordem não contígua; a API devolve um vetor de palavras; o adapter projeta cada palavra em little-endian. O teste compara cada endereço/valor exatamente uma vez dentro do patch e preserva a ordem **entre patches**. Não afirma equivalência da ordem de instruções para palavras disjuntas dentro do mesmo patch. Aliases entre patches e entre elementos de paletas diferentes continuam preservados.

| Medida final por variante nativa | Quantidade |
| --- | ---: |
| Checks | 42.547 |
| Cadeias comparáveis completas | 89 |
| Preparações comparadas | 92 |
| Contextos após binding comparados | 110 |
| Arenas completas comparadas com ARM | 33.361.920 bytes |
| Contextos de binding comparados separadamente | 450.560 bytes |
| Valores de stores comparados antes de sobreposição | 18.848 bytes |
| Destes, palavras de paleta / comando | 18.480 / 368 bytes |
| Releitura real das tabelas emprestadas | 124.849 bytes |
| Projeção anterior preservada, incluindo bytes opacos | 16.957.440 bytes |

As medidas se sobrepõem e **não devem ser somadas como payload produzido**. Nas arenas completas, somente 36.704 bytes de paleta, 724 de comando e 4.704 de outros campos são cobertos por stores observados, contando a união em cada snapshot comparado. Os outros 33.319.788 bytes são preservados/opacos. Nos contextos de binding, 1.116 bytes têm stores observados e 449.444 permanecem iguais. A projeção anterior copia os bytes opacos sem reinterpretá-los; a releitura de tabelas e as igualdades de Context/dirty/AoS são as verificações diretas de empréstimos. A auditoria privada preserva essas distinções.

## Validação e reprodução

`native-layout-name/native-validation.json` fixa os quatro fontes compilados, todos os headers locais transitivos enumerados por `clang++ -MM`, os dois MODs, a fixture, o compilador e os executáveis. Há cópias dos fontes/headers. Pins são conferidos antes/depois de builds, replays e controles negativos. As duas variantes passaram com as contagens exatas acima: otimizada `-O2` e ASan/UBSan `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`. O produtor exige exit zero e stderr vazio nos casos positivos.

Dezessete mutações rodaram como **34 processos reais**, em ambas as variantes: digest externo, truncamento, byte extra, magic, contagem, arena, SHA MOD, tamanho de registro, joint count, source index, store count, byte opaco de saída, destino de reserva, dirty store, valor do primeiro patch ocultável por alias, largura de store e arquivo MOD alterado. Mutações estruturais recebem digest recalculado para alcançar o parser; todas retornam erro esperado sem relatório de sanitizer. O teste de patch oculto altera apenas a observação intermediária, deixando arenas finais iguais. Há ainda testes tipados de descriptor ausente, lock, history, armazenamento inválido, falta de capacidade e buffer de histórico. O harness ARM retém oito controles negativos próprios: mutex, alocação, histórico, leitura inválida, global ausente, orçamento, descriptor65535 e destino fora de extensão; são guards de análise, não validações atribuídas ao jogo.

```sh
python3 -B research/private/joint-palette-composition-v1/validate_native.py \
  --output research/private/joint-palette-composition-v1/native-replay \
  --oracle research/private/joint-palette-composition-v1/arm-write-observed/oracle.bin \
  --sha256 e4e33ecaad1e411566a9b5bb5a4628819640c24f12f89201c2523c596bc405b0
```

A saída deve ser nova. O executável recebe exatamente `oracle.bin sha256 em062.mod body001.mod`. Repetir o adapter não requer Ghidra, Unicorn ou execução da ROM. Para regenerar a observação ARM, use o venv privado e `verify_arm.py --output <nova-pasta>`; a fixture v2 é uma transformação posterior explícita do snapshot fixado por `extend_store_fixture.py`.

O ARM canônico mediu 0,259 s de parede, 0,251 s de CPU e 114.573.312 bytes de pico RSS do processo em macOS. O relatório nativo mede separadamente o tempo de cada compilação, replay e controle. CPU/família de processos, memória de toda a família e tokens nativos não foram medidos. Não houve ensaio Windows independente neste escopo. Buffers reais da GPU, shader, pesos/normais, aquisição de handles, scheduling, histórico, compartilhamento pelo pai e alocação/locks continuam fora deste perfil. Nenhum resultado aqui estabelece renderização completa ou gameplay.
