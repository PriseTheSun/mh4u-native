# Paletas nativas e planos de buffer

`joint_palette` implementa a equação de `959d00` e o perfil normal comprovado de
preparação e binding. O resultado é um conjunto imutável de palavras e mudanças
de estado para armazenamento registrado pelo chamador. Nenhum endereço ARM é
embutido no runtime, e nenhum buffer gráfico ou shader é simulado.

A implementação usa `src/joint_palette.hpp/.cpp`, testes em
`src/joint_palette_tests.cpp` e a operação única `palette_product` de
`motion_transform_math`. As provas privadas ficam em
`research/private/joint-palette-native-v1/` e no oracle independente
`research/private/joint-palette-composition-v1/`.

## Fonte e perfil

A investigação congelada [joint-render-binding.md](joint-render-binding.md)
possui manifesto SHA-256
`58061bf87aebb97fe6062abd5708aeb744219972595e118de4a45a50c716e52a`.
O ELF fonte tem SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Foram lidos o assembly de `959d00`, `1a5c10`, `6be688` e `6bdc44`, incluindo
literais e ramos que o perfil não executa. A associação a MOD vem do loader
`6806c0` e do dono [model_resource](model-resource-runtime.md).

As Evidence principais são:

| Trecho | Evidence |
| --- | --- |
| `959d00`, assembly | `ev_be23db2c9ac9ae3704ed78e445b2fe3462fa80eba73a91b26507b71756740343` |
| `959d00`, bytes | `ev_76c01fe415465a782e119d6622389544d168ebd32b0af5bacb351765f09f3564` |
| `6bdc44`, assembly | `ev_53c86d9594aa6945eed676160c78e8bc271ec3fca594a5468f1c90a7cb3e0a52` |

Os arquivos `022-procedure_assembly.txt` e `023-procedure_assembly.txt` da
investigação retêm `6be688` e `1a5c10`. Os offsets citados nesta documentação
são hexadecimais; contagens e limites em bytes são decimais.

## Donos e API

`ResourceView` empresta B e paletas já interpretadas pelo dono MOD. `PaletteView`
recebe somente os índices que o dono extraiu pelo low16 do descriptor; este módulo
não repete o parser, não usa os bits superiores como count e não calcula inverse
bind. `InputIdentity` preserva IDs explícitos de recurso, pose, contexto e ordinal
de paleta. Esses IDs são rótulos de snapshots emitidos pelos respectivos donos;
não são hashes truncados nem prova autônoma de associação dos dados.

`JointMatrices` empresta `span<const Matrix>` para os oracles ou
`span<const joint_output::Joint>` para o dono canônico da hierarquia. Em ambos os
casos, `size()` e `operator[]` acessam os mesmos bits sem extrair/copiar matrizes
por quadro. A view não prolonga o lifetime do dono; ele precisa permanecer vivo
e imutável durante a chamada síncrona. Não é necessário ligar o runtime de
`joint_output` somente para ler seus tipos.

As operações são separadas:

- `generate(B, JointMatrices, índices, vetor, identidade)` devolve 12 palavras
  por índice, índices fonte em ordem e identidade de entrada.
- `prepare(model, resource, context, manager)` devolve contexto/flags resultantes,
  reservas observadas, mudanças ordenadas dos dirty words e patches de comando/
  paleta. Não modifica o chamador.
- `bind(context, manager, handle)` devolve o contexto após o ramo normal de
  `6bdc44`. É uma operação distinta: o original não vincula automaticamente cada
  paleta produzida em `959d00`.

`Unknown`, `Null` e presença explícita são fatos diferentes. Campos não usados
pelo ramo admitido permanecem desconhecidos. Por exemplo, zero joints retorna
após o comando inicial; handle sentinel não consome manager; buffer normal null
não consome índice, stride, lock ou matrizes; dirty bit já marcado não consome a
paleta nem os locks. O perfil de parent sharing aceita o byte ativo com pai
explicitamente null, mas rejeita pai presente ou desconhecido quando consultado.

