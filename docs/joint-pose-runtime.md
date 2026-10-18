# Restauração dos joints correntes

`joint_pose::restore` implementa `0x95c7dc` até o retorno. É o dono dos
transforms correntes J usados pelo consumidor de pending layers. Os snapshots
B de transição permanecem separados: `9577ec` pode copiá-los **antes** de
restaurar J. Esta função não escolhe ações, não amostra curvas, não altera
bindings/relógios e não demonstra uma pose de gameplay.

## Contrato e dados desconhecidos

`Joint` conserva exatamente os fatos necessários, como bits binary32:

| Campo | Offset original em J, stride a8 | Efeito |
| --- | --- | --- |
| reference_translation: 3 words | +48 | Preservado; origem de T no ramo de matriz |
| rotation: 4 words | +58 | Quaternion corrente |
| scale: 4 words | +68 | XYZ e padding +74 |
| translation: 4 words | +78 | XYZ e padding +84 |

A representação C++ não pretende ter o layout binário do objeto original.
Os bytes não representados de J ficam com seu dono; o oracle confirma que
`95c7dc` não os modifica. Os paddings representados recebem os bits de +0.
Não se deve acrescentar outra cópia desses transforms ao consumidor pending.

`ModelReference` distingue `Unknown`, `Null` e `PresentModel`. Um modelo
presente contém `MatrixTable`, novamente distinta entre `Unknown`, `Null`
e span de matrizes. Cada `Matrix` tem 16 words na **ordem de memória fonte**;
a API não presume a convenção de um exporter glTF. A view é emprestada,
deve sobreviver à chamada e não é retida no resultado. O chamador fornece
fatos de um recurso verificado e garante storage original de J, recurso e
templates possuído e disjunto. Aliases, pointer overflow e corrupção de
endereços ARM não fazem parte do contrato.

Com `flags238&10000000`, cada joint recebe os templates explícitos T/Q/S.
Q é copiado por bits, T/S recebem W+0. Modelo e tabela não são consumidos.
Com zero joints, esse ramo também não consome templates nem ambiente numérico.

Sem esse bit, o original lê rModel e `rModel+6c` **mesmo com zero joints**:

- Modelo desconhecido ou nulo gera erro de integração.
- Tabela desconhecida gera erro, inclusive para zero joints.
- Ponteiro de tabela explicitamente nulo é permitido somente para zero joints.
- Tabela presente vazia continua distinta de tabela nula; ambas permitem N0.
- Para N>0, o span precisa cobrir todos os joints. Só o template S é exigido;
  T vem da referência de cada joint e Q da matriz correspondente.

Os templates opcionais representam conhecimento, não defaults. Eles são T em
`10e6370`, Q em `10e6560` e S em `10e6380`. Leituras REA incompletas da BSS não
forneceram valores atuais. A prova separada de inicialização está em
[root-motion-defaults.md](root-motion-defaults.md); não prova imutabilidade.

## Extração numérica

A função usa o trace `f32(f32(m0+m5)+m10)`. Com trace positivo, calcula
`r=sqrt_f32(f32(trace+1))` e `f=div_f32(0.5,r)`, produzindo:

```text
Q = [(m6-m9)*f, (m8-m2)*f, (m1-m4)*f, r*0.5]
```

No outro ramo, escolhe i=1 se m0<m5, senão 0, e troca por 2 se m[5*i]<m10.
Com `next=[1,2,0]`, j=next[i], k=next[j]:

```text
r = sqrt_f32(f32(f32(f32(m[5*i]-m[5*j])-m[5*k])+1))
f = div_f32(0.5,r)
Q[i] = r*0.5
Q.w = (m[4*j+k]-m[4*k+j])*f
Q[j] = (m[4*i+j]+m[4*j+i])*f
Q[k] = (m[4*i+k]+m[4*k+i])*f
```

Cada soma, subtração, produto, divisão e raiz arredonda separadamente para
binary32. Não usa o caminho double de `root_motion`, não normaliza o resultado
e não repara nem exige ortogonalidade da matriz. O domínio admite os nove
componentes consumidos finitos, desde que todos os intermediários/resultados
sejam finitos. Campos não lidos da matriz e referências/templates apenas
copiados podem conter quaisquer bits, inclusive NaN e zero com sinal.

