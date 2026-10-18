# Aplicação de movimento do modelo — 0x95cab0

`motion_application::apply` implementa o consumidor numérico completo `95cab0`,
com amostragem real pelo dono `motion_sampler`, acúmulo de camadas, predição e
aplicação à posição/rotação do modelo. A implementação passou na comparação de
831 resultados finitos com as instruções ARM originais. Um caso adicional
executa uma normalização de quaternion zero na fonte; o consumidor nativo
retorna erro tipado e não publica o estado parcialmente calculado.

Isso fecha uma função do motor dentro do perfil declarado. Não determina
bootstrap, política de ações, cadência, câmera, colisão ou gameplay. A composição
com o relógio e as juntas pertence ao chamador. Unicorn é usado somente como
ferramenta privada de análise; o módulo C++ não contém runtime ARM/3DS.

## Fonte e propriedade

Fonte: ELF SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`,
intervalo `[0x95cab0,0x95de18)`, 4.968 bytes. O bloco retido tem SHA-256
`31b51f2ed9c7e8268d11b28101bf81b8253ec87bad1d0b609e17abc2088243d1`.
REA Evidence reutilizada:
`ev_489b357b29cb3198fb345b7875af1996eb4494a1b6e95353acaaddd5c420bef1`.
Pseudo e assembly estão em `research/private/root-motion/0095cab0.txt` e
`research/private/root-motion-runtime/application-v1/apply-llvm.asm`.
O novo oracle separa código e literais e verifica cada intervalo contra o ELF.
As expectativas antigas com substituição de sampler não foram usadas como
oracle desta implementação.

A única chamada externa de `95cab0` é `aea0e4`. O harness executa esse dispatcher
e os decoders originais necessários, sem substituir resultados ou modificar PC.
Os recursos LMT são obtidos pelo produtor de parsing existente; o adapter C++
usa `motion_resource::Resource` como dono de identidade, ações e tracks. Não há
segundo parser LMT no consumidor ou no adapter.

O estado de camada é `motion_pending::Layer`: ele contém `mi::State` e os campos
temporários `rotation_98`/`translation_c8`. Relógios, atributos, flags, máscaras
e identidades continuam nos donos existentes. Os campos preparados `+64/+68`
são lidos como entrada; esta função não inventa outro relógio.

| Região original | Dono nativo | Efeito nesta função |
| --- | --- | --- |
| Modelo +40/+50 | `Model.position/rotation` Bits4 | Aplicação final condicional |
| Modelo +60/+bd0 | `Model.scale/null_scale` Bits4 | XYZ consumido conforme modo; todo campo preservado |
| Modelo +be0/+bf0 | `translation_delta/rotation_delta` Bits4 | Delta ou template explícito |
| Layer +20/+30 | `mi::State.prediction_rotation/translation` | Predição condicional |
| Layer +78/+a8 | `previous_rotation/translation` | Recebe a amostra corrente |
| Layer +88/+b8 | `base_rotation/translation` | Preservado |
| Layer +98/+c8 | `mp::Layer.rotation_98/translation_c8` | Amostra corrente, com espelho se solicitado |
| Layer +d8/+e8 | `Tracks.translation/rotation` | Cursores atualizados pelo sampler |

Os demais campos de Layer e os bindings de juntas/eventos são preservados. O
original não lê nem escreve as flags globais de modelo `+238/+23c` nesta função;
elas permanecem no dono da composição, sem uma segunda cópia neste `Model`.

## Ordem observada

As camadas são percorridas em ordem crescente. `selector == 0xffff` pula toda a
camada. Nas demais, T e depois Q são amostrados em `prepared_frame`. T.W é gravado
como +0. `attributes & 0x8100` nega T.X, Q.Y e Q.Z, por bits de sinal.

O bit de atributo 1 exclui a camada do acúmulo, mas mantém amostragem, predição e
atualização de `previous_*`. Para contribuintes, `attributes & 0x2020` escolhe
entre amostra direta e delta relativo. O delta usa inversa de `previous_rotation`,
a rotação atual do modelo e a diferença de translação; não normaliza
automaticamente os quaternions de entrada. A inversa com norma zero usa o
template explícito `10e6570`, conforme a fonte.

O primeiro contribuinte inicializa o acumulador sem aplicar os pesos. Nos
seguintes, os pesos são `Layer.weight × Channel.weight`. O caminho normal usa
lerp de T e nlerp de Q; `attributes & 0x40000` soma T ponderado e compõe a rotação
aditiva interpolada a partir do template `10e6560`. Não há clamp de pesos.

Predição, após a acumulação de cada camada:

| Flags | Pares T/Q adicionais, nesta ordem | Base do delta |
| --- | --- | --- |
| `flags & 1` | `end_frame` | Amostra corrente |
| Sem bit 1, `flags & 4`, `field08 & 1` | Nenhum | Predição anterior preservada |
| Sem bit 1, `flags & 4`, sem `field08 & 1` | `end_frame`, `previous_frame` | Segundo par |
| Demais | `prepared_next_frame` | Amostra corrente |

No terceiro ramo, `previous_frame` é **Layer +50**, não `frame +4c`. Apenas o
primeiro par adicional recebe o espelho nesse ramo; o segundo conserva os bits
amostrados. A translação prevista é a diferença rotacionada pela inversa da
rotação base. A rotação prevista é o produto dessa inversa pela amostra futura.
Depois, `previous_*` recebe a amostra corrente, não a amostra de predição.

A aplicação final usa os atributos da **camada zero**, inclusive quando outro
índice foi o primeiro contribuinte. Com `0x2020`, T substitui a posição; `0x2000`
também aplica a escala do modelo. Sem esses bits, a função transforma o delta
para o espaço local, aplica `scale × null_scale`, transforma de volta e acumula
na posição. Esse cálculo usa o polinômio de quaternion, sem pressupor unidade.
O bit `0x80` substitui Q; caso contrário, compõe e normaliza Q. Quando não há
contribuintes, a pose permanece e os deltas recebem templates.

## API, domínio e erros

```cpp
motion_application::Result apply(
    const motion_application::Model&,
    std::span<const motion_pending::Layer>,
    const motion_initialization::Templates&,
    motion_initialization::Resolver,
    motion_application::Limits = {});
