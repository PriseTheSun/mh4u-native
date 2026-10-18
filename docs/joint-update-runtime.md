# Atualização nativa dos joints por camadas

`joint_update::apply_layers` executa os contratos completos das duas entradas
finais de `95b790`: `95df38` (rotação/translação) e `95e6ec`
(rotação/translação/escala, incluindo espelho). A implementação cobre várias
layers, mistura normal e aditiva, cópia raw e o atributo de sobrescrita.

O prefixo de `95b790` prepara L64/L68 e chama `95cab0` antes dessas folhas.
Esse prefixo, a aplicação de movimento do modelo, o relógio e a mistura
posterior de snapshots B **não** são executados por este módulo. O chamador
fornece L64 explicitamente preparado; nenhuma conversão para segundos ou
convenção de glTF é introduzida. A investigação da cadeia e as evidências
anteriores estão em [joint-update.md](joint-update.md).

## Um dono por estado

- `joint_pose::Joint` continua sendo o dono dos transforms correntes J.
  A nova API `joint_pose::matrix_rotation` expõe a mesma conversão usada por
  `restore`; seu corpo matemático não foi duplicado nem alterado.
- `Metadata` contém somente os words J94/J98, que as folhas leem sem modificar.
- `LayerView` empresta o `motion_request::Layer`, os `motion_binding::Tracks`
  canônicos e o frame L64. Não cria um segundo relógio, máscara ou binding.
- O resolver tipado existente fornece `motion_sampler::TrackView`. O sampler
  é o único dono das equações de codecs e das mudanças de cursor.
- `motion_transform_math` é o dono compartilhado das operações idênticas
  destas folhas e de `958884`, consumidas também por `motion_query`.

O resultado contém um vetor J e os Tracks na mesma ordem das layers de
entrada. O chamador só publica esses valores após sucesso. Erros preservam
layer, joint, channel, identidade da track e o diagnóstico do dono quando
aplicável. Ausência do resolver não transforma uma track presente em canal
nulo. Views e storage dos recursos precisam sobreviver à chamada; callbacks
pertencem ao chamador e seus efeitos externos não são transacionais.

As folhas originais leem rModel e seu ponteiro de tabela +6c na entrada,
inclusive com zero joints/layers. `ModelReference` conserva Unknown, Null e
Present. Modelo desconhecido/nulo e ponteiro de tabela desconhecido são
rejeitados. Uma tabela explicitamente nula é permitida até que um fallback
de rotação realmente precise de uma matriz. Não se exige uma matriz que o
ramo não consome.

Limites de layers, joints e bindings copiados são conferidos antes da
alocação. Os padrões 8/256/256 são política de recursos, não máximos inferidos
do jogo. A contagem original cabe em u32; o produto máximo de trabalho
`layers*joints*3` também precisa caber em size_t. Contadores usam size_t.
Falhas de alocação e capacidade são erros tipados, sem estado parcial.

## Seleção e ordem das folhas

Ambas percorrem layers em ordem crescente e ignoram selector `ffff`.
Ao contrário da consulta `958884`, essas folhas não pulam uma layer por
flags&2. O modo local começa em 1. Attr80000 acrescenta o bit 2; após cada
layer ativa o modo passa a zero, mesmo quando todos os seus joints foram
pulados.

Para cada joint de destino:

1. J94&1 impede a atualização.
2. Na folha com escala, attr100 e low8(J98) diferente de `ff` selecionam
   o índice de origem para channels, matriz e translação de referência.
   O índice é validado antes de acessar o storage. J94 continua sendo o
   gate do **destino**, e os cursores pertencem ao sampler da origem mapeada.
3. Se modo !=1 e A.flags&0f==0, pula esse joint.
4. Amostra Q e depois T somente quando as respectivas tracks existem.
   Q ausente usa o template global quando O238&10000000 ou attr40000;
   caso contrário usa a matriz da origem. T ausente usa o template no
   mesmo modo global, ou J+48 da origem.
5. A folha com escala amostra S quando presente, ou copia o template S.
   A folha `95df38` nunca consome o channel de escala.
6. No espelho mapeado, inverte os bits de sinal de Tx, Qy e Qz.

As tabelas auxiliares de conversão têm endereços diferentes nas folhas,
mas os três conteúdos observados são `[1,2,0]`. A conversão canônica preserva
ordem, arredondamento e índices; não exige ortogonalidade nem normaliza Q.

Quando modo !=0, a folha copia Q/T/S raw, zerando os paddings T.w/S.w.
Não usa o peso para atenuar essa cópia. Attr80000 numa layer posterior
portanto causa **sobrescrita**, ainda sujeita ao gate A.flags low4. Não é
um peso alternativo de interpolação.

Quando modo ==0, os pesos são o produto binary32 de peso do channel e da
layer. A mistura normal usa T/S `atual*(1-peso)+amostra*peso`, zera os seus
paddings e interpola Q com ajuste de hemisfério e normalização do resultado.
A folha sem escala preserva S nas misturas posteriores.

