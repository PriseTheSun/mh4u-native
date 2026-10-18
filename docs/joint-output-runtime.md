# Saída nativa das articulações

`joint_output::apply`, em `src/joint_output.hpp/.cpp`, implementa a entrada
`95b940` e sua cadeia `1a63fc → 6d3948 → 95de18 → 95f114`, incluindo o
callback normal `9608d8`, o callback null e a reconstrução simples da
hierarquia por `95c020/95b39c`. Recebe a RT local já calculada pela
[transição](motion-transition-runtime.md); não avança relógios, amostra
animações nem presume a composição enviada à GPU.

## Um dono para cada estado

`State::joints` contém `joint_pose::Joint` e `joint_update::Metadata`, sem
reinterpretar os seus campos. A matriz atual `J+8`, depth `J+90`, identidade
do callback `J+88` e relação de owner `J+a0` completam cada registro.
`local_rt` representa `O+70`; `world`, `O+b0`. Elas não são aliases. Escala,
flags, índice/modo do attachment, dirty, ordem e `fieldc4c` também são
entradas explícitas. `fieldc4c` é **u32**: `95b99c` executa `STR`, não `STRB`.

`Parameters::model` reutiliza `joint_pose::ModelReference`. Aqui somente a
presença de `rModel` é testada; `PresentModel{Unknown{}}` é válido, pois a
rotina não lê sua tabela de matrizes. O pai e suas tabelas distinguem
Unknown, Null e presença resolvida. O getter suportado é explicitamente
`uplayer_d02258`; outro getter não é escolhido por aparência ou por nome.
As tabelas do pai são spans emprestadas durante a chamada. O resultado
possui o próprio estado e não conserva esses empréstimos.

Todos os arrays de bits fornecidos são snapshots inicializados do estado
correspondente. Unknown descreve fatos de ponteiros/associações que não
foram resolvidos; não autoriza preencher transformações desconhecidas com
identidade. Os templates globais de 12+4 words são opcionais explícitos,
nunca valores extraídos de BSS incompleta nem defaults presumidos.

A função não modifica as entradas. Retorna `Output` apenas após terminar
todas as operações: estado, indicação de limpar `O+30`, callback selecionado
quando houve traversal e quantidade de matrizes atualizadas. Em caso de
erro, não publica poses, flags, depth ou ordem parcialmente calculados.
Quem integra aplica `attachment_cleared` à identidade canônica do pai;
quando false, essa identidade permanece intacta.

## Ordem e leitura condicional

1. Pai null ou com `(flags4 & 7)` fora de `{1,2}` limpa o attachment e copia
   RT local. Não consulta getter, mapa ou joints desse pai.
2. Pai válido seleciona sua matriz world se o índice u32 tem o bit de sinal.
   Caso contrário, o getter comprovado reduz o índice a low8 e lê o mapa.
   Byte `ff` seleciona world; outro byte seleciona a matriz de um joint.
3. Multiplica RT local pela matriz selecionada e aplica o modo do halfword
   alto de `O+38`: 0 copia a base global; 1 normaliza cada linha de quatro
   componentes quando sua norma atinge `2^-23`; 2 retém normas na diagonal;
   outros valores mantêm a base. Esses modos só ocorrem com pai válido.
4. Escala as três primeiras linhas, incluindo a quarta coluna. A última
   linha permanece crua. As comparações com os templates atualizam somente
   `flags104 & c0` e `flags104 & 100`. Igualdade IEEE considera +0 e −0
   iguais, NaN desigual e infinitos de mesmo sinal iguais.
5. `rModel` null ignora toda a hierarquia, ordem, callbacks e dirty. Se
   presente, executa rebuild quando necessário, seleciona callback e visita
   exatamente N índices da ordem fornecida. Por fim seta `flags230 |= 1`.
6. O gate de `95b940` atualiza a word `fieldc4c=2`; no outro ramo preserva
   seus quatro bytes. Os demais campos permanecem intactos.

A seleção segue a precedência original: `08000000` escolhe null;
`04000000` escolhe `95a7bc`; sem `40` escolhe normal; `40|100000` escolhe
normal e seta `flags104|=40`; `40|4000` escolhe `95ba60`, também setando 40;
o restante escolhe `95c24c` e seta 40. Os callbacks alternativos são fatos
conhecidos, mas a implementação rejeita sua **invocação**. Uma seleção
alternativa com zero joints, ou todos ignorados, continua sendo válida.

O traversal ignora `J.flags94 & 20`. Quando normal, constrói RT de Q/T,
aplica escala de J e multiplica pela matriz do pai indicado nos bits 8–15
de flags94. `ff` usa world do modelo. Não normaliza Q, não aplica inverse
bind e não visita pais implicitamente. Na ordem limpa, índices repetidos,
ordem inversa e pais ignorados usam literalmente a matriz então existente.
Com `flags238 & 01000000`, o callback próprio do joint é lido após a função
selecionada, mesmo quando esta é null. Callback próprio desconhecido ou não
nulo é erro se consumido; com joint ignorado ou gate desligado, é preservado.
O parâmetro de opções de `95de18` é fixo em zero por esta entrada completa;
a API não promete outras opções isoladas.

## Rebuild e limites