## Ordem numérica

A matriz fonte tem quatro linhas consecutivas na memória. Para `c=0..2` e
`r=0..3`, a operação compartilhada calcula:

```text
v = B[r,0] * W[0,c]
v = v + B[r,1] * W[1,c]
v = v + B[r,2] * W[2,c]
v = v + B[r,3] * W[3,c]
out[4*c+r] = v                 quando r != 3
out[4*c+3] = v + context[c]    quando r == 3
```

Cada produto/soma arredonda separadamente para binary32, sem FMA. Todos os
16 valores de B e apenas as três primeiras colunas de W são consumidos; a quarta
coluna de W pode conter NaN sem bloquear a operação. Não há hipótese de matriz
afim, normalização ou rearranjo de valores pelo chamador.

Aritmética consumida exige operandos/resultados finitos, round-to-nearest-even e
subnormais graduais. O teste altera/restaura FPCR.FZ ou MXCSR.FTZ/DAZ para provar
rejeição real do ambiente incompatível. Paleta vazia não executa aritmética nem
exige ambiente float. O perfil não compara flags de exceção IEEE. Erros incluem
ordinal da paleta, índice da entrada e causa aritmética original.

## Preparação e endereços lógicos

O comando inicial de `1a5c10` precede até o gate de zero joints. O tamanho reservado
é `round_up_16(4 * low16(descriptor))`; mesmo tamanho zero copia o word `model+154`
quando o destino admitido é não null. A capacidade para essa escrita de quatro
bytes é validada separadamente. São preservados cursor anterior em `command638`,
novo cursor, OR de bit0 em `flags188` e limpeza de bit1 em `flags4c`. O caminho de
material é rejeitado se `((flags1bc >> 16) & 0x813) != 0`; crescimento de command
buffer também é rejeitado quando seria solicitado.

Com joints presentes, o perfil exige recurso auxiliar `R+214=0`, histórico
`O+c50` negativo e ausência de compartilhamento efetivo com pai. As flags baixas
de `O+104` são reduzidas a bit0 somente quando alguma das seis estava presente.

O helper normal usa `bank=handle&0xfff`, `slot=(handle>>12)&0xfff` e a máscara ARM
`1 LSL (slot&255)`, que é zero para shifts de 32 a 255. `ffffffff` retorna sem
consultar manager. Dirty words são observados e atualizados em ordem: handles
repetidos veem a escrita anterior, inclusive escritas sem alteração quando a
máscara é zero. O destino é `normal + stride*bank + 48*slot`. Locks locais/globais
precisam estar conhecidos e zero no ramo que efetivamente os consulta.

`Location` separa token de armazenamento e offset u32. O token identifica uma
alocação viva, não null, com origem alinhada a 16 bytes. `Region` declara a
extensão acessível. Cálculos usam largura ampliada; domínio admitido rejeita wrap
u32, região insuficiente, token zero e escritas desalinhadas a quatro bytes.
Para equivalência de endereços, o chamador registra regiões com origem/offsets
não sobrepostos aos structs de contexto/manager/modelo e às tabelas fonte; deve
usar o mesmo token para views da mesma alocação. Regiões distintas não podem
esconder alias físico sob tokens diferentes. Patches de destino podem coincidir
ou sobrepor-se entre si e são preservados em ordem, inclusive stride zero.

O binding calcula a mesma localização, sem tratar `ffffffff` como sentinel.
O tag é `((handle>>24)+1)/2`, reduzido a zero se maior que 15. O endereço antigo é
comparado sem low4; se coincide com o destino, o tag antigo permanece. Caso
contrário, o novo estado preserva os low4 do endereço calculado em OR com o tag.
Assim, destinos alinhados apenas a quatro bytes continuam distintos do endereço
mascarado que será comparado na próxima chamada. Os campos `438` e `43c` são
atualizados separadamente. Buffer normal null pula ambos; histórico null deixa
`440` intacto. Histórico presente ou desconhecido é uma rejeição explícita.