A aritmética exige nearest-even e gradual underflow. Uma probe com operandos
volatile detecta FZ/FTZ/DAZ, sem alterar configurações do processo. Cópias e N0
não exigem ambiente que não consomem. O domínio exclui modos alternativos,
FPSCR exception flags/traps, operandos aritméticos não finitos e overflow;
`-ffast-math` é rejeitado na compilação. Raiz negativa/divisor zero têm erro
explícito, sem fallback, embora não ocorram no corpus finito admitido.

A API retorna `Restored{joints}` ou erro tipado com índice do joint quando
aplicável. Os limites são verificados antes da alocação; o default de 256 joints
é política de recursos, não um máximo do formato. A contagem também precisa
caber no u32 original. Erros tardios e falhas de alocação/capacidade não
publicam estado parcial nem alteram o span de entrada.

## Evidência e execução

Alvo fixado: ELF SHA256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
REA sessão 1792288837845/006 contém 712 bytes de 95c7dc e literais,
Evidence `ev_e02221a550af0226286d9dc7e5a394d4935cd302caa56e2900009cbf5da7ce4c`;
/007 contém a tabela next de 12 bytes,
Evidence `ev_d1dfaa4982185167eed4690dfc7e7e811e0facc4846d6860d06b3651cd3313f8`.
Todos os 724 bytes coincidem com o ELF. As leituras 009/010 de BSS retornam 0 bytes
incompletos e estão preservadas como tal.

O novo oracle executou **931 casos** completos em Unicorn 2.1.4/ARM11MPCore,
FPSCR 0, com as instruções originais, incluindo VSQRT. Não há helper externo
substituído, stub ou patch. Ele reaproveita os 48 casos de reset anteriores,
reconstruindo o J original pelo seed/inicialização e comparando 648 words
alterados do relatório fixado. Os demais incluem N0 e contagens até 256,
ponteiros nulos/desconhecidos não consumidos, fronteiras do trace e seleção
de diagonal, matrizes não ortogonais, magnitudes binary32 diversas, 160 novas
matrizes de rotação e templates com bits arbitrários. Matriz e estado de
instância são entradas sintéticas declaradas, não uma captura do rModel do
caçador em partida.

Leituras precisam estar inicializadas; writes são permitidos somente em
J+58..87 de cada registro e nos 60 bytes de stack usados. O harness verifica
retorno, SP/callee-saved, imutabilidade do modelo/recurso/matrizes/templates
e preservação dos outros bytes de J. Observa explicitamente as leituras de
rModel+6c em N0. Onze controles negativos cobrem fatos ausentes, ponteiros
nulos com acesso, write guard, limite de instruções e execução de literal.
O maior caso executou 14.864 instruções, abaixo do teto de 100.000.

O C++ passou **89.439 verificações em cada build**, normal e ASan+UBSan,
comparando Q/T/S/paddings e referência por bits. Testes públicos exercitam
tri-state, capacidade, desconhecidos, não finitos consumidos/não consumidos,
erro numérico no segundo joint sem mutação, ambiente de arredondamento e
FPCR.FZ real com restauração RAII. Uma alteração proposital no output do
oracle foi detectada. O teste de `-ffast-math` falhou como esperado.
Não foram injetadas falhas globais de allocator; os catches foram revisados
e os limites de capacidade são testados pela API.

Artefatos em `research/private/joint-pose-v1/`: `arm-final/` conserva casos,
pins e relatório; `native-validation.json` contém argv, métricas, hashes
pré/pós e resultados finais; `source-readback.json` registra proveniência;
`pre-capacity-tagging/` preserva o ensaio anterior ao ajuste de erros de
alocação. O manifesto fixa fontes, documentos e evidências. A execução local
é macOS arm64; integração/Windows ficam com o build central. Não se afirma
execução em hardware 3DS, scheduler, gameplay ou equivalência universal IEEE.

```sh
research/private/root-motion-runtime/venv/bin/python research/private/joint-pose-v1/verify_arm.py --output NOVO_DIRETORIO
# O teste C++ aceita: joint-pose-tests [oracle.bin]
```

O custo do oracle e dos binários está nos relatórios; não houve medição de
tokens, memória da família inteira de processos ou comparação de performance
com um workflow anterior equivalente. Nenhuma janela persistente foi alterada.