Quando dirty está ativo, o produtor reseta depth para `ffffffff` e limpa
`mapping98 & 30000`, resolve depths e executa as trocas exatas de todos os
pares `i<j`. Não usa `stable_sort`: em depths `[1,1,0]`, por exemplo, a
ordem resultante é `[2,1,0]`. A ordem é reescrita apenas nos N primeiros
bytes; bytes adicionais são preservados.

O perfil resolvido aceita uma hierarquia acíclica com o mesmo owner. Roots
não leem owner nem callback durante o rebuild. Filhos precisam de owner
self, pai dentro da coleção e callback null; desconhecido, null, outro
owner, callback não suportado e ciclos geram erros distintos. O teste de
callback extra do traversal continua independente dessa regra.

Os limites padrão são 256 joints, bytes de ordem, entradas de mapa e
matrizes do pai. São políticas de trabalho ajustáveis, verificadas antes
da alocação/cópia ou acesso correspondente. A contagem cabe em u32.
Rebuild tem um limite adicional explícito de 256 índices representáveis
sem repetição em u8. Uma ordem limpa maior pode repetir índices u8, se o
chamador ampliar seus limites; não extrapolamos o rebuild original para
esse domínio. Com N=0, a ordem Unknown/Null é preservada: o original carrega o
ponteiro, mas não o dereferencia, usa para decidir um ramo ou publica seu
valor. Esta é uma abstração explícita por uso efetivo, não uma inferência
de null; N>0 exige presença e extensão conhecidas.
A caminhada nativa é iterativa e detecta ciclos, sem
recursão ilimitada. Falhas de alocação/capacidade são erros tipados.

## Matemática compartilhada

`motion_transform_math` é o único dono de `rt_matrix`, `matrix_product`,
`scale_rows` e `attachment_basis`. A transição e esta saída usam a mesma
RT de `6d385c/9608d8`, com Q cru e T copiado. As matrizes são arrays de
16 words na ordem de memória observada; isso não estabelece convenção
glTF, handedness ou unidades.

A aritmética exige nearest-even, gradual underflow e componentes e
intermediários finitos efetivamente consumidos. Cada multiplicação,
adição, subtração, divisão e sqrt estreita a binary32, sem FMA. Um probe
com operandos volatile rejeita FZ/FTZ/DAZ. `__FAST_MATH__` é recusado na
compilação. Copies não consomem esse domínio: T não finito pode atravessar
RT e a última linha de uma escala; passa a ser rejeitado se uma
multiplicação posterior o consumir. Os flags de exceção FPU não são
parte do estado comparado. Não há reparação de matrizes nem quaternions.

## Verificação e proveniência

A investigação histórica permanece intacta em
`research/private/joint-output-v1`, manifesto SHA-256
`bbc9ac86489cfa8bf1975a62d137354fcd81d57de8562394e38acefcb97eb739`.
Ela contém os Evidence IDs e readbacks da [cadeia recuperada](joint-output.md).
Aquele documento histórico descrevia flags de layer como `+4`; o assembly
confirma halfword `+6`, consumido pelo dono `motion_request::Layer::flags`.
A correção está documentada nos consumidores atuais sem alterar o pin antigo.

O oracle independente desta etapa pertence a
`research/private/joint-output-composition-v1` e está descrito em
[joint-output-oracle.md](joint-output-oracle.md). Executa as duas entradas
ARM completas em sequência, com os helpers reais, sem stubs ou alteração
de PC, sobre o ELF privado
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
O corpus preserva 150 casos históricos e amplia para 401: 399 cadeias
comparáveis bit a bit, além de dois casos onde a normalização ARM gera
não finitos e a transição nativa rejeita esse domínio explicitamente.
São três snapshots de sete arenas, incluindo bytes opacos. O resultado
não depende das equações da implementação C++ para gerar suas respostas.

`src/joint_output_tests.cpp` passou **137 checks** tanto no executável
normal como com ASan/UBSan. Cobre também desconhecidos não consumidos,
matrizes de pais anteriores, ordenação discriminante, largura de c4c,
limites, ciclos, erros transacionais, NaN/Inf copiados, limiar inclusivo
da normalização e FZ real de FPCR em macOS arm64. Os comandos, logs, pins
e tempos estão em `research/private/joint-output-native-v1/native-strict`.
O executor exige o stdout esperado, stderr vazio e sanitizers sem recovery.
A primeira rodada limpa permanece preservada em `native-final`; `native-strict`
é a validação canônica, com todos os headers transitivos fixados.
O adapter `src/joint_output_resource_tests.cpp`, de autoria independente,
compara a cadeia completa; seu relatório próprio retém a compilação
normal/sanitizada e o hash da fixture.

Reprodução dos testes próprios, com diretório de saída novo:

```sh
python3 research/private/joint-output-native-v1/validate_native.py \
  --output research/private/joint-output-native-v1/native-replay
```

O manifesto deste módulo fixa fontes, docs, backups da matemática,
relatórios próprios e a referência ao oracle independente. Não substitui
as evidências históricas. Nenhuma janela persistente foi controlada.
Não houve execução de binário Windows por este agente; integração e
cross-build pertencem ao relatório do projeto.

Esta etapa ainda exclui callbacks alternativos e callbacks de joints,
owners externos no rebuild, GPU, inverse bind e integração do scheduler.
A matemática foi demonstrada sobre estados sintéticos explícitos; isto
não demonstra aparência final, cadência do jogo, combate nem coop jogável.
