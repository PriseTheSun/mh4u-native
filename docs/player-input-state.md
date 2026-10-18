# Entrada e seleção de estado de uPlayer: recorte estático

Os callers 0xb1d114 e 0xb23b74 não recebem diretamente `act_st`/`act_no`.
Eles encaminham um valor de `pl_st`, dois parâmetros de pedido de animação e
um argumento adicional à rotina 0xb3fc50. Essa rotina escolhe o selector da
animação e os estados numéricos. Foi recuperado também um classificador de
setor angular e magnitude, mas ainda não a produção dos campos a partir do
controle físico nem a política completa que escolhe a próxima ação.

Nenhuma função ARM ou partida foi executada neste recorte. Não foi alterado o
runtime nativo, o launcher ou qualquer pacote. As equações abaixo são derivadas
de instruções e dados do ELF e de respostas REA; não recebem nomes de ações
como andar, correr ou ficar parado.

## Alvo e evidência

O ELF ARMv6 LE é o mesmo de [player-action-controller.md](player-action-controller.md):
SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
A sessão REA/Ghidra 12.1.4 `1792282296342` usou o perfil focused, heap de 3 GiB
e `Decompiler Switch Analysis` desativado. A sessão foi operada pelo agente
responsável pela investigação de bancos; este recorte reutilizou as respostas.

A retenção local está em `research/private/player-input-state/`:

- `rea-records.json`: caminhos, SHA-256, Evidence IDs e identidade do perfil das
  26 respostas utilizadas desta sessão; normalizações de 017–030 em arquivos
  de texto separados.
- `source-readback.json`: **12 intervalos, 2.580 bytes**, todos iguais às regiões
  inicializadas do ELF. `verify_evidence.py` reproduz essa comparação e exige
  o SHA do alvo e um único mapeamento ELF `PT_LOAD` por intervalo.
- `offline-provenance.json`: comandos, hashes do alvo, ferramenta LLVM e cada
  desassemblagem auxiliar. Intervalos podem conter literais ou bytes adjacentes;
  não se afirma que cada linha emitida seja uma instrução executável.
- `numeric-contract.json`: fatos numéricos e lacunas em formato estruturado.
- `arm-test-proposal.json`: 352 casos **propostos e não executados**, descritos
  adiante. `manifest.json` registra os arquivos desta entrega.

Foram reutilizadas a identificação de `uPlayer`, a tabela virtual e a reflexão
retidas na sessão `1792281468783`. O documento anterior explica sua proveniência.
Em particular, o setter 0xb1c050 tem Evidence
`ev_d0ba121f4fe6b39797928a272e1065f194301e2bf3f8169db7ba78e3fab02fb2`
e a reflexão tem
`ev_5b2163b7ffe8988f8aa199ec965f5efa263351a99abbeacc3e773e6ed5f4fa5c`.
Não foi tratado um nome gerado por Ghidra como identificação original de função.

## Fronteiras diferentes: estado geral, ação e pedido

Defina `pbw=u32(player+0xe30)` e `pw=u32(player+0xa29c)`, com acessos little-endian.
Os campos nomeados pela reflexão são `pl_st=u8(pbw+0x370)`,
`act_st=u8(pbw+0x371)` e `act_no=u16(pbw+0x372)`. Esses valores e os selectors
LMT pertencem a domínios distintos.

| Entrada | Encaminhamento observado |
| --- | --- |
| 0xb1d114, 40 bytes | Conserva r0–r3; chama 0xb3fc50 com argumentos de stack `[0,0,argumento5_original]`. O quinto argumento é copiado como 32 bits via VFP. |
| 0xb23b74, 36 bytes | Conserva r0–r3; chama 0xb3fc50 com stack `[0,2,argumento5_original]`. |
| r1 em 0xb3fc50 | Seu byte baixo é escrito em `pl_st`; os testes da rotina comparam o argumento inteiro com 1, 2 e 3. |
| r2/r3 em 0xb3fc50 | Inteiros com sinal, convertidos por `VCVT.F32.S32` para `s0`/`s1`: parâmetro de interpolação e frame inicial de pedido. Não são `act_st`/`act_no`. |
| Primeiro argumento de stack | Encaminhado como flags r3 ao setter 0xb1c050 nos caminhos comuns. |
| Segundo argumento de stack | Modo de pedido usado por 0xb27c1c nos caminhos que o chamam. |
| Terceiro argumento de stack | Participa de um gate específico; não tem significado de gameplay atribuído. |

