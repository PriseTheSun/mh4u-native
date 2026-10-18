# Política de ação: tabela original e seleção `pl_state=0`

Esta etapa acrescenta três ramos de `0xb3fc50` ao dono nativo
`player_selection`, com execução original completa de pedido, setter e
transição. Também identifica a tabela de política instalada por `act_no=0`.
A decisão completa input → ação ainda não foi executada: a tabela tem gates
anteriores que não podem ser omitidos para escolher seus últimos registros.

O estado inicial do construtor **não** pertence automaticamente ao ramo
`selector=0x203` validado aqui. O construtor `0xb0b994`, chamado por
`0xb0c1d4`, grava `pbw+0x358=0x44160000` (bits de 600.0f). Isso segue o ramo
acima do limite e consulta recursos de área. Esse ramo era rejeitado pelo perfil0.14 descrito abaixo. A extensão
[player-selection-area.md](player-selection-area.md) agora o resolve com
contexto explícito; ausência de contexto permanece desconhecida. As contagens
e fixtures desta página continuam sendo os históricos de0.14.

## Fontes e diferença entre observação e execução

Alvo: ELF ARMv6 little-endian
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
A retenção nova é `research/private/player-action-policy-v1/`.
`source-observed/source-facts.json` verifica **12 readbacks / 27.704 bytes**
contra o ELF, conserva Evidence IDs, argumentos, hashes e desassemblagens.

Foram reutilizados os handlers retidos na sessão `1792281468783`, a seleção
na sessão `1792282296342` e as novas consultas da sessão `1792300233949`.
A última sessão pertenceu ao agente responsável pelo REA; não houve importação
adicional por este recorte. O span de `b0d0e4` inclui os gaps da análise Ghidra:
nenhum corpo parcial de procedimento foi tratado como função completa.

A tabela e o interpreter abaixo são **conclusões estáticas**. A execução
original desta entrega começa em `b3fc50` e termina em seu retorno, no perfil
condicional declarado. Unicorn 2.1.4 é ferramenta privada de análise; nenhum
runtime ARM/3DS foi incorporado ao programa nativo.

## Produtor e consumidor da tabela

Use `pbw=u32(player+0xe30)` e `pw=u32(player+0xa29c)`.
O dispatcher `b12130` lê `u16(pbw+0x372)`; o índice 0 encaminha para
`b4f010(player,0)`, e o índice 1 para `b4f5dc(player,0)`.

Na entrada de `b4f010`, quando `pbw+5==0`, o handler escreve 1 nesse byte,
chama `b22094(player,0x8000,0,0)` e testa `b07e80`. A folha `b07e80` compara
`u8(pbw+0x6f9)` com **1**, não com qualquer valor não zero. Se o teste é
falso, chama `b6a97c(player,0xf2efb8,0)`.

`b6a97c` instala a tabela em `pw+0x100`, converte o terceiro argumento inteiro
com sinal para float32 em `pw+0x140`, grava zero em `pw+0x144`, limpa os três
u16 em `+104/+106/+108` e o byte `+148`, e chama o fill real para limpar
48 bytes de latch a partir de `+10a`.

`b0d0e4` consome esse ponteiro, percorre registros de **22 bytes**, limitado
a 48 registros e ao primeiro byte `0xff`. Consulta `b24354(player,record)`;
a resposta pode marcar o latch correspondente em `pw+10a+i`. Gates de tempo,
contexto e ação permanecem entre a resposta de input e a chamada `b1c050`.
O caller `b1407c` executa essa consulta antes do despacho de handlers, sujeito
a outros gates, incluindo `b17f78`.

A tabela `0xf2efb8` contém **35 registros e um sentinel**, 792 bytes com o
sentinel. O catálogo privado preserva todos os words e os bytes originais.
Três registros finais demonstram o vínculo com magnitude sem atribuir nomes
de movimentos:

| Registro | `act_st` | `act_no` | Flags de transição | Word do predicado | Word de contexto |
|---|---:|---:|---:|---:|---:|
| 32 | 1 | 2 | 1 | `0xc017` | `0x4009` |
| 33 | 1 | 1 | 1 | `0xc009` | `0x4008` |
| 34 | 1 | 9 | 0 | `0xc008` | `0x400b` |

`b24354` aplica `word+6 & 0x3fff`. Seu caso 8 aceita retorno não zero da
virtual `+0x98` no canal 0; o caso 9 exige retorno unsigned maior que 2.
O caso 23 exige primeiro a virtual não zero e depois `b24204` com argumento
`u16(record+8)`. A virtual concreta de uPlayer é `b417a4`: seus gates podem
retornar zero; fora deles, usa a classificação `0/1/3` com limites 50 e 110
já recuperada em [player-input-state.md](player-input-state.md).

Isso não permite remover os 32 registros anteriores, ignorar o contexto ou
escolher uma ação apenas pela magnitude. Também não prova direção física do
joystick, significado de cada ação ou gameplay completo.

## Contrato nativo adicionado ao dono existente

A assinatura permanece `player_selection::begin_common(State,CommonArguments)`;
`player_controller_step::apply_common` já compõe esse dono com comando e
transição. Foram anexados três campos opcionais, mantendo os agregados e os
callers antigos:

- `PbwFields::field6f9`: u8 original.
- `PbwFields::field358`: word original, comparado como **int32**, não float.
- `PwFields::field7fc`: u8 original.

