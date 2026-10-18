# Ordem condicional: pad, aQuest, sPlayer e uPlayer

Este recorte fecha o vínculo que faltava em [player-input-scheduler.md](player-input-scheduler.md):
o objeto de descriptor `10825a4` chama-se **aQuest** e pertence ao gerente de
áreas em `main+2ac`, publicado em `1057290`. Seu virtual `+20=65a8bc` roda
na passagem desse gerente, entre o update de pad e as fases de unidades.
Assim, nos caminhos recuperados e quando os gates abaixo permitem, a ordem
é **pad → aQuest → sPlayer → uPlayer** dentro da mesma invocação de `af4c00`.
Isso é uma conclusão de controle de fluxo estático, não execução de um frame
inteiro nem licença para substituir todos os gates por um loop simplificado.

## Fonte e reprodução

ELF local `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Sessão REA `1792287542293`, Ghidra 12.1.4, ARMv6, heap 3 GiB, perfil focused
sem Decompiler Switch Analysis. Retenção nova em
`research/private/player-input-scheduler-v2/`: `rea-evidence.json`,
`rea-snapshot.json`, `evidence-index.json`, `readback.json` e `manifest.json`.
O manifesto registra exportação/encerramento e hashes dos documentos,
artefatos e dependências anteriores. `retain_evidence.py` confronta todos os
readbacks completos com os bytes PT_LOAD e verifica o fechamento.

`reference_scan.py` reproduz candidatos por palavras alinhadas e branches
ARM B/BL; não é um call graph completo. Disassembly LLVM ARMv6/VFP2 e
assembly REA estão retidos. Sem execução do scheduler, construção integral,
hardware ou callbacks nesta rodada. O snapshot anterior permanece intacto:
manifesto v1 SHA-256
`4156450470b7f6ec117d25684e10d0f0487b7e3275b6d7dcf622246030263a30`.

## Identidade e criação de aQuest

O inicializador `d1e540` passa descriptor `10825a4`, string `aQuest`, base
`10e6914` e tamanho `0x1cc` a `5bf984`; instala a tabela de descriptor
`e7c194` e chama `301df4` com `65ac04` e `0x100000`. São argumentos de
registro observados, não atributos LMT nem flags do modelo em execução.
Não executamos a inicialização completa da árvore de descriptors.

`e7c194+8` contém a factory `cfd0a4`. Essa factory aloca `0x1cc`, chama
`5dd874` e instala no objeto a vtable **e65598**. A base `5dd874` grava
explicitamente `obj.byte14=0` e `obj.word18=0`, além dos demais campos e
helper de construção; esses zeros vêm de writes, não de BSS presumida.

| Slot de e65598 | Destino | Efeito relevante comprovado |
| --- | --- | --- |
| +14 | cfd170 | retorna descriptor10825a4 |
| +18 | 65a8b4 | retorna1 |
| +1c | 65a7f4 | inicialização; recuperada na rodada anterior |
| +20 | 65a8bc | dispatcher que pode alcançar sPlayer |
| +24 | 65ac08 | callback de saída; não aprofundado nesta rodada |
| +28 | 65a314 | retorna descriptor1082258, relação de área pai |
| +2c | 5dd830 | chama virtual+14 e retorna word+4 desse descriptor |

O word retornado por `5dd830` é preservado como identificador numérico do
descriptor. A ligação ao pai de área em `1082258` é diferente da base de tipo
`10e6914`; não confundir as duas hierarquias. A palavra zero em `e65594`
continua sendo anterior à vtable efetiva.

Evidence desta sessão: 017/018/026 (registro de aQuest), 032 (factory na
tabela), 039/040/052 (base), 033/041/042/061 (getters e ready). Factory e
vtable completas reutilizam read110/read106 da sessão1792286478819.

## Gerente de áreas e associação a main

O constructor de main `af4f54` chama `af3d30`, quando a alocação de `0x3840`
tem sucesso, e armazena o retorno em `main+2ac`. `af3d30` chama `642aec` e
instala vtable **e6b7a8**. Seu slot+1c é **6426ac**, e +2c é **642698**.

`642aec` constrói 512 registros de stride`0x1c` a partir de `M+14`, publica
M em `1057290`, zera as contagens/pendência e os oito slots ativos, registra
o descriptor base`10e6914` com nome `AreaRoot` e chama `6424c4` para percorrer
a árvore de tipos. Os campos recuperados são:

| Campo de M | Representação observada |
| --- | --- |
| +14+n*1c | registro de catálogo: descriptor no+8, pai no+c |
| +3814 | quantidade de registros do catálogo |
| +3818 | quantidade da pilha ativa |
| +381c | descriptor solicitado, ou ponteiro nulo |
| +3820+4*i | ponteiro de área ativa, i de0 a7 |

`6423ec` procura descriptor por igualdade de ponteiro; se não encontra,
acrescenta registro e incrementa a contagem. `6424c4` percorre filhos por
descriptor+0c e irmãos por+8. Para descriptor sem bit`0x20000000` em+18,
instancia uma área temporária pelo virtual+8 do descriptor, consulta seus
virtuais+2c e+28, liga o registro ao pai e destrói a instância temporária.
Depois recursa nos filhos. Isso explica como a relação retornada por
`65a314` participa do catálogo; não demonstra quais áreas estavam ativas
numa execução real, nem que o catálogo nunca exceda sua capacidade.

Evidence: 021/022/028 (constructor e publicação), 025/030/031 (derivação e
vtable), 048–051/053 (catálogo). Ligação main+2ac: pseudo040/read043 do v1.

## Solicitação, construção e início do update

Um caminho concreto em `b73564..b73578` carrega M de `1057290`, passa
descriptor`10825a4` e chama seu virtual+2c. `642698` apenas grava esse
argumento em `M+381c`. Os bytes/literais estão em reads034/035/047/054/059;
`b73574` não foi reconhecido como função pelo Ghidra, portanto usamos o
recorte LLVM e não atribuímos um nome ao estado ou caller completo.

Ao final de `6426ac`, se há descriptor pendente e a pilha está vazia ou sua
última área tem byte14 não zero, `642944` aplica a solicitação e depois o
caller zera `M+381c`. Em `642944`, r1 continua contendo o descriptor na
chamada a `6423ec`; o pseudocódigo omite esse argumento, mas o assembly o
preserva. O algoritmo percorre os pais de catálogo, constrói a pilha sem
`AreaRoot`, mantém um prefixo compatível pelos identificadores de tipo e
base, destrói o sufixo antigo com virtual+24/+4 e cria o novo com a factory
virtual+8. Grava cada objeto em `M+3820+4*i` e seu índice em `obj+18`.
Uma solicitação idêntica à pilha inteira força a recriação da última área.

O loop de `6426ac` ocorre **antes** desse processamento de pendência:

1. Percorre i=0..7 e interrompe no primeiro slot nulo.
2. Se `obj.byte14==0`, chama virtual+18; retorno zero interrompe a passagem.
3. Caso ready não zero, recarrega o slot, chama virtual+1c, recarrega o slot
   novamente e grava byte14=1.
4. Recarrega o slot, chama virtual+20 e continua ao índice seguinte.

Assim, um aQuest recém-criado pela pendência não recebe update nessa mesma
passagem. Em uma passagem posterior, seu ready retorna1, mas uma área
anterior da pilha ainda pode bloquear a chegada a ele. Os reloads após
callbacks fazem parte do código; não assumir que os callbacks são no-ops
ou que os ponteiros permanecem invariáveis.

Evidence central: 019/020/027 (`6426ac`), 023/024/029 (`642944`). Assembly do
update: `ev_6204907b807286018c3a66dcd08fc5e055e74ea5fbdbe1dae48cb99b12177621`.
Assembly da aplicação: `ev_12b752c6a29bb9ad27143ca50acbcafa7b61cbc6b7fb22dcf64e71e07b6d1270`.

## Ordem recuperada e seus gates

| Ponto em af4c00 | Ação, omitindo callbacks intermediários |
| --- | --- |
| af4cf4 | main+2b0 virtual+1c → sMhPad694644 |
| af4cfc | 2af838(main+2fc) |
| af4d0c | main+2ac virtual+1c → gerente6426ac |
| dentro6426ac | área ativa virtual+20 → aQuest65a8bc, se alcançada |
| dentro de caminhos de65a8bc | sPlayer virtual+1c → af8ae8, se gates internos permitem |
| af4d54 | se g==0: main+298 virtual+1c → 644e98→644c64 |
| dentro644c64 | unidade permitida virtual+24 → uPlayerb48230 |
| af4d7c | se g==0: fase posterior6450b0→644f84, que também pode chamar+24 |

g é calculado antes do pad por `2b19ec(*(fb6b7c))`, ou zero se esse ponteiro
é nulo. A chamada do gerente de áreas em af4d0c não está sob esse gate g.
Os callbacks entre as linhas são reais e podem alterar estado; esta tabela
preserva ordem relativa e não afirma que possam ser removidos.

O caminho aQuest→sPlayer reutiliza a prova do v1: `obj.byte82` em0..4 com
`obj.byte60==2` alcança `65bb38`, que pode chegar a `2b5a40` diretamente ou
via `659630`; este chama sPlayer em`2b5adc`. Para byte82=5/byte60=2, a rota
`65c80c→2b0dd8` pode chamar sPlayer em`2b0e1c`. Há gates internos adicionais;
não basta atribuir esses dois bytes para garantir a chamada.

`af8ae8` pode produzir os campos de entrada do player antes do dispatcher
que uPlayer alcança em sua fase de unidades. Permanecem os gates de
main+50/+57, flags do grupo, estado2/bit400 da unidade, máscaras de permissão
e gates internos de uPlayer documentados no v1. Não deduzimos que sPlayer
sempre rode, que todo player pertença à lista ativa, que ocorram exatamente
um update por frame, nem que uma chamada omitida deva reutilizar ou zerar
entrada automaticamente.

## Limites e próximo ponto verificável

A ordem relativa dos três elos agora está fechada **nessa rota condicional**.
Falta executar uma passagem composta com estado explicitamente válido,
incluindo a árvore/pilha de áreas e os gates dos callbacks. Um próximo
ensaio pode começar em `6426ac` com slots e pendência declarados, observando
ready/init/update e a passagem de criação; tal ensaio sintético não provará
o estado real de uma sessão do jogo. Para reconstruir política de entrada,
o consumidor deve manter os gates de sPlayer/uPlayer e a identidade de sua
lista, não escolher velocidade, ação, frequência ou ordem de inserção por
conveniência. A relação de relógio `643d68` e o grupo efetivo do caçador
continuam limites independentes.

As consultas auxiliares atendidas nesta sessão retêm os wrappers de motion
command, tabelas/literais e prefixo95aa90/helperae923c/literal95ae38 para
outros módulos. As xrefs examinadas de104e048/104e054 são leituras/cópias;
isso não prova imutabilidade dessas tabelas. Nenhum runtime, CMake, pacote,
launcher ou snapshot anterior foi modificado por este recorte.