```

O resultado é `variant<Output, Error>`. `Output` contém o novo `Model`, as mesmas
camadas em ordem e contadores `sampled_layers`, `contributing_layers`, `samples`
e `prediction_updates`. Os limites padrão são oito camadas e 256 bindings de
juntas copiados por camada; são política de trabalho/alocação do chamador.

`Error` preserva camada, componente, identidade de track e erro do resolver,
sampler ou aritmética. Template desconhecido é diferente de vetor zero.
`base_translation_10e62c0` e `identity_10e6560` são obrigatórios na entrada,
inclusive com zero camadas; os demais templates são exigidos quando consumidos.
A falha não publica pose, predição ou cursor parcial. Efeitos externos de um
resolver continuam sendo responsabilidade do chamador.

A aritmética consumida exige binary32 finito, arredondamento nearest-even,
subnormais preservados e operações MUL/ADD separadas. `__FAST_MATH__` é rejeitado.
Inversa e normalização pertencem ao shared math; nlerp/produto/polinômio também
reutilizam esse dono. A ordem instrucional distinta do delta relativo é mantida
localmente: `(m01*y + m02*z) + (0 + m00*x)`. Cópias brutas preservam NaN, infinito,
signed zero e padding quando nenhum cálculo os consome. Produtos de peso
descartados pelo primeiro contribuinte não impõem um erro adicional; FPSCR e
exceções de ponto flutuante não são parte da API.

Os endereços ARM são abstraídos por identidades de recursos e storage separado;
overflow de ponteiros e alias entre objeto/track/código não pertencem a esse
perfil. O sampler continua sendo dono dos limites de cursor e extent.

## Oracle e validação executada

`research/private/motion-application-native-v1/verify_arm.py` executa as funções
originais com guards de código, leitura inicializada, escrita permitida,
imutabilidade de recursos e retorno/ABI. Os templates, bancos, modelo inicial,
quadros preparados e buffers de trabalho são explícitos dados de diagnóstico.
O prefixo original de pending/initialization prepara os canais reais. Os dois
LMTs de entradas 54913 e 92608 permanecem hash-bound; as únicas mudanças nas
cópias de análise são relocações de ponteiros derivadas pelo dono do formato.

O snapshot `arm-first` contém 734 execuções: 122 ações reais em quatro perfis
(488), 240 combinações sintéticas e seis fronteiras. `arm-complete` preserva os
734 registros e adiciona somente 98 execuções, em 14 sequências sobre sete ações
representativas. As sequências passam pelo início, meio, fim e retorno ao início,
preservando o resultado anterior como estado de entrada antes das mudanças de
clock declaradas. Não houve repetição dos casos base para aumentar a contagem.

Cada compilação normal e ASan/UBSan estrita passou:

- 832 execuções, 5.544 chamadas reais do sampler; 831 arenas de modelo idênticas
  bit a bit e um erro de domínio separado;
- 6.807.552 bytes de arenas ARM comparados, incluindo a região opaca inalterada;
  6.815.744 bytes de imutabilidade da entrada contados separadamente;
- 29.450 checagens no adapter e 45 fronteiras públicas;
- falha tardia de resolução de Q após avanço real de um cursor comprimido T,
  preservando o estado publicado;
- 28 processos negativos: hash, truncamento, comprimento, magic, contagens,
  identidade de fonte, resultado esperado adulterado e LMT adulterado.

Os guards negativos do harness também interromperam leitura de modelo/template
não inicializado, escrita proibida e orçamento insuficiente. Os testes públicos
distinguem ausência, templates desconhecidos, Q zero, operandos não finitos,
ambiente de arredondamento e bindings opacos não consumidos.

O adapter reconstrói somente os campos tipados do estado e projeta-os no arena
anterior; não usa uma segunda implementação da álgebra para gerar expectativas.
Confere identidades completas de track/cursor, inclusive banco, pesos, flags,
padding e campos preservados. Os traces de chamada/frame são observações ARM,
conferidas com o plano numérico dos gates; não há instrumentação de frame dentro
do sampler C++ na comparação. Seus cursores e resultados são comparados pelo
estado completo, e as falhas tardias usam a sequência de resoluções C++ observada.

Normal `-O2` e ASan/UBSan `-O1 -fno-sanitize-recover=all` foram compilados no
macOS arm64 com warnings como erros. Todas as execuções positivas tiveram exit 0
e stderr vazio. Os negativos falharam pela razão esperada, sem diagnóstico de
sanitizer; mutações estruturais receberam hashes recalculados para atravessar o
guard de identidade. Fontes e headers transitivos foram fixados antes/depois e
copiados no snapshot de compilação. Windows não foi executado nesta entrega.

Fixture autoritativo: `arm-complete/oracle.bin`, SHA-256
`655dbe45e6f2fc9d484bf9756dd9d015da73e9b4a68959341ffae97f319eb3c2`.
Relatório nativo: `native-strict/native-validation.json`, SHA-256
`c79c8f6a23f722392d92ccb141507c705e1e4c938afe7c368bad304950f35993`.
`incidents.json` retém a correção inicial do offset `+50`, detectada pelo trace
ARM antes do freeze. O fixture original não foi alterado para corresponder ao C++.

Reprodução em pasta nova:

```sh
python3 -B research/private/motion-application-native-v1/validate_native.py \
  --output research/private/motion-application-native-v1/native-replay
```

O executável público não recebe argumentos. O adapter recebe:
`oracle.bin SHA256 54913 fonte54913.lmt SHA256 92608 fonte92608.lmt SHA256`.
Os relatórios retêm os argv exatos. O oracle base levou 5,41 s de parede,
6,25 s de CPU e pico RSS do processo de 201.916.416 bytes; a extensão levou
0,55 s de parede. CPU/pico da extensão, família de processos e tokens não foram
medidos. Essas medidas descrevem análise, não desempenho de gameplay.