0xb27c1c preserva `s0`/`s1` e seleciona 0xb0c198 para modo 0,
0xb0c060 para modo 1, ou 0xb0c170 para modo 2. Outros valores retornam sem
pedido. Os dois primeiros wrappers já estão descritos na investigação anterior.
0xb0c170 fixa `s2=1.0` e flags de pedido `0x11`; quando `s0<0`, troca seu sinal
e usa `0x13`, antes de saltar para 0xb07480. Os floats 0 e 1 estão nos literais
0xb0c190/194. Esses são flags do pedido, não atributos do header LMT ou botões.
A confirmação adicional de 0xb27c1c/0xb0c170 é desassemblagem LLVM do ELF
identificado, registrada separadamente das respostas de função do REA.

0xb3fc50 primeiro limpa alguns campos de `pw` e grava `pl_st`. Com argumento
r1 igual a 3, retorna após essa preparação. Um ramo relativamente pequeno,
quando r1 é exatamente 1, tem a seleção seguinte:

| Condição | Selector solicitado | Chamada posterior a 0xb1c050 |
| --- | --- | --- |
| `u8(pw+0x9a8)!=0` | 0x18 | `(act_st=0, act_no=0)` |
| `pw+0x9a8==0` e `u8(pw+0x594)==0` | 8 | `(0,0x18)` |
| `pw+0x9a8==0` e `u8(pw+0x594)!=0` | 8 | `(10,0x0e)` |

Nesses caminhos, a rotina marca `pbw+0x376=1` antes do setter e limpa
`pbw+0x6fa` depois. Isso identifica a seleção estática; executar o pedido ou
setter ainda requer suas dependências reais. Não prova uma transição causada
por um botão ou joystick.

Há também um caminho em 0xb3ff08–0xb3ff60 que termina no setter com
`(0,0x2d,flags=1)`: depende de 0xd04924, 0xd0473c, terceiro argumento de stack
zero, 0x2b1dcc e `u32(pbw+0x3dc)&0x40`. O produtor desse bit e a política dos
gates não foram resolvidos. O bit não foi rotulado como um botão.

## Classificação angular já delimitada

0xb23f94 recebe `(player, id, mode)`. O argumento `id` é comparado com valores
**exatos**, não usado como uma máscara genérica. Valores não reconhecidos
retornam 0, inclusive combinações que não estejam nesta tabela.

| ID | Canal | Limite inferior L | Limite superior U |
| --- | --- | --- | --- |
| 0x400 | 0 | 0xe000 | 0x2000 |
| 0x800 | 0 | 0x6000 | 0xa000 |
| 0x1000 | 0 | 0xa000 | 0xe000 |
| 0x2000 | 0 | 0x2000 | 0x6000 |
| 4 | 1 | 0xe000 | 0x2000 |
| 8 | 1 | 0x6000 | 0xa000 |
| 0x10 | 1 | 0xa000 | 0xe000 |
| 0x20 | 1 | 0x2000 | 0x6000 |

O ângulo `A` está em `u16(pbw+0x3c0+2*canal)` e a magnitude `M` em
`u16(pbw+0x3c4+2*canal)`. Ainda não se demonstrou se canal 0/1 corresponde a
analógico, direcional, outro controle ou entrada transformada. A escala física
da magnitude também é desconhecida.

Para `mode!=1`, o dispatcher chama 0xb2ab7c com um quinto argumento zero.
A função calcula:

```text
D = 0
se ((u8(pbw+0x451)!=0 e (u32(pw+0xf0)&0x4000)==0) ou argumento5!=0):
    D = u16(u32(pw+0xb0) - F_b1061c(player))
resultado = M>=50 e u16(A-D-L)<=u16(U-L)
```

`u16` aqui é redução módulo 65.536, não saturação. Os dois limites são
inclusivos; setores adjacentes aceitam o mesmo ângulo no limite compartilhado.
Uma futura escolha única de direção depende da ordem do caller. A rotina
0xb1061c escolhe um valor de `pw+0x14c` em um ramo, ou usa um objeto global e
0x568b54 no outro; sua interpretação como orientação de câmera não está
comprovada neste recorte.

Para `mode==1`, 0xb24b60 chama a virtual +0x98 com o canal conservado em r1.
Se a virtual retorna zero, retorna zero; caso contrário aplica a mesma
comparação circular com `D=0`. Seu pseudocódigo omite o argumento de canal da
chamada virtual; o assembly mostra que ele é preservado.

## Magnitude e gate virtual

A tabela virtual de `uPlayer` em 0xe6bca4 contém 0xb417a4 no slot +0x98.
Essa função retorna zero se **todos** os testes seguintes forem verdadeiros:
0xd0473c(player,0), 0xb07e80(player) e `u8(pw+0x50d)==1`.
Caso contrário salta para 0xb0a41c, que não chama outra função:

| Magnitude M | Retorno de 0xb0a41c |
| --- | --- |
| 0..49 | 0 |
| 50..109 | 1 |
| 110..65535 | 3 |

A comparação intermediária com 90 não cria outra categoria; abaixo de 110,
o `CMP` condicional com 50 produz a tabela acima. Não há retorno 2 nessa
folha. Nenhuma categoria recebeu nome de movimento.

