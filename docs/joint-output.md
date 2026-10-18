# Saída e hierarquia dos joints

O recorte recupera a cadeia `95b490 → 6d385c` e `95b940 → 1a63fc → 6d3948 → 95de18 → 9608d8`. O oracle executou 150 estados sintéticos com as instruções ARM originais, incluindo o getter virtual do pai e a reconstrução limitada da ordem dos joints. As sete arenas resultantes coincidiram byte a byte com equações independentes. Isto fornece um contrato de matrizes e de mudanças de estado; ainda não demonstra a paleta enviada à GPU nem sua composição com inverse bind.

A investigação é privada, sobre o ELF SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Não altera o jogo, o renderer nem módulos nativos. A sessão REA `1792291312711`, Ghidra 12.1.4/ARMv6, heap 3 GiB e análise Decompiler Switch desativada, foi exportada e fechada; o bridge terminou com código 0. O launcher existente permaneceu aberto.

## Evidência e limites de interpretação

`research/private/joint-output-v1/evidence-index.json` liga cada pedido, Evidence ID, hash e origem. `readback.json` compara todos os bytes completos ao ELF. `rea-evidence.json` e `rea-snapshot.json` preservam o conteúdo da sessão. O recorte reutiliza a vtable e os corpos `95b490/95b940` da sessão anterior `1792290219632`, sem nova importação para esses corpos.

| Observação | Evidence principal |
| --- | --- |
| Construção de RT, `6d385c` | `ev_690c1565cce28e8e5733e06cd11e100718b0d994b6620cfc2474a49888fcab14`; bytes `ev_704dad3dd1d4293797bddd6e5a190d8e685d35d91b0babe94a502d84f7dd4794` |
| Attachment e modos de base, `6d3948` | `ev_24fcfda43f1eb32fda8ea7fbc20e3ae9e200abead0dd37196d07150c4408be0c`; bytes `ev_9942495d05d263b47b31f223f59dd467d0b9c53b195518c2024192b0985849c8` + suplemento `ev_e56454122bd8d495e598bd3111c63ebbc8e4110ef753626deb2a2b7bb939a52b` |
| Comparações de transformação, `1a63fc` | `ev_78aa7280e4555d16ecf6abf920f83fb4221274e22674b14e39c29b14d3ea6ac3` |
| Traversal `95de18` e seletor `95f114` | `ev_5b8a1c0501026b4a3d6dc593d9f360584dd51d1cae7332caaf7eda4edebbc048`; `ev_afce377e50026678ba630c25cfe2a14e1b0d2a335759440de53a9e521e2bb15e` |
| Transformação normal `9608d8` | `ev_588d5c85581ad4dfbe90ef9e63b449f8df6c4f4c8ff9e1e6edd3b8980e3b9004`; bytes `ev_794e1f507d9b04616d8627ea59997224a235f11352486eed97e3d69f0536bb27` |
| Depth `95c020` e ordenação `95b39c` | `ev_dbe52be0f2e89548b4b604d22de6a1064639f384ebb59ff292d9a309964c10ec`; `ev_7c6b94bb2ac1d293c774750e3b2102b17d5b81f7041395f863179d07b01dee2d` |

Ghidra não reconheceu funções contendo `d02258`, `95a7bc`, `95ba60` e `95c24c`. Isto permanece resultado desconhecido do provedor; os readbacks completos e o disassembly LLVM derivado do mesmo ELF permitem examinar os intervalos. A execução do getter `d02258` usa seu corpo de 64 bytes e a entrada real `uPlayer.vtable+0x54 = d02258`; não fabrica o retorno da virtual. Os outros três callbacks têm apenas delimitação estática neste recorte.

Todos os offsets e endereços abaixo estão em hexadecimal. Nomes como RT, matriz e depth são descrições das operações observadas, não tipos recuperados do código fonte. As linhas de matriz descrevem a ordem de memória de 16 floats; não atribuem automaticamente convenção glTF, handedness ou unidade física.

## Dados e ordem observados

