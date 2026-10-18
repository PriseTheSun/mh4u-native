# Seleção de ações do caçador: recorte estático

Foi recuperada uma cadeia numérica entre estados de `uPlayer`, pedidos de
animação e o binder de LMT. Ela demonstra seleção de recurso/slot, parâmetros
de frame e velocidade, e uma regra de parada quando o loop do recurso é
negativo. **Ainda não demonstra uma política completa de joystick, andar,
correr ou ficar parado.** Nenhum controlador foi implementado neste recorte.

Esta análise usa somente código e dados locais. Não executou o jogo nem as
funções ARM. Os ensaios isolados anteriores de root motion permanecem em
[root-motion-runtime.md](root-motion-runtime.md), com seu próprio escopo.

## Alvo, cobertura e retenção

- ELF ARMv6 LE: SHA-256
  `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
- REA/Ghidra 12.1.4, perfil `analysis-tools-focused.json`, heap 3 GiB,
  `Decompiler Switch Analysis` desativado. Sessão `1792281468783`.
- Respostas completas: `research/private/rea-session-1792281468783/`.
- Bundle: `research/private/player-action-controller/rea-evidence.json`, SHA-256
  `80b342fb901133d95b2ebff4bc7c74da7db1c22074e46ee3f69f46facb2c75f0`.
- Snapshot, manifesto, contrato numérico, tabelas de dispatch e desassemblagem
  auxiliar ficam no mesmo diretório privado. A sessão foi exportada e fechada;
  o cliente terminou com código 0. Não há garantia de replay sem reimportação.
- `source-readback.json` verificou **18 intervalos, 3.176 bytes** de
  `read_bytes` contra segmentos inicializados do ELF; todos coincidem.

Os valores de runtime, as unidades de tempo e a chegada a um ramo durante uma
partida não foram medidos. A reflexão é evidência dos nomes de campos, não de
que todo estado possível tenha sido compreendido.

## Identidade do objeto e cadeia de chamadas

O registro em 0xd2e068 referencia a string `uPlayer`, o descritor 0x10ba220 e
o descritor pai 0x10ba354. Na tabela virtual 0xe6bca4, o slot +0x14 contém
0xd05b4c, que retorna 0x10ba220. Essa cadeia identifica a tabela sem depender
de um nome atribuído pelo analista. O slot +0x70 aponta a 0x9575dc; +0x84 a
0xb085bc, getter do selector da primeira camada; +0x94 a 0xb15040.

| Etapa | Endereço e comportamento demonstrado |
| --- | --- |
| Estado principal | 0xb66a20 lê `pbw->act_st`; tabela de 14 destinos em 0xb66a40. O valor 0 chega a 0xb12130 por 0xb66a78/7c. |
| Ação dentro desse estado | 0xb12130 lê `pbw->act_no` u16, aceita índices inferiores a 182 e usa a tabela em 0xb1215c. |
| Exemplo de handler | `act_no=0` chega a 0xb4f010, argumento 1 igual a 0. `act_no=1` chega a 0xb4f5dc, também com argumento 1 igual a 0. |
| Pedido comum | 0xb0c060 ou 0xb0c198 → 0xb07480 → 0xb04e4c → chamada virtual +0x70. |
| Pedido virtual específico | O slot +0x94 de `uPlayer` usa 0xb15040, que remapeia alguns selectors e chama 0xb04e4c. Sua existência não prova que todo pedido passe por esse remapeamento. |
| Escrita da camada | 0x9575dc grava selector, atributos, frame e velocidade, marcando a camada como pendente. |
| Ligação posterior | 0x9573b8 → 0x9577ec consome o bit pendente 2 → 0xcd7540 obtém a ação → 0x95aa90 liga os tracks. Esta última cadeia reutiliza a investigação de root motion. |

O getter 0xb085bc lê `model+0x244`; seu retorno é um **selector de animação**,
não `act_no`. Da mesma forma, `act_st` e `act_no` não são IDs de slot LMT.

## Campos nomeados pela reflexão original

Defina `pbw = *(player+0xe30)`, `pw = *(player+0xa29c)` e
`M = model+0x240+layer*0x130`.

| Campo | Nome embarcado |
| --- | --- |
| pbw+0x370 | `pl_st` |
| pbw+0x371, u8 | `act_st` |
| pbw+0x372, u16 | `act_no` |
| pbw+4 | `r_no` |
| pbw+0x31 | `pl_type` |
| pw+0xc0, float | `mNowMotSpeed` |
| pbw+0x39c, float | `mPlayerSpeed` |
| pw+0xc4, float | `mPlayerSpeed2` |
| pw+0x4c / +0x50 | `mActionTimer` / `mDashTimer` |
| pw+0xdc | `mBlendPnt` |

0xb1c050 grava os argumentos de estado em `act_st`/`act_no`, preserva os
valores anteriores em pbw+0x377/+0x378, zera bytes de subestado em +5/+6/+7 e
reinicia vários campos. Entre as escritas estão `mPlayerSpeed=1` e
`mPlayerSpeed2=1`. Isso prova um reset nessa função, **não velocidade constante
durante a ação** nem a equação que combina esses campos com `M.mSpeed`.

## Contrato observado do pedido de animação

O assembly de 0x9575dc recebe `r0=model`, `r1=layer`, `r2=selector`,
`r3=runtime attr`; os argumentos VFP são `s0=parâmetro de interpolação`,
`s1=frame inicial solicitado`, `s2=velocidade`. As escritas principais são:

| Destino | Fonte/efeito |
| --- | --- |
| M+4, u16 | selector |
| M+6, u16 | 2, ou 0x102 no ramo com transição |
| M+8, u16 | 2 |
| M+0xc | atributos fornecidos pelo caller |
| M+0x40 / +0x44 | parâmetro de interpolação; há exceção para camada 0 antes inativa |
| M+0x5c | `s2`, velocidade |
| M+0x48 | 1 |
| M+0x54 | `float(action.frameCount-1)` |
| M+0x4c / +0x50 | frame efetivo inicial após os testes observados |

O selector escolhe `bank=(selector>>8)&15` e `slot=selector&255`, lendo o
resource de `model+0xc04+4*bank`. O valor 0xffff é tratado como inativo.
Não foi demonstrado qual membro ARC ocupa cada bank no caçador carregado;
portanto um selector como 0x502 não pode ser associado a um arquivo extraído
somente por seu slot 2.

Quando `attr&0x20000` está presente, o frame efetivo incorpora o float em
M+0x14. O pedido também conserva o frame solicitado em M+0x10. Os testes de
limite e a inicialização de máscaras de eventos estão retidos em assembly;
esta documentação não oferece uma implementação genérica desses ramos.

**Regra nova comprovada:** em 0x9576e0–0x9576f0, `action.loopFrame<0` causa
`M.mAttr |= 4`. Esse atributo seleciona parada no fim no avanço já investigado.
O binder converter loop negativo em zero não elimina essa decisão anterior.
O campo `action.flags` observado como 0x800000 no corpus é outro campo e não
foi utilizado como substituto de `M.mAttr`.

0xb04e4c transforma o parâmetro de interpolação: acrescenta 2 quando positivo;
na camada 0 divide o resultado pela velocidade quando ela é diferente de zero.
Passa o frame e a velocidade adiante. Os ramos fornecem runtime attr 0 ou
0x10000, usando comparações que incluem a representação do float. Após sucesso,
o flag de pedido 0x20 ainda pode substituir `M.mAttr` por 0x2084. Assim, os
**flags de pedido**, o **attr da camada** e os **flags do header LMT** constituem
três domínios distintos. Esta análise não os unifica em um enum.

Os wrappers 0xb0c060 e 0xb0c198 colocam **1.0 em s2**. O segundo evita reiniciar
quando o selector atual já é igual ao solicitado. Ambos tratam sinal negativo
do parâmetro de interpolação por mudança de sinal e flag de pedido 2. Isso
não é prova de que o valor 1 represente 60 FPS reais.

## Exemplos numéricos que já podem orientar a próxima investigação

No handler de `act_st=0, act_no=1`, durante a entrada `pbw+5==0`, o código
marca +5 como 1 e, conforme os testes 0xd0473c/0xb09284 e `pl_type`, solicita
0x502, 0x523 ou 0x528. O caminho comum de 0x502 passa `s0=4`, `s1=0` e o
wrapper fixa `s2=1`. O pedido comum acrescenta 2 a `s0`, resultando em 6 para
esse ramo. São parâmetros da animação; não uma distância ou velocidade física.

O handler de `act_st=0, act_no=0` contém pedidos 1, 5, 0x15, 0x3f–0x43,
0x6a/0x6b, 0x203, 0x21f e 0x501, sujeitos a gates distintos. Essa diversidade
impede resumir o estado como uma única animação. O código consulta ambiente,
flags, valores do jogador e término de animação. Não foi atribuído o nome
“idle” a esse estado, nem “walk/run” ao estado 1.

0xb15040 possui remapeamentos condicionados pelo byte pbw+0x34: os pares
0x41/0x43, 0x3b/0x35, 0x3f/0x42, 0xbbd/0xbbc e 0xc1e/0xc1d. O significado de gameplay do byte não
foi demonstrado; não foi rotulado como sexo, arma ou modo de locomoção.

## Evidências principais e limites do provedor

| Resposta da sessão | Evidence e uso |
| --- | --- |
| 008-read_bytes | `ev_a625fa31b847a137381ad45ba09ea3c02dead374e5b06688028d37e15066f644`: setter 0x9575dc completo, incluindo literais |
| 020/021 | `ev_446c2de8538f09f06e98636268e3214bf33e0cba40c975e34788bb0476c9726c` / `ev_eb6278c440bbee5963dae3e3a153be3853f85200eff9b756608b57279d90d96a`: pedido 0xb04e4c |
| 019 | `ev_d0ba121f4fe6b39797928a272e1065f194301e2bf3f8169db7ba78e3fab02fb2`: escrita dos estados em 0xb1c050 |
| 028/029 | `ev_8a6be14922803bd05592ed9276f37468273f02684f9c777d6a6d460d5bdd1b91` / `ev_ee9e0dbe4212ea3b16d6c01af96da2ff0dee4518a1dc7ec7aa57a1ca049b675f`: remapeamento 0xb15040 |
| 033–036 | Tabela virtual, getter do descritor e registro de `uPlayer`; IDs completos no manifesto |
| 044/045 | `ev_3c19cbf08b631142f65b0a33f39487e94dd502a6d485c04f34206821eb17b00b` / `ev_ea768383255abb9f83c66e3f51ccb3b0a8dd08a65b44f680ad0f502e1aa08761`: handler 0xb4f010 |
| 052 | `ev_5b2163b7ffe8988f8aa199ec965f5efa263351a99abbeacc3e773e6ed5f4fa5c`: nomes e offsets da reflexão |
| 057/058/063 | Bytes da tabela de `act_no`, offset 0x372 e branches dos dois exemplos |
| 061/062/067 | Handler 0xb4f5dc, assembly e literais dos pedidos |
| 069 | `ev_20232674acca02930c858ebef99c7e4a16e9fbe860945b7e714d1579cdd7bd4f`: dispatcher de `act_st` e seus destinos |

Ghidra não reconheceu 0x9575dc e alguns getters como procedimentos. Para eles,
usaram-se bytes REA e desassemblagem LLVM, sem alterar a base efêmera ou o ELF.
Os resolvers também não incluem vários blocos das tabelas de switch. Listas
vazias de callers foram corroboradas com instruções de branch e não tratadas
como ausência de chamadas. O pseudocódigo 059 de 0xb12130 contém avisos de
blocos removidos e tipos instáveis; ele não sustenta os mapeamentos acima, que
foram lidos das instruções e tabelas explícitas. Argumentos VFP omitidos pelo
pseudocódigo foram conferidos no assembly.

## Próximas perguntas mínimas

1. Seguir os produtores de `act_st/act_no` em 0xb1c050, começando pelos callers
   0xb1d114 e 0xb23b74, e seus testes de input. Isso deve distinguir mudança de
   estado, subestado, término e solicitação de animação sem nomear por pose.
2. Localizar as escritas de `pw.mNowMotSpeed` (+0xc0) e o consumidor que calcula
   `M.mSpeed` (+0x5c). Os multiplicadores `pbw.mPlayerSpeed` e `pw.mPlayerSpeed2`
   têm nomes e resets comprovados, mas sua composição ainda não.
3. Recuperar a inicialização de `model+0xc04+4*bank` e associar cada resource ao
   caminho ARC/LMT. Só então selectors numéricos poderão selecionar as curvas
   reais correspondentes sem suposição sobre a ordem dos recursos.
4. Para validar os exemplos com execução isolada, declarar explicitamente os
   retornos dos gates, os bytes de subestado e os recursos ligados; interceptar
   uma chamada não demonstrará a política do gate interceptado.

Essas lacunas impedem promover este recorte a controlador original completo.
Ele fornece as fronteiras numéricas e os endereços para continuar a recuperação.