O slot +0x154 contém 0xb1ba78, cujo corpo é apenas `BX LR`. No caminho de
`act_no=1` descrito anteriormente, a chamada desse slot não demonstra uma
rotina adicional de input para essa tabela virtual concreta. Outras tabelas
ou overrides não foram investigados.

Ghidra representa 0xb417a4 com corpo não contíguo incluindo
0xb0a41c–0xb0a44b e 0xb417a4–0xb417fb. A descrição acima separa o gate e a
folha pelas instruções e pelo salto observado; não pressupõe que a fronteira
de procedimento gerada pelo provedor corresponda à organização original.

## Ensaio ARM proposto, sem gates substituídos

A proposta desta seção foi posteriormente executada e ampliada: o módulo
[player-input-runtime.md](player-input-runtime.md) retém 131.980 execuções
originais e sua comparação com C++. A proposta e os relatórios históricos
deste diretório continuam intactos. A investigação dos produtores e da ordem
de chamadas está em [player-input-production.md](player-input-production.md).

O subconjunto mínimo isolável é a classificação numérica. A proposta privada
contém 320 combinações de oito IDs, oito ângulos próximos dos limites/wrap e
magnitudes 0, 49, 50, 51 e 65535. Usa `mode=0`, `pbw+0x451=0` e argumento
forçado zero, de modo que não chega à chamada externa 0xb1061c. Outros 16
casos verificam IDs não reconhecidos nos dois modos; 16 verificam a folha
0xb0a41c nos limiares 49/50, 89/90 e 109/110 e nos extremos, em ambos canais.

O futuro ensaio deve carregar os bytes originais do ELF identificado, mapear
apenas o objeto sintético, `pbw`/`pw`, stack e sentinela de retorno, limitar
instruções e resetar memória por caso. Deve permitir as escritas de stack
normais, rejeitar escrita nos objetos e falhar se alcançar código fora dos
intervalos declarados. Nenhum gate pode receber uma resposta fabricada.

Esses **352 casos ainda não foram executados**. Mesmo quando executados,
provarão somente a equivalência desses predicados no domínio declarado. Não
provarão o produtor físico de input, chegada ao ramo em uma partida, seleção
de `act_st/act_no`, transição visual ou integração com combate.

## Cobertura e próximo recorte

A pergunta sobre os dois wrappers está respondida; a relação numérica entre
setores, magnitude e gate virtual está delimitada. O controlador completo
continua pendente. Os próximos passos com maior valor são localizar quem
escreve `pbw+0x3c0..0x3c7` e `pbw+0x3dc`, e recuperar um caminho completo entre
o dispatcher de estado e um setter, com ordem dos testes e efeitos dos gates.
Para switches em 0xb12130, é necessário seguir tabela e instruções reais;
o pseudocódigo com blocos removidos não basta para atribuir uma transição.

A associação de bancos de animação é tratada separadamente em
[player-motion-banks.md](player-motion-banks.md). Ela não converte, por si só,
um selector em uma política de controle. Esta investigação não muda a política
diagnóstica de locomoção ou o protocolo de rede existentes.

## Evidências principais desta sessão

| Respostas | Evidence e fato |
| --- | --- |
| 017/018 e 019/020 | Wrappers 0xb1d114 e 0xb23b74; bytes 041/042, `ev_6947ff46f28389ba1619d5a83b361d632906a9cd15962d77bc328433b7c8a393` e `ev_b7476a0339e1eaea300a53d3a64d68eec404c773b04ef5e6d4a1641c85f216a6`. |
| 021/022 | Seletor comum; assembly `ev_197e97f615685f2c397603e13b727957f43b19977da8d9a144347a1ae654e91b`; bytes 043 `ev_d7a8fd5b06fa180e1abf568188a9ddb6538c2b9befc77059a883ca2dab7a1f74`. |
| 023/024 | IDs exatos e dispatch por modo; bytes 044 `ev_8ec38d61d00cd35f05e75eb2e20cd156ff24a34425c148f4b853243b1086783a`. |
| 025/026 e 027/028 | Predicados angular/virtual e angular/magnitude; assembly `ev_aa1a9e5cb7b9073408319b427ff24ff6d646b0b80fa6cded46dbe469cb8084d4` e `ev_7480ca6e2f858b45aaa74eca13fdd517829196207a37c9163238bcee89ac71b9`. |
| 029/030 | Corpo não contíguo do gate; assembly `ev_ed09032992c69597ea55a7ab8244d4a62fa320c0b942a86e5f49b65827640b7e`. |
| 038–040 e 047–049 | No-op, slots virtuais, gate, folha e referência angular; folha 048 `ev_8a3d90418b0e93787e76e566817c3da5252e6e9ff4899c19b3e840e7fa2b3f42`; IDs completos e hashes no readback. |