| Campo | Uso observado |
| --- | --- |
| `O+220`, `O+224` | Quantidade e array de joints, stride `a8` |
| `O+22c` | Array de índices u8 que determina a visita |
| `O+c00` | Snapshots, stride `38` |
| `O+240+i*130` | Camadas; flags em `+4`, parâmetro de mistura em `+60` |
| `O+40`, `O+50`, `O+60` | Translação xyz, quaternion xyzw e escala xyz do modelo |
| `O+70`, `O+b0` | RT local e transformação após attachment/escala |
| `J+58`, `J+68`, `J+78` | Quaternion xyzw, escala xyz e translação xyz atuais |
| `J+8..47` | Matriz de saída, 16 floats |
| `J+94` | Flags; bits 8–15 selecionam pai, `ff` usa matriz do modelo |
| `J+90`, `J+98`, `J+a0` | Depth, flags auxiliares e owner utilizados pelo rebuild |
| `J+88` | Callback de joint; somente null no perfil executado |

O chamador do recorte anterior ordena as duas virtuais. Aqui são chamadas diretamente, em sequência, com a mesma memória: `95b490` primeiro, `95b940` depois. Não executamos o scheduler, relógio, pending, sampler, input ou controlador nesta validação.

### Mistura e RT: `95b490`, `6d385c`

`95b490` chama `6d385c` antes de examinar camadas. Se nenhuma camada tem flag `100`, grava zero em `O+23c` e retorna. Havendo uma, cada snapshot com owner diferente de `ffffffff` seleciona a camada correspondente. O joint é ignorado quando `J+94 & 41` ou quando `layer+60 == 0`.

Para os restantes, o parâmetro `t = layer+60` não é limitado a `[0,1]`. Translação e escala recebem `current*(1-t) + snapshot*t`, com padding escrito zero. Quaternion usa a mesma combinação, negando o snapshot se o dot for negativo, e normaliza somente o resultado. Snapshot permanece intacto. Dot e soma das normas usam a ordem x, y, z, w; multiplicação e adição estreitam separadamente para float32. Valores de quaternion não unitários de entrada são preservados quando a mistura é ignorada.

`6d385c` não normaliza quaternion nem aplica escala. Para `Q=(x,y,z,w)`, calcula `x2=x*2`, `y2=y*2`, `z2=z*2` e escreve:

```text
[1-(y*y2+z*z2), x*y2+w*z2,     x*z2-w*y2,     0]
[x*y2-w*z2,     1-(x*x2+z*z2), y*z2+w*x2,     0]
[x*z2+w*y2,     y*z2-w*x2,     1-(x*x2+y*y2), 0]
[T.x,           T.y,           T.z,           1]
```

Cada operação é float32 na ordem indicada; não usa FMA. O produto posterior `A*B` significa dot de linha de A com coluna de B, ordem `k=0,1,2,3`, multiplicação e adição float32 separadas.

### Attachment: `6d3948`, `1a63fc`

Se `O+30` é null ou seu objeto tem `(word+4)&7` fora de `{1,2}`, o código zera `O+30` e copia `O+70` para `O+b0`. Caso contrário, seleciona a matriz do pai. `O+34` negativo usa `parent+b0`; não negativo chama sua virtual `+54`.

No getter real de uPlayer, índice negativo devolve `parent+b0`. Índice não negativo lê `parent.map[low8(index)]`, usando o ponteiro em `parent+228`. Byte `ff` devolve `parent+b0`; outro valor seleciona `parent.joints + byte*a8 + 8`. A redução a low8 é literal, incluindo o caso 257. Não acrescentamos bounds ao comportamento original; as arenas de teste fornecem índices válidos.

Após `world = localRT * parentMatrix`, `O+38 >> 16` controla a base:

- `0`: substitui os primeiros 12 floats pelos templates globais `10e6300/310/320`.
- `1`: para cada uma das três primeiras linhas de quatro componentes, calcula norma float32. Normaliza a linha quando os bits assinados da norma são pelo menos `34000000` (`2^-23`); abaixo disso a mantém. O corpus é finito, portanto não generaliza a comparação assinada a NaNs.
- `2`: substitui cada linha por sua norma na posição diagonal e zeros nas outras posições.
- Demais valores: mantém a base composta.

Esses modos só ocorrem no ramo de pai válido. Depois, em todos os ramos, multiplica cada uma das três primeiras linhas, incluindo a quarta componente, pela respectiva escala do modelo. A última linha não recebe essa escala.

