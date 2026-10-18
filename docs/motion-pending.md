# Consumo de layers pendentes e restauração de joints

`0x9577ec` consome o bit pendente `2` das layers, preserva transforms de
transição, reinicializa os samplers e chama `95aa90` para a ação resolvida.
O ramo sem selector ativo termina em `95c7dc`, que restaura os joints.
As cópias para a tabela de transição acontecem **antes** dessa restauração.
Não se deve representar as duas tabelas como um único conjunto de transforms.

Esta investigação entrega um contrato estático completo dessas duas funções
e um oracle de **561 execuções ARM isoladas**, sem stubs. A cobertura executada
de `9577ec` usa ações ausentes ou slots vazios: `95aa90` executa seu retorno
original para ponteiro nulo. O binding de ações presentes pertence a outro
recorte e não é afirmado como executado neste oracle histórico.

A implementação posterior está em [motion_pending](motion-pending-runtime.md),
com [1.664 casos ARM completos](motion-pending-oracle.md), incluindo ações
presentes. A [restauração nativa](joint-pose-runtime.md) possui mais 931 casos
próprios. Os dados abaixo preservam o escopo da investigação inicial.

## Fonte, evidência e validação

ELF `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Nova sessão REA `1792288837845`, Ghidra12.1.4/ARMv6, heap3GiB, perfil focused.
Retenção em `research/private/motion-pending-v1/`; manifesto, bundle e snapshot
registram hashes e fechamento. Toda leitura completa foi confrontada com
PT_LOAD. O código original não foi alterado.

| Evidência | Conteúdo |
| --- | --- |
| sessão1792274318294/042 | pseudocódigo9577ec, `ev_00c8b523d53eaea5e94d9dc59ef7d326eed60852520e0a554bd0ee7c81a8a39d` |
| sessão1792274318294/045–046 | pseudo/assembly95c7dc; assembly `ev_3f8532be2c30b8ba8c9cbe7cab7d7b8d440cd2d287c39410434a7b301df5b16a` |
| nova005 | bytes9577ec..957f80, `ev_2c209865b45ee4ba80660307182178863a2aaa468aafde50ccc68262165c01fd` |
| nova008 | assembly9577ec, `ev_5654e5f580207d7e562c702401686f4a2ab9b74db6af34680e1cc8446b94e7b0` |
| nova006/007 | bytes95c7dc..95caa4 e tabela de índices e32eb8 `[1,2,0]` |
| sessão1792283675798/019–020/read113 | produtor960b50: alocações e inicialização das tabelas |

O corpo de9577ec é não contíguo: `[9577ec,957ba4)` e
`[957ba8,957f78)`; `957ba4` é literal zero, seguido dos ponteiros globais em
957f78/7c. `95c7dc` executa `[95c7dc,95ca88)` e usa literais até95caa4.
LLVM serve de leitura auxiliar sobre a mesma fonte; dados não foram tratados
como instruções executáveis no harness.

Os readbacks BSS novos009/010 retornaram **0/44 bytes** para10e6370 e
**0/16 bytes** para10e6560, ambos `complete=false`. Isso não demonstra zeros.
O contrato abaixo exige snapshots explícitos T=`10e6370[3]`,
Q=`10e6560[4]`, S=`10e6380[3]`; o oracle usa valores sintéticos deliberadamente
não triviais. A prova anterior de inicializadores é separada de qualquer
alegação de imutabilidade ou valor atual em partida.

## Estruturas e ownership

Defina `O` como modelo, `L(i)=O+240+i*130`, N=`O.word220`,
C=`O.word234`, J=`*(O+224)`, B=`*(O+c00)` e A(i)=`*(L(i)+128)`.
Offsets, strides e máscaras nesta descrição são hexadecimais.

| Estrutura | Campos consumidos/escritos |
| --- | --- |
| J+j*a8 | +48[3] translação de referência; +58[4] Q atual; +68[3] S atual; +78[3] T atual; +74/+84 padding zero |
| B+j*38 | +0 índice de layer ou ffffffff; +4 flags; +8[4] Q; +18[3] T; +24 padding; +28[3] S; +34 padding |
| A(i)+j*34 | channels em+0,+10,+20 (stride10) e word+30 de flags |
| rModel=`*(O+f0)` | +6c aponta matrizes de16float, stride40 |
| L(i) | selector u16+4, flags u16+6, interp+40, restante+44, peso+48 |
| O | +238 flags de cálculo, +23c acumulador de bits; bancos em+c04+4*bank |

960b50 demonstra **alocações separadas**: Nraw*a8 para J, Nraw*34 para cada
A(i), Nraw*38 para B. Escreve em O+220 `Nraw&255`, zera A(i) e B, marca as
layers pendentes e inicializa os índices de B para ffffffff. As matrizes são
referências do recurso rModel; não são aliases da cópia Q/T/S em B.
O caminho pode reaproveitar tabelas quando o recurso muda mantendo a
contagem; não rezerar tudo em toda troca. Alocador e troca completa de
recurso não foram executados neste oracle.

O harness usa arenas distintas. Isso corresponde à separação observada no
produtor, sob alocações válidas, mas não prova comportamento de ponteiros
corrompidos ou arenas sobrepostas. Uma implementação nativa deve ter um dono
para cada tabela, com spans/índices de identidade explícitos. Dentro de cada
channel, ponteiro do track, cursor, tempo acumulado e quarto word pertencem
ao mesmo estado do sampler; não criar uma segunda cópia dentro do pending.

## Plano de9577ec

Para contagens pequenas não negativas verificadas, a primeira busca encontra
`p`, o menor índice de layer cujo flags&2 é não zero. Se não existe, retorna
sem alterar flags de cálculo, joints ou snapshots. Depois encontra `a`, o
menor índice cujo selector é diferente deffff. **p e a podem ser distintos.**
Os loops de contagens com bit alto e contagens gigantes não foram executados;
não generalizar a representação signed/unsigned do decompilador a esses casos.

### Nenhum selector ativo

1. Limpa bit20000000 de O+238. Se rModel é nulo, retorna imediatamente;
   os pending bits permanecem intactos nesse retorno.
2. Procura de C−1 até p a maior layer pendente h. Limpa bit2 em todas as
   layers de h até p, inclusive nas intermediárias que já não o tinham.
3. Para cada joint, copia Q/T/S atuais de J para B, grava os dois paddings
   como +0, índice=h, e conserva em B.flags somente bit20.
4. Faz `O.word23c |= O.word238 & 00ffffff`.
5. Tailcall95c7dc restaura J. B conserva o snapshot **anterior** ao reset.

### Existe selector ativo

Marca bit20000000 em O+238 e percorre i de C−1 até p, em ordem decrescente.
Isso dá importância à ordem das layers e às flags temporárias de B.

Para layer não pendente: se selector!=ffff e os **bits** do peso são
exatamente3f800000, para cada joint cujo B.flags&38==0 acrescenta
`(A(i).flags & 70)>>4` a B.flags. Não chama o binder nessa layer.

Para layer pendente:

1. Limpa bit2 da layer. Calcula `blend` com o teste original:
   `int32(bits(interp)) > 34000000` e `(peso>+0 || i==a)`.
   É comparação signed dos bits de interp, não simples comparação float.
   O literal34000000 é2^-23; o oracle cobre a fronteira e o próximo float.
2. Se blend, antes de apagar os samplers, marca bit8 em B.flags quando
   `(flags&38)==0` e
   `(B.index==i || ((0f & ~(flags&7)) & A(i).flags)!=0)`.
3. Reinicializa somente os campos abaixo; os demais bytes ficam intactos.
4. Resolve ação: selectorffff→nulo; caso contrário bank=(selector>>8)&15,
   slot=selector&255, recurso=`O+c04+4*bank`; se presente, chama cd7540.
   Chama95aa90(O,i,ação), inclusive quando o ponteiro da ação é nulo.
5. Se o restante L+44 após o binder é diferente de +0, acrescenta bit100 às
   flags da layer; não há um else que limpe bit100 nessa função.
6. Se blend, para cada B com `(flags&30)==0` e
   `(flags&8 || ((0f & ~(flags&7)) & A(i).flags)!=0)`, copia Q atual de J.
   Se restante==0, grava apenas índice=ffffffff, conservando T/S anteriores.
   Se restante!=0, marca bit10, copia T/S com padding zero e grava índice=i.

Após o loop: `O.word23c |= O.word238 & 00ffffff`, e para cada B conserva
somente bit20 em flags. As máscaras temporárias07/08/10 não devem vazar para uma
segunda implementação paralela da mesma tabela. A função não reordena
layers por peso, não avança o relógio e não aplica movimento ao modelo.

### Campos reinicializados antes do binder

| Destino relativo a L | Write exato |
| --- | --- |
| +78,+88,+98 (16bytes cada) | snapshotQ, copiado sem normalização |
| +a8,+b8,+c8 (16bytes cada) | snapshotT.xyz e quarto word +0 |
| +d8,+e8 | ponteiro do channel de cena =0 |
| +e4,+f4 | quarto word desses channels =+0 |
| +f8,+fc,+100,+104 | quatro bindings de eventos =0 |
| A(i)+j*34: +0,+10,+20 | ponteiros de três channels =0 |
| A(i)+j*34: +c,+1c,+2c | quarto word de cada channel =+0 |
| A(i)+j*34: +30 | flags=0 |

Os campos internos+4/+8 de cada channel **não** são apagados aqui. O binding
posterior porae923c os inicializa quando houver track. Também não há write
aos masks de eventos em L+108..124 neste reset. Generalizar para memset de
16bytes por channel ou zerar masks alteraria a execução observada.

## Contrato de95c7dc

Se `O.flags238 & 10000000`, cada J recebe T/Q/S dos snapshots globais;
Q mantém seus quatro componentes, T/S têm padding +0. Nenhum quaternion é
normalizado e não há fallback inferido.

Caso contrário, T vem de J+48, S vem do snapshot global, e Q é extraído da
matriz `rModel+6c+j*40`. Mesmo com N=0, esse ramo lê rModel e o ponteiro+6c
antes de retornar. Não transformar isso em uma exigência silenciosamente
dispensada ao declarar cobertura de acessos originais.

Seja m[0..15] a ordem dos floats **na memória**, sem impor convenção glTF.
Cada add/sub/mul/div/sqrt abaixo arredonda para binary32, FPSCR0. A soma
trace é `f32(f32(m0+m5)+m10)`. Para trace>0:

```text
r = sqrt_f32(f32(trace+1)); f = div_f32(0.5,r)
Q = [(m6-m9)*f, (m8-m2)*f, (m1-m4)*f, r*0.5]
```

Para trace<=0: i=1 se m0<m5, senão0; troca i por2 se m[5*i]<m10.
Seja j=next[i], k=next[j], next=[1,2,0]:

```text
r = sqrt_f32(f32(f32(f32(m[5*i]-m[5*j])-m[5*k])+1))
f = div_f32(0.5,r)
Q[i] = r*0.5
Q.w = (m[4*j+k]-m[4*k+j])*f
Q[j] = (m[4*i+j]+m[4*j+i])*f
Q[k] = (m[4*i+k]+m[4*k+i])*f
```

Subtrações/somas antes do produto também são binary32. Não há normalização
ou correção de matriz degenerada. O ensaio usa matrizes de rotação válidas;
NaN, infinito, raiz inválida e divisão por zero permanecem fora da cobertura
executada e devem ter domínio explícito antes de integração nativa.

## Oracle e próximos passos

`verify_arm.py` executa bytes originais em Unicorn2.1.4/ARM11MPCORE, com
leituras inicializadas, regiões graváveis, allowlist de código e retorno
real. Hooks apenas observam e rejeitam escapes. T/Q/S são entradas
sintéticas declaradas; não executamos um inicializador de BSS nesta rodada.
Modelo Python separado calcula o resultado esperado e compara **todos os
bytes** de sete arenas, incluindo bytes desconhecidos que devem permanecer.

`arm-final/arm-validation.json` tem SHA-256
`b440299218be5e761831445e7c33c1b3068f3284c535d15d441bdaa06c75da96`:
561 casos, 233.721 instruções, 62.517 leituras, 57.646 escritas, zero
divergências bit a bit. Foram848 entradas reais no retorno nulo de95aa90 e
320 entradas no gettercd7540. Cobre contagens0..3, flags temporárias0..63,
amostras dos flags de samplers, índices de layer distintos, gates de peso,
restante e interp, duas rotas de reset e seis matrizes. Três controles
negativos omitem count, templateT ou ponteiro de samplers e falham no acesso.
O protótipo513casos fica preservado em `arm-first/`, não é o oracle final.

Reprodução, em destino novo:

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/motion-pending-v1/verify_arm.py --output NOVO_DESTINO
```

A implementação nativa posterior reutiliza os donos de layer/canais/recursos,
mantém B separado de J e valida comprimentos e identidades antes de publicar
o resultado. Ela compõe o initializer completo entre as duas fases de snapshot,
incluindo o ramo de ação presente. Ausência explícita continua distinta de
identidade desconhecida. A próxima fronteira é a [pose e mistura de
transições](joint-update.md); a ligação com gameplay e scheduler continua pendente.