Ausência significa desconhecido. Campos ignorados pelo ramo não são exigidos.
O argumento inteiro `pl_state` é comparado antes de qualquer redução a byte.
O novo perfil admite exatamente zero; os perfis históricos 1 e 3 continuam.

Após o prefixo comum, a seleção de `pl_state=0` é:

| Condição, na ordem original | Pedido em `b27c1c` | Continuação `b1c050` |
|---|---:|---|
| `pw9a8!=0`, `pbw6f9!=1` | 24 decimal (`0x18`) | `(0,0,flags)` |
| `pw9a8==0`, `pw7fc!=1`, `pbw6f9!=1`, `pw594!=0` | `0x246` | `(10,0,flags)` |
| Mesmos gates, `pw594==0`, `int32(pbw358)<=0x43160000` | `0x203` | `(0,0,flags)` |

O modo do request e a conversão int32→float32 de interpolação/frame são
preservados. Valores conhecidos `pbw6f9==1`, `pw7fc==1` no ramo que o lê, ou
`pbw358` acima do limite retornam `unsupported_selection_context`; um campo
consumido ausente retorna `unknown_selection_context`. Nenhum lookup externo
foi substituído por um booleano falso.

`PendingAnimation` retém qual continuação foi escolhida. No ramo `0x246`,
ela continua `(10,0)` mesmo se `pw594` mudar durante o pedido. No ramo
histórico de `pl_state=1`, a leitura posterior de `pw594` continua no ponto
original. O caller deve fornecer o estado após o pedido; não há retomada
implícita com o estado anterior.

## Oracle completo e comparação nativa

`verify_zero_arm.py` reutiliza o harness completo anterior, os helpers reais
só ampliando os intervalos permitidos para os três ramos e `b07e80`. Não há
stub, patch de retorno, PC rewrite ou interceptação de resultado.

O snapshot autoritativo é `arm-second/arm-validation.json`, SHA-256
`9c0cf0f96a44a361a370b724916cd161e43827528782cf918b33f86c134b3bb7`:

- **162 retornos originais**, 192 chamadas ao setter, 81 aceitas.
- 122.492 instruções, 44.275 leituras e 31.071 escritas observadas.
- Modos 0/1/2 e retorno real do dispatcher no modo 3, uma/duas layers ativas,
  flags 0/1/2, gates locais verdadeiros/falsos, valores int32 de conversão,
  selector já igual e bancos explicitamente ausentes.
- Limite signed de `pbw358`, valores negativos raw e comparação de bytes com
  1 diferenciada de bool; campos ignorados com valores discriminantes.
- Oito guardas negativos: cinco fatos/bytes consumidos omitidos e três ramos
  além do perfil. A execução para por erro conhecido; não retorna sucesso.

Os LMT imutáveis são os membros originais 92608/co_0 e 92610/co_2, fixados
por SHA e fornecidos ao teste C++ pelo dono `motion_resource::Resource`.
Os bancos 0/2 são uma escolha explícita da receita de loader recuperada em
[player-motion-banks.md](player-motion-banks.md). A aquisição ou ativação
original dessa receita não foi executada neste oracle.
O slot 24 de co_0 é vazio e permanece uma rejeição real; os slots 3 e 70 de
co_2 são ações presentes. Há casos com os argumentos exatos de `b312dc`
`(pl_state=0,interpolation=0,frame=0,flags=2,mode=1,extra=0)`, mas sujeitos aos
campos diagnósticos acima, não aos valores completos do construtor.

`generated-final/player_selection_zero_fixtures.hpp` é projeção dos estados,
argumentos e retornos observados, sem fórmula nativa para gerar esperados.
Bindings usam a identidade viva do Resource, banco, slot e grupo; bindings
retidos usam um token diagnóstico separado. O teste compara todos os campos
declarados dos donos seleção/transição, as duas layers, efeitos do comando e
retorno da transição. O original conserva toda memória opaca fora da lista
permitida por guardas e snapshots; isso não é contado como bytes de resultado
nativo produzido. Storage canônico adicional não lido pelo perfil é inicializado
explicitamente no adapter e não atribuído ao estado original do jogo.

A validação final fica em `native-complete/native-validation.json`: **2.066 checks por execução** nas builds normais
e ASan/UBSan estritos, `-fno-sanitize-recover=all`, stderr vazio, fontes copiadas
e hashes de todos os headers transitivos conferidos antes/depois. Inclui a
regressão dos fixtures históricos de seleção e controlador, sem regenerá-los.
Os testes adicionais distinguem contexto ausente, gates excluídos, estado
inteiro fora do domínio, uso preguiçoso dos campos e falha tardia da transição
sem publicar alterações do pedido.

## Limites e próximo fechamento

`b4f010`, `b4f5dc`, `b0d0e4` e o caminho completo input→tabela→ação ainda não
foram executados nesta etapa. O perfil mantém layer flag2 antes do pedido,
portanto não usa a consulta de movimento anterior no wrapper; esse consumidor
já tem investigação própria, mas não foi composto neste oracle.

Para ligar a inicialização original, o próximo recorte mínimo é a consulta
`6c33d4→69d568` e seus dados de área, junto com a origem/contexto de `pw7fc`.
Para ligar input à ação, é necessário executar a ordem da tabela inteira no
contexto admitido, provar os gates que rejeitam os registros anteriores e
preservar latches/efeitos; não basta executar isoladamente os predicados 8/9.
Não há nomes de andar/correr, regras de combate, bootstrap completo ou testes
de execução Windows reivindicados por este documento.