`1a63fc` compara por igualdade float os primeiros 12 floats da matriz com `10e6300..632f`, limpando bits `c0` de `O+104` se iguais ou setando-os se diferentes. Faz o mesmo com a última linha versus `10e6330..633f` para o bit `100`. Os demais bits são preservados. Os 64 bytes desses globais são parâmetros sintéticos explícitos do oracle, inclusive casos não identidade; não se infere o conteúdo da BSS.

### Traversal e matriz de joint

`95b940` chama `1a63fc`. Se `O+f0` é não nulo, chama `95de18(O, order, N, 0)` e seta `O+230 |= 1`. O recurso é apenas testado por presença nesse caminho. Ao final escreve `O+c4c=2` se `(O+4)&7 != 2` ou se `(~((low16(O+4))>>10) & 12) != 0`; aqui `12` é hexadecimal.

`95de18` obtém primeiro o callback de `95f114`, depois visita os índices na ordem fornecida. Ignora joints com `J+94 & 20`. Quando seu quarto argumento tem bit 1, exige também `J+98 & 10000`; `95b940` fornece zero. Invoca o callback escolhido se não null. Existe ainda chamada do callback em `J+88`, virtual `+18`, condicionada por opções e `O+238 & 01000000`; esse callback permanece null no perfil executado.

O callback normal `9608d8` constrói TRS local com as equações acima e escala as primeiras três linhas antes de multiplicar pelo pai. Pai `ff` usa `O+b0`; outro valor usa a matriz em `O.joints[parent]+8`. O resultado é gravado em `J+8`. Não normaliza Q, não lê inverse bind e não reordena joints. Quando o pai é ignorado por flag `20`, sua matriz prévia é usada. A ordem fornecida é consumida literalmente quando não há rebuild.

### Seleção e rebuild em `95f114`

A tabela `e32e30..e32e57` contém cinco pares de função/ajuste: `(0,0)`, `(95a7bc,0)`, `(9608d8,0)`, `(95ba60,0)`, `(95c24c,0)`. A precedência em `O+238` é:

| Predicado, avaliado em ordem | Callback |
| --- | --- |
| `08000000` | null |
| `04000000` | `95a7bc` |
| sem `40` | `9608d8` |
| `40` e `100000` | `9608d8`, seta `O+104 |= 40` |
| `40` e `4000` | `95ba60`, seta `O+104 |= 40` |
| restante | `95c24c`, seta `O+104 |= 40` |

A seleção dos callbacks alternativos está derivada da fonte; o oracle positivo executa somente `9608d8`. Uma guarda negativa rejeita a fuga para `95a7bc`.

Se `O+c49 != 0`, antes da seleção o código reseta `J+90=ffffffff` e limpa `J+98 & 30000` em todos os joints, chama `95c020` para cada um, chama `95b39c` e limpa `O+c49`. No subconjunto executado, `J+88=null`, todos `J+a0=O`, pais válidos e árvore acíclica: depth do root é zero; depth do filho é depth do pai mais um. O helper usa `Ja0.owner+224`, não pressupõe globalmente o mesmo owner. `95b39c` reinicializa a ordem em `0..N-1` e compara todos os pares `i<j`, trocando índices quando o depth do primeiro é maior, unsigned. Não substituímos essa operação por uma ordenação estável abstrata.

Callbacks de joint não nulos em `95c020` levam a `95881c`, virtual `+14`, caminhada de tipos, `95e5e8` e coleções ligadas. Esses ramos não foram executados nem implementados. Um ciclo do perfil simples chega à guarda de stack; isto não prova que o jogo valida ciclos.

## Oracle executado e reprodução