O perfil de regiões não representa ponteiro null obtido por overflow nem o
fallback para os campos `450/458/460` de outro objeto. Binding fora da extensão
registrada é recusado mesmo quando o original apenas formaria o endereço sem
dereferenciá-lo. Essa é uma restrição de capacidade do novo dono, não um erro
atribuído ao original.

O plano é transacional: falha tardia não publica comando, flags, dirty words ou
paletas parciais. O aplicador deve verificar os mesmos snapshots/allocations,
aplicar todas as mudanças de uma vez e respeitar a ordem de patches. Não há
concorrência durante a operação, emulação de mutex, reset implícito de dirty bits
ou alegação de equivalência de lifetime/cache por frame. O módulo não executa um
plano em armazenamento bruto nem gerencia recursos gráficos.

## Validação e limites

O teste público local passou **202 verificações** tanto com otimização normal
quanto com ASan/UBSan estrito, `-fno-sanitize-recover=all`, stderr vazio e contagem
exata. O produtor retém argumentos, compilador, hashes antes/depois e snapshots de
todos os headers transitivos em `joint-palette-native-v1/native-final/`.

```sh
python3 research/private/joint-palette-native-v1/validate_native.py \
  --output research/private/joint-palette-native-v1/native-replay
```

O oracle independente executou 94 chamadas originais de `959d00` e 115 bindings
`6bdc44`, preservando os 43 casos históricos. Observou 101 retornos reais de
`6be688`, dos quais 81 destinos foram produzidos, e 81 stores de dirty words.
A captura canônica é `joint-palette-composition-v1/arm-observed/`; um fixture
v2 derivado de seus traces retém 4.738 escritas originais para comparar valores
antes que aliases posteriores os sobreponham. O adapter compara 4.712 desses
stores; os demais pertencem aos dois casos aritméticos rejeitados. Essa
derivação não é uma nova
execução ARM. O corpus inclui seis casos com o dono MOD e 93 joints emprestados
do AoS canônico. As poses dos recursos reais foram calculadas offline; as
perturbações são explicitamente sintéticas.

O [adapter independente](joint-palette-oracle.md) passou **42.547 checks por
variante** normal e ASan/UBSan estrita: 89 cadeias completas, 92 preparações e
110 contextos de binding comparados. Duas entradas retêm o resultado ARM e
recebem erro numérico nativo; três bindings recebem erro de extensão registrada,
sem alegação de igualdade desses resultados. Foram rejeitadas 17 mutações em
cada variante, inclusive uma escrita intermediária escondida por alias, e
relidos 124.849 bytes de tabelas emprestadas. O relatório autoritativo é
`joint-palette-composition-v1/native-immutable/native-validation.json`.

O destino deve ser novo. Cobertura inclui matrizes gerais, valores não finitos
somente em componentes ignorados, overflow consumido, ambiente float incompatível,
limites cumulativos, empréstimo AoS, aliases, máscara zero, ordem de dirty words,
falha tardia, descritores zero/upper16, tags, buffers null/desconhecidos e gates
fora do perfil. Os oracles ARM independentes e seu adapter são descritos no
manifesto final; nenhum valor esperado do ARM é gerado pelo novo código C++.

Os limites padrão são políticas do novo motor: 256 matrizes/paletas, 24 índices
por paleta, 6.144 índices totais, 64 dirty words, 257 patches e 16 MiB por buffer.
O count de 24 também corresponde à capacidade do registro MOD de 28 bytes;
não é uma checagem observada em `959d00`. Limites ajustáveis não autorizam ler
fora das views ou fabricar presença/identidade.

Continuam fora desta entrega: parent/history/resource auxiliar, mutex/alocadores,
reset de cache por frame, dispatcher/desenho completo, shader, pesos efetivamente
usados pela GPU, deformação de normais/tangentes e convenção de projeção. Os dados
reais do oracle são payloads MOD com poses offline declaradas, não capturas do
jogo integral. As rotinas constituem preparação de dados nativa verificável;
ainda não estabelecem gameplay ou fidelidade do renderer.
