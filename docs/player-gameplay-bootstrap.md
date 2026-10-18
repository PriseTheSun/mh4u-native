# Entrada inicial do caçador: estado, contexto e curvas

O primeiro pedido explícito observado em `b312dc` chama `b3fc50` com **pl_st=0**, flags `2` e modo `1`. Ele não estabelece `pl_st=1/selector8` como início do gameplay. O selector ainda depende de campos e do contexto carregado. A investigação também recuperou e executou o inicializador completo das três curvas globais: duas delas têm valores pós-inicialização diferentes dos zeros presentes no ELF.

Esta entrega usa somente o ELF fixado SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`, bytes REA e recursos locais catalogados. Não modifica o runtime nem executa o jogo completo. Artefatos ficam em `research/private/player-gameplay-bootstrap-v1/`; a fonte da ROM permanece privada e inalterada.

## Curvas globais com inicialização comprovada

`d3f6fc..d3f850` é uma função ARM completa, sem calls. Seus literais ocupam `d3f850..d3f87c`. O trecho `d3f760..d3f77c` escreve exatamente:

| Endereço | Palavras binary32, em ordem | Valores aproximados |
| --- | --- | --- |
| `104e048` | `00000000 3f800000` | 0; 1 |
| `104e050` | `3eaaaa3b 3f2aaa3b` | 0,3333300054; 0,6666600108 |
| `104e058` | `00000000 00000000` | 0; 0 |

Os dois valores intermediários são os literais exatos, **não** divisões recalculadas por três. O snapshot de 24 bytes é `curve-init-complete/curves-post-init.bin`, SHA-256 `283bcb5502bd9e7c2daf2245fa36170f8f1f838accd1ed146ff3ba0f3e03b922`. O snapshot anterior do ELF todo zero continua sendo evidência do arquivo antes desse inicializador.

O inicializador inteiro executou em Unicorn 2.1.4/ARM11MPCore, com três padrões distintos de registradores e RAM anterior: `00`, `a5`, `ff`. Todas as instruções originais foram executadas, sem stubs ou substituições. Guards limitam código, leituras aos literais e escritas aos dois intervalos declarados. Três controles negativos rejeitaram ausência do retorno original, de um literal e do destino da última palavra. `verify_curve_init.py` e `curve-init-complete/validation.json` retêm instruções, acessos, hashes e resultados. Isso não é uma execução de startup completo.

Há ligação estática ao startup: a entrada REL32 `ebf648`, ordinal 500 de 954, tem deslocamento −1572684 e resolve para `d3f6fc`. Ela precede `ebf64c → d1d458`, inicializador dos templates já demonstrado. O dispatcher `1013b8`, chamado por `100010`, e sua interpretação REL32 reutilizam `root-motion-defaults/source-selection.json`. A ordem na tabela não foi convertida em afirmação de execução integral do processo.

O scan de palavras alinhadas em todos os segmentos file-backed encontrou referências literais a `104e048` em `b050b0/d3f86c`, a `104e050` em `3dc52c/3de298/b050a4/ce71d4` e a `104e058` em `9607f8/b050ac`. Os consumidores examinados leem as curvas; `d3f6fc` é o escritor recuperado. O segmento é gravável. **Não se provou imutabilidade posterior**: aliases, cálculos de ponteiro e escritas indiretas não são excluídos por esse scan.

## Da construção ao primeiro update

A cadeia de construção anterior é `b6d588 → b0c1d4 → b07770`, seguida pelo reset derivado `b688ec`. O dono de [bootstrap da animação](model-animation-bootstrap.md) mantém a distinção entre oito slots construídos e duas camadas ativas nessa cadeia. Aqui foi fechado mais um componente: `b0c1d4` chama `b0b994` em `b0c2a8`.

`b0b994` inteiro foi executado com os helpers originais `2fef50/2fef54` e `10b778`. O modelo usa o alias real da construção `pbw=model+e38`; memória prévia e registradores foram envenenados com três padrões. O template `10e6440` é entrada explícita de identidade, corroborada pelo inicializador anterior, não execução de `d1d458` neste ensaio. Os três casos executaram 2.028 instruções no total e escreveram os mesmos 1.938 bytes por modelo. Bytes não escritos preservaram o padrão anterior. Três negativos rejeitaram ponteiro, template ou helper ausente. Nenhum allocator ou retorno fabricado foi usado.

| Campo | Efeito de `b0b994` | Limite |
| --- | --- | --- |
| `pbw+354/+358` | `44160000` em ambos; interpretação float 600 | Escrito em `b0bc78/b0bc80`; não é zero implícito |
| `pbw+34c/+34e/+350/+35c` | u16=100 | Sem nome de atributo de gameplay deduzido só pelo valor |
| `pbw+370/+371/+372` | pl_st=0, act_st=0, act_no=0 | Estado do reset, anterior aos carregadores e updates |
| `pbw+4` | word=0 | Inclui byte da etapa de update |
| `pbw+93d4/+93d5` | bytes 0 e 1 | Não torna o modelo pronto por si só |

Resultados e modelos completos estão em `base-reset-complete/`; o produtor é `verify_base_reset.py`. O ensaio não executa `b0c1d4` ou `b6d588` inteiros. O reset derivado retido `b688ec` escreve novamente pl_st/act_st/act_no e readiness como zero; ele também zera `pw+9a8`, `pw+594` e `pbw+6f9`. Não se encontrou ali uma reescrita de `pbw+358`. O estado final após contexto/loaders ainda pode diferir.

O produtor recuperado de readiness é `b07fa4(player,part,resource,...)`: no ramo `part==0`, chama virtual `+58`, cujo alvo de uPlayer é `960b50`, e depois grava `pbw+93d4=1` em `b080b4`. É uma ligação ao setter do recurso do modelo. Esse byte não prova sozinho quais bancos LMT estão vigentes. O ramo possui outros efeitos de materiais e callbacks; não foi executado integralmente aqui.

`b48230` testa readiness e, com `pbw.byte4==0`, chama `b312dc`. Só após o retorno incrementa esse byte e grava `pbw.byte9402=1`. Quando byte4 vale 1, há outro ramo que pode chamar `b1407c`, sujeito ao gate `1a1eb8`. Não foi inferido FPS, scheduler completo nem relação temporal com uma entrada SDL.

## O pedido inicial e o que falta para fixá-lo

Em `b313bc..b313c8`, `b312dc` chama virtual `+118`. Para a vtable de uPlayer `e6bca4`, o alvo é `b431e8`, uma instrução `BX LR`. A chamada seguinte, `b313e8 → b3fc50`, recebe:

```
state=0, interpolation=0, initial_frame=0,
flags=2, mode=1, extra=0
```

O prefixo de `b312dc` já passou por `d04924`, `b09fb8`, `bd9290` e `b0550c` no modelo e em oito submodelos. `bd9290` recebe o manager, o player e `pbw.byte33`; seus efeitos transitivos não foram fechados aqui. Portanto “primeiro pedido explícito” descreve o call site observado, não prova ausência de pedidos ou mudanças de contexto dentro desses helpers.

Isso é argumento recuperado do call site, não valor inventado para um teste. Contudo `b3fc50` decide a ação por campos adicionais. Um subconjunto direto é:

| Condições anteriores na seleção de estado 0 | Pedido |
| --- | --- |
| `pw9a8=0`, `pw7fc!=1`, `pbw6f9!=1`, `pw594=0`, `signed(pbw358)<=43160000` | selector `0203`, transição act_st/act_no `0/0` |
| Mesmos gates, mas `signed(pbw358)>43160000` | Consulta categoria por `6c33d4`; pode pedir `0001`, `0015` ou `021f` conforme categoria e gates adicionais |

Como a construção grava `pbw358=44160000`, ela **não justifica o primeiro ramo** sem uma mudança contextual posterior. O campo é preservado em bits; seu sentido físico não é atribuído aqui. `6c33d4` usa o ID `pbw.byte9`, passa por `69d568` e lê `manager+15c+24*id`, ou retorna zero quando ID>=21. O produtor dessa categoria e o valor final de `pw7fc` antes do primeiro update continuam sendo fronteiras explícitas. Foi localizado outro reset de `pw7fc` em `b6b37c`, mas seus callers `af9440/b260ac` não foram ligados à primeira construção nesta entrega.

A validade do recurso também é condicional ao banco vigente. O [dono dos bancos](player-motion-banks.md) mostra que o loader principal liga banco0 a `co_0` (membro 92608) e banco2 a `co_2` (92610). Nesse contexto, `0001` resolve para co_0/1 (199 frames, loop 0), `0203` para co_2/3 (267 frames, loop 62) e `021f` para co_2/31 (251 frames, loop 68). Em `lco_2`, os slots 3/31 são vazios. `source-facts.json` reutiliza fatos do dono e verifica novamente os hashes dos LMT locais; não reinterpreta flags de cabeçalho como atributos runtime.

A investigação paralela de seleção executou cadeias originais de estado0, inclusive os argumentos exatos acima com selector203/co_2. Isso valida o caminho sob entradas explícitas, não estabelece que essas entradas sejam o estado final do startup. A contagem e o contrato dessas execuções pertencem ao relatório do dono de seleção.

O sufixo de `b312dc` também importa: `b07c08 → b06f8c → b07bb8 → b40234 → b07bb8`, seguido de chamadas de manager. `b06f8c` chama o orquestrador de animação `9573b8` após gates/alterações de flags e frames. Portanto copiar apenas o primeiro pedido não reproduz todo o primeiro update. Não foi executado `b312dc` completo nem removido esse sufixo por stubs.

## Evidência, reprodução e encerramento

Sessão pública REA `1792300233949`: Ghidra 12.1.4, ARMv6, heap de 3 GiB, perfil focused com Decompiler Switch Analysis desabilitado. A primeira resolução demorou 141,679 s; soma dos tempos de todas as chamadas 146,167 s. CPU/pico de memória da família REA e tokens não foram medidos. Os ensaios ARM pequenos registram seus próprios tempos e escopo de medição.

- `017/018/031`: b312dc; `015/016`: no-op virtual+118 e ponteiro da vtable.
- `047/048/059`: b06f8c e vínculo ao avanço da animação.
- `080/081/083/084`: produtor de readiness e virtual+58.
- `085/086/087/088`: reset b0b994, código e literal final completo.
- `067/072`: inicializador das curvas e ligação REL32; Evidence dos 384 bytes `ev_2371ba697abea0e8043ada9c14c30442c19c6e4842fbd58068445b2591c4422b`.

Ghidra não reconheceu `d3f6fc` como função; resolves 063/064 e tentativas 065/066 estão preservados como resultados negativos. A demonstração usa readback exato, LLVM e execução original limitada. Os corpos com switches de `b0d0e4/b17f78/b24354`, solicitados pelo outro investigador, têm limites de análise explícitos; spans completos foram retidos para que esse dono interprete os ramos necessários.

`source-facts.json` audita 31 leituras, 32.960 bytes, todas completas e iguais ao ELF, incluindo três leituras reaproveitadas. O bundle contém 86 Evidence. Exportação 090 e fechamento 091 retornaram sucesso; bridge terminou com código 0 e não restou processo Ghidra da sessão. A tentativa inicial sem PTY `1792300226692` saiu antes de abrir um binário. Nenhum launcher foi manipulado.

Reprodução local, usando diretórios novos:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-gameplay-bootstrap-v1/verify_curve_init.py --output research/private/player-gameplay-bootstrap-v1/recheck-curves
research/private/root-motion-runtime/venv/bin/python -B research/private/player-gameplay-bootstrap-v1/verify_base_reset.py --output research/private/player-gameplay-bootstrap-v1/recheck-reset
```

A próxima fronteira útil é fechar os produtores contextuais de `pw7fc`, categoria `6c33d4` e eventuais mudanças de `pbw358`, além da ordenação entre aquisição dos bancos e readiness. Até lá, o driver deve receber esse contexto explicitamente; o perfil state1/selector8 não deve ser rotulado como bootstrap original.