O produtor é `research/private/joint-output-v1/verify_output.py`. O snapshot canônico é `arm-delivery/verify_output_snapshot.py`; o relatório `arm-delivery/arm-validation.json` tem SHA-256 `f977a172567beaf8bfea886d9219db2d504451c15ee1321fd26bd3fc123d93c1`. Traces completos de PCs, leituras e escritas estão em `arm-delivery/traces.json.gz`, fixados no relatório. Para repetir, use o Python local com Unicorn 2.1.4 e um diretório de saída novo:

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/joint-output-v1/verify_output.py --output research/private/joint-output-v1/arm-replay
```

A localização do Python deve ser ajustada à instalação local; o script aceita `--output` explícito e recusa criar saída existente. O manifesto retém as fontes e hashes; não exige Ghidra para repetir o oracle. O ELF privado é necessário. Nenhum byte do jogo entra em área pública.

Foram 150 cadeias completas das duas entradas, 126.861 instruções, 37.766 leituras e 24.535 escritas observadas. Sete arenas somam 36.864 bytes por caso, incluindo o complemento opaco, e os 5.529.600 bytes comparados coincidiram exatamente. As equações independentes estreitam cada operação a float32; os originais executam VFP2 não fused. Não é comparação de uma implementação nativa ainda.

O corpus cobre 0/1/2/3/4/8 joints, árvores lineares/reversas/múltiplas raízes, ordem explícita e reconstruída, quaternion identidade/não unitário/zero, escala negativa/não uniforme/zero, parâmetros de mistura de −0,5 a 1,5, hemisfério oposto, flags que ignoram mistura ou saída, ausência de recurso, ausência de camada, pais ausentes/inválidos/válidos, índice −1/0/2/257, map `ff` e modos de attachment 0/1/2/3. As linhas de norma próximas a `2^-23` exercitam o limiar. Não pretende cobrir todos os valores float ou todos os flags.

As seis guardas negativas demonstram falha por template não inicializado, ordem ausente, orçamento, callback de joint fora do perfil, callback alternativo fora do perfil e ciclo atingindo limite da stack. São guardas do harness, não validações atribuídas ao jogo. FPSCR começa zero em cada entrada virtual; flags de exceção não são comparadas. Sem stubs, PC rewrites ou retornos fabricados; o estado e os templates são sintéticos e declarados.

`arm-first/second` preservam perfis anteriores. `arm-third` preserva um diagnóstico do primeiro controle negativo mal escolhido (root sem leitura do callback); o controle final usa um filho e alcança o ramo excluído. `arm-fourth/final` preservam a ampliação; `arm-delivery` só esclarece o comentário de escopo do produtor, com resultados e trace idênticos a `arm-final`. Nenhum snapshot anterior foi substituído.

O run canônico mediu 0,375 s de parede, 0,408 s de CPU e pico RSS do processo Python de 71.565.312 bytes em macOS. Estes custos não incluem importação REA, nem medem pico de família ou tokens. O manifesto registra a importação separadamente. Nenhum ensaio Windows foi feito neste recorte.

## Próxima interface e fronteira

Uma futura biblioteca pode receber explicitamente modelo TRS, matriz/modelo pai resolvido, templates, joints atuais, snapshots, camadas, flags e ordem/depth; devolver as mutações exatas e 16 floats por joint. Deve preservar a distinção entre matriz local RT, matriz attached/scaled do modelo e matriz hierárquica do joint. Callbacks alternativos e callbacks de joint precisam de suporte demonstrado ou rejeição explícita.

A interface de render ainda exige localizar o consumidor de `J+8`, suas associações ao recurso/mesh e eventual produto com inverse bind. O recorte não lê essas matrizes de bind. Não é correto ligar diretamente os 16 floats ao contrato glTF de skinning sem confirmar essa etapa. Também continuam fora deste recorte políticas de gameplay, scheduler, inicialização global completa, produtor das flags especiais e casos de callback não nulo.

## Consultas auxiliares compartilhadas

`d12f8c` foi retido em 140 bytes de código e 16 de literais (`7149f2ca`, `00000000`, `bf800000`, `007fffff`), Evidence `ev_ae0a77ce7ab5ae0267f9f76438ae575b46b5814864e20eb7ab91c2e74951283a`; assembly `ev_903ddd239688d016f4d32adaca357a387b5d22aa8cf5aab91fc668d32b057d37`. É floor por bits, preservando −0 e retornando o padrão original para expoente pelo menos 23, inclusive Inf/NaN. Não chama helpers. A soma com sentinela positiva ocorre nos outros ramos e afeta flags; a lógica de bits determina o resultado no domínio finito observado. A execução desse helper pertence ao oracle separado de `motion_query`, não às 150 cadeias aqui.

As duas tabelas de índice em `e3305c..e33073` são `[1,2,0,1,2,0]`, readback `ev_a6fd0ea4ebd563aabe12f5b019899a73f69c038cda17cce72d4c2cddf5895ef4`. Foram entregues ao dono de `joint_update`; não fazem parte da aritmética da saída normal testada aqui.