Attr40000 usa T `atual+amostra*peso`. Forma
`deltaQ=nlerp(templateQ,amostraQ,pesoQ)` e aplica `deltaQ*Q_atual`, sem uma
normalização adicional. Na folha com escala:

```text
S_novo = S_atual * ((S_amostra - S_template) * pesoS + S_template)
```

Cada componente segue essa ordem. As misturas aditivas preservam os quartos
words T/S existentes; não os zeram como a cópia e a mistura normal.

## Domínio numérico compartilhado

`motion_transform_math` usa o produto escalar e a soma dos quadrados em
ordem X,Y,Z,W. Se o produto escalar é negativo, inverte o hemisfério da
amostra; a interpolação é `atual*(1-peso)+amostra*peso`, seguida de
`1/sqrt(norm)` e multiplicação de cada componente pelo recíproco. Não há
clamp de pesos. Norma zero gera erro tipado, sem fabricar identidade.

O produto quaternion segue a ordem original, com delta à esquerda:

```text
x = ((dw*cx + dx*cw) + dy*cz) - dz*cy
y = ((dw*cy + dy*cw) + dz*cx) - dx*cz
z = ((dw*cz + dz*cw) + dx*cy) - dy*cx
w = ((dw*cw - dx*cx) - dy*cy) - dz*cz
```

Cada operação aritmética arredonda separadamente para binary32; não há FMA
nem double intermediário. Operandos e intermediários consumidos devem ser
finitos. Aritmética exige nearest-even e gradual underflow; uma probe com
operandos volatile detecta FZ/FTZ/DAZ sem mudar o ambiente. `-ffast-math` é
rejeitado. Cópias raw e inversões de sinal preservam bits não finitos e
zeros com sinal. Flags/traps de exceção FPU não são estado modelado.

## Evidência e validação

ELF fixado: SHA256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
As folhas, helper d022a0 e literais vêm das leituras REA já retidas em
`joint-update-v1`. A nova leitura da sessão1792291312711/004 retém
`e3305c..e33074`, Evidence
`ev_a6fd0ea4ebd563aabe12f5b019899a73f69c038cda17cce72d4c2cddf5895ef4`.
O novo readback compara os bytes com PT_LOAD do ELF, incluindo as duas
tabelas `[1,2,0]`; não trata memória BSS ausente como zero.

O oracle executou **862 casos completos**, 431 por folha, em
Unicorn2.1.4/ARM11MPCore, FPSCR0. Inclui 618 casos sintéticos e 244 casos
usando todas as **122 ações preenchidas** dos LMT 54913/92608, com registros,
referências, extremes e chaves fonte preservados. Os estados de layers,
pesos, templates, matrizes e mapas bone→joint/espelho são entradas
diagnósticas explícitas; não são uma captura da configuração original em
partida. As curvas reais incluem os codecs comprimidos já admitidos pelo
sampler proprietário.

Código, literais e cursores têm bounds; toda leitura precisa estar
inicializada. As escritas se restringem a J+58..87, frame/key do channel e
stack delimitada. O harness executa os helpers/decoders originais, observa
as chamadas de sampling e verifica retorno, SP/callee-saved, todos os
bytes fora dessas regiões e recursos imutáveis. Nenhum hook substitui
instrução, resultado, retorno ou amostra. Dez controles negativos distintos
cobrem dados não inicializados, chaves reais ausentes, writes, orçamento e
execução de literal. O maior caso executou 63.219 instruções, sob o teto
por chamada de 200.000.

O C++ passou **332.330 verificações em cada build**, normal O2 e ASan+UBSan,
comparando Q/T/S/referência/padding, identidade e cursor por bits. Testes da
API cobrem Unknown/Null, limites, templates faltantes, índice de espelho,
resolução e erro tardio sem mutação, norma zero, não finitos, overflow,
arredondamento e FZ real. O reader de teste mede o arquivo antes de alocar;
um arquivo sparse acima de 256MiB e uma fixture truncada foram rejeitados.
A compilação com fast-math falhou como esperado.

Artefatos em `research/private/joint-update-native-v1/`. `arm-first/`
preserva o primeiro corpus; `arm-before-guard-deduplication/` conserva o
relatório que repetia um controle negativo. O produtor foi corrigido e o
oracle completo repetido em `arm-final/`; os bytes da fixture permaneceram
idênticos. A repetição não é contada como cobertura adicional. O relatório
nativo fixa a fixture por hash, logo suas comparações continuam aplicáveis.
Backups de joint_pose anteriores à API de conversão ficam em
`pre-conversion-api/`; fontes, evidências e revisão têm pins no manifesto.

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/joint-update-native-v1/verify_arm.py --output NOVO_DESTINO
# O teste nativo aceita: joint-update-tests [oracle.bin]
```

Custos de execução e peak RSS do processo estão nos relatórios. Não foram
medidos tokens, pico da família completa ou comparação entre workflows
equivalentes. Esta validação é macOS arm64 e ARM isolado; build Windows e
integração ficam registrados pelo build central. Não demonstra hardware3DS,
combate, scheduler completo, relógio integrado ou coop jogável. Nenhuma
janela persistente foi controlada.
