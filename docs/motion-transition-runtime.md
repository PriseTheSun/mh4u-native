# Mistura nativa das transições

`motion_transition::apply`, em `src/motion_transition.hpp/.cpp`, reproduz
`95b490` completo, incluindo a construção da matriz RT em `6d385c`. Recebe
as poses e os snapshots já produzidos pela atualização dos ossos e pelo
[consumo das camadas pendentes](motion-pending-runtime.md). Ainda não liga
essa sequência ao relógio ou ao personagem do laboratório.

## Estado e ordem

As views emprestam `motion_request::Layer`; poses, metadados e snapshots
usam `joint_pose::Joint`, `joint_update::Metadata` e
`motion_pending::Snapshot`. Não há nova interpretação de LMT, canais,
relógios ou recursos. O resultado devolve somente joints atualizados, RT
local e flags acumuladas `O+23c`, além de contagens diagnósticas.

Primeiro, a função constrói RT a partir da rotação e translação do modelo,
mesmo com zero layers ou joints. A rotação não é normalizada e a escala
não participa desta matriz. Os três words de translação são copiados crus;
a aritmética finita se aplica aos componentes de Q efetivamente consumidos.
`motion_transform_math::rt_matrix` é o mesmo dono usado na saída hierárquica.

A busca examina `layer.flags & 100`, o halfword em `L+6`. Sem nenhuma camada
com esse bit, zera as flags acumuladas e retorna os joints intactos. Se
encontrar uma, conserva as flags acumuladas, inclusive quando não há joints
ou todos são ignorados. O selector, atributos e flags de desligamento das
camadas não filtram esta operação.

Para cada joint, em ordem crescente:

1. Lê o owner do snapshot; `ffffffff` ignora o joint.
2. Lê `J+94`; qualquer bit de `41` ignora o joint.
3. Usa o owner para escolher `layer.field60`, mesmo que essa camada não
   tenha o bit `100`. Tanto +0 como −0 ignoram toda a mistura.
4. Mistura Q, depois T e S. Q ajusta o hemisfério do snapshot e normaliza
   o resultado. T/S recebem `current*(1-t) + snapshot*t`, com padding +0.

O peso não é limitado a `[0,1]`. Snapshots, metadados, campos de layer e
translação de referência dos joints permanecem intactos. Quando a mistura
é ignorada, os componentes e paddings preservam seus bits, inclusive valores
não finitos. Snapshot.flags e os paddings dos snapshots não são consumidos.

## Erros e domínio

Os limites padrão de oito layers e 256 joints são políticas ajustáveis de
trabalho e alocação. Não são máximos de formato inferidos. A quantidade deve
caber no contador original u32. O resultado é privado até o sucesso; uma
falha depois de outros joints já calculados não altera o estado emprestado.

Ausência de snapshot, metadata ou layer selecionada é erro quando esse
dado é lido. Assim, um owner fora da faixa não bloqueia um joint ignorado
por flags, e um owner sentinel não exige metadata. Sem bit `100`, a rotina
não exige os arrays de snapshots e metadata.

A matemática compartilhada exige nearest-even, gradual underflow e
operandos/intermediários aritméticos finitos, com arredondamento binary32
separado para cada operação. Norma zero é rejeitada explicitamente; não se
fabrica um quaternion identidade. O erro retém joint, layer, componente e
diagnóstico do dono da aritmética. Flags acumuladas de exceção FPU não fazem
parte do estado modelado.

## Evidência e verificação

O contrato vem da [investigação de saída dos joints](joint-output.md),
com assembly REA `ev_ebaeb4dcc5489df60c06035691e99f07c01a80754c3fb4281222ab71bbc24cfa`
e bytes conferidos com o ELF privado
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
O oracle composto registra o estado completo antes, após `95b490` e após
`95b940`; a comparação nativa e seus limites ficam em
[joint-output-oracle.md](joint-output-oracle.md).

As verificações da API cobrem leitura condicional, pesos extrapolados,
hemisfério, bits crus, paddings, limites, modo de arredondamento e falhas
tardias sem publicação parcial. Resultados e fontes compiladas ficam em
`research/private/motion-transition-native-v1/`.

O executável próprio passou em 75 verificações, tanto normal como com
ASan/UBSan. A comparação composta cobre 401 casos: 399 cadeias com os dois
estados de saída idênticos ao ARM e duas rejeições explícitas por norma
zero. Os 29.417.472 bytes comparados com o ARM são contabilizados
separadamente dos 14.929.920 bytes de verificações de imutabilidade das
entradas, conforme o relatório do oracle.

A ligação com pending, preparação/clock, sampler e seleção do personagem
ainda requer uma passagem composta comprovada. Matrizes RT locais não são
automaticamente uma paleta de skinning; attachment, hierarquia, relação com
mesh/inverse bind e os callbacks especiais têm seus próprios contratos.
Este módulo não comprova gameplay, execução Windows ou coop jogável.
