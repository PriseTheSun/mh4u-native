# Atualização de joints e consulta de movimento do modelo

A cadeia recuperada separa três estados: samplers por joint/layer, transforms
correntes dos joints e snapshots de transição. `958884` consulta os canais de
movimento **do modelo**; não atualiza joints. `95b790` prepara/amostra a pose
antes do avanço de relógios de `9573b8`; a mistura dos snapshots de transição
ocorre depois, no virtual `+4c = 95b490`. A construção seguinte, pelo virtual
`+50 = 95b940`, ainda exige fechar seus helpers. Nenhum desses passos deve ser
substituído por uma convenção de glTF.

Este recorte contém prova estática e **154 execuções ARM isoladas**, sem stubs.
O ensaio cobre somente uma layer ativa, na folha de joints `95df38` e na
consulta `958884`, usando tracks constantes ou ausentes e estados sintéticos
explícitos. Não executa o scheduler completo, as duas rotas de mistura entre
layers, nem o caminho inteiro de aplicação da pose.

## Fonte e retenção

ELF `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Sessão REA `1792290219632`, Ghidra12.1.4/ARMv6, heap3GiB, perfil focused,
exportada e encerrada. Retenção em `research/private/joint-update-v1/`:
`evidence-index.json`, `readback.json`, `manifest.json`, bundle e snapshot.
Toda leitura completa foi comparada aos bytes PT_LOAD do ELF fixado.

| Evidência | Cobertura |
| --- | --- |
| nova003/008/009 | bytes/pseudo/assembly958884; pseudo `ev_8a76842fc41c11fcfad0c308889cea0575fcc5b87af0e9512516f79083d13c3b` |
| nova004/014 | folha95df38 e seus dois literais finais; bytes `ev_5d570ace227916c7d8f8c129395204d951908b93ba9250479cc62d5567fa8074` |
| nova005/015 | folha95e6ec e literais até95f114 |
| nova006/007/012/013 | helperd022a0, prefixo95b790, tabela de índices e80620 `[1,2,0]`, assembly completo |
| nova010/011/017 | callers de958884,95b790,9573b8 |
| nova018–020 | wrapperb06b38, inclusive bytes/literal |
| nova025 | vtable uPlayer slots+4c/+50; `ev_02835813ab081b7da403db22f33f0d4872acb14aa28ee83f16afb8277849b0da` |
| nova031–035 |95b490/95b940: pseudo, assembly da mistura e bytes completos |
| sessão1792274318294/023,025,054,066,067 | limites e pseudo95b790,9573b8,95f210,b06f8c reutilizados |
| sessão1792281468783/020–021 | wrapperb04e4c reutilizado |

`95b790` tem corpo não contíguo, reconhecido pelo Ghidra como uma função com
cinco faixas: `[95b790,95b930)`, `[95df38,95e280)`, `[95e294,95e5e0)`,
`[95e6ec,95ea30)`, `[95ea44,95f108)`. As folhas são entradas de tail branch;
não são declaradas aqui como procedimentos independentes recuperados.
`958884` executa `[958884,958bcc)` e `[958bdc,958c70)`, com literais entre
as duas faixas. Dumps LLVM são leitura auxiliar da mesma fonte; os intervalos
mais largos incluem vizinhos e não definem limites de função.

## Estado e ordem observada

Offsets, máscaras e strides são hexadecimais. Defina modelo `O`,
`L(i)=O+240+i*130`, contagem de layers `C=O.word234`, contagem de joints
`N=O.word220`, `J=*(O+224)`, `A(i)=*(L(i)+128)`, `B=*(O+c00)`.
J tem stridea8, A stride34, B stride38. Os donos são separados, conforme o
produtor já documentado em [motion-pending.md](motion-pending.md).

Em `9573b8`, depois dos retornos para recurso nulo/contagem zero:

1. Se alguma layer tem flags&2, chama `9577ec` uma vez.
2. Combina os atributos de **todas** as layers, inclusive as inativas:
   `O238 = (OR(attrs)&ffffff) | (oldO238&ff000000) | (O23c&ffffff)`.
3. Chama `95b790` antes de avançar frames e máscaras de eventos.
4. O restante do relógio segue o contrato já recuperado em
   [root-motion-runtime.md](root-motion-runtime.md).

`95b790`, com O238&20000000 limpo, procura flags100 nas layers: se existe,
tailcall95c7dc; caso contrário retorna sem atualizar joints. Com bit20000000:
prepara `L64=clamp(L4c,0,L54)` e
`L68=clamp(f32(L4c+f32(L5c*O14)),0,L54)`, e chama a aplicação de movimento
`95cab0`. Attr800 passa L64 por `d12f8c`, fora do oracle desta rodada.
N=0 ou **atributos da primeira layer** `L(0).attrs&200` encerram o trabalho
antes dos joints. Nos demais casos:

- `O238 & 82004140 == 0`: tail para95df38.
- Caso contrário: tail para95e6ec.

Essa ordem é local às funções observadas. Não estabelece FPS, ordem física de
input, nem scheduler global. A folha isolada pressupõe L64 já preparado;
não apresenta esse pressuposto como execução de95b790 inteiro.

## Folha95df38: contrato de uma layer ativa

A entrada lê `rModel=*(O+f0)` e seu ponteiro de matrizes+6c, mesmo com N=0.
Percorre layers em ordem crescente e ignora selectorffff. Um modo local começa
em1, recebe OR2 quando attr80000 está presente, e passa a0 após cada layer
ativa. Para cada joint:

- J+94&1 impede atualização.
- Se modo!=1 e A+30&0f==0, pula o joint.
- O helperd022a0 lê somente O238bit28. O modo de referências globais é
  esse bit ou o attr40000 da layer.
- Q presente em A+0: chama aea0e4 em L64. Q ausente: usa snapshot global
  10e6560 no modo global, ou conversão da matriz rModel+6c+j*40.
- T presente em A+10: chama aea0e4 em L64. T ausente: usa 10e6370 no modo
  global, ou **J+48**, a translação de referência deste joint.
- Esta folha não lê o channel de escala A+20.

Na primeira layer aceita, escreve Q raw em J+58, escala global10e6380 em
J+68, T em J+78 e zero nos paddings+74/+84. Pesos da layer e dos channels
**não atenuam essa primeira cópia**; Q não é normalizado implicitamente.
A conversão matriz→Q segue as equações de95c7dc já documentadas; a tabela
next `[1,2,0]` desta folha fica em **e80620**, distinta da tabelae32eb8
usada por95c7dc.

O caminho estático de layers posteriores usa pesoQ=A+c*L48 e
pesoT=A+1c*L48. Na mistura normal, T usa lerp; Q usa ajuste de hemisfério,
lerp raw e normalização do resultado. Attr40000 acrescenta T ponderado e
forma um deltaQ pelo blend normalizado desde o snapshot globalQ, antes de
multiplicá-lo à esquerda do Q corrente. O produto não recebe normalização
extra. Esses ramos têm bytes e assembly retidos, mas **não foram executados
no oracle154**. Sua equação e ordem float32 completas exigem perfil próprio.

Attr40 normalmente entra em O238 pelo fold e escolhe a outra folha. A ausência
de leitura de escala em95df38 não significa que a escala de animação é
ignorada no jogo.

## Folha95e6ec: fronteira estática

A folha alternativa lê Q/T/S, com channels em A+0/+10/+20, e contempla a
mistura aditiva e o mapeamento de espelho. Para attr100 e low8(J+98)!=ff,
seleciona outro sampler/matriz/referência pelo índice mapeado; no ramo
correspondente inverte Tx, Qy e Qz. Retemos o corpo e os literais completos,
sem afirmar que todos os índices, defaults, combinações de atributos e
pesos estejam cobertos por execução. O próximo oracle deve validar os
índices contra N e separar essa rota da folha já demonstrada.

## 958884: consulta dos canais de cena/modelo

ABI observada: r0=modelo, r1=saídaT4 opcional, r2=saídaQ4 opcional,
s0=frame consultado. Não lê N, J ou A. Inicia agregadoT em10e62c0[3] e
agregadoQ em10e6560[4]. Percorre layers em ordem crescente e ignora
selectorffff ou flags&2. Para cada layer aceita:

- Restringe o frame a0..L54; attr800 chama d12f8c.
- Chama aea0e4 para T em L+d8 e Q em L+e8, inclusive com ponteiro de track
  nulo e com saídas finais nulas.
- Track ausente retorna `[0,0,0,1]` pelo sampler: não usa os defaults de
  referência dos joints. Attr100 inverte Tx, Qy e Qz.
- Primeira layer aceita copia T/Q raw, sem normalizar ou aplicar pesos.
- Layers posteriores usam pesoT=L48*(L+e4), pesoQ=L48*(L+f4); mistura
  normal/aditiva tem a estrutura descrita acima, ainda estática neste perfil.

SaídaT tem quarto word zero; saídaQ tem quatro componentes. Saída nula
suprime somente a escrita final, não o sampling. Com tracks comprimidos o
sampler pode mudar cursor/tempo: não chamar esta consulta de função pura.
O oracle atual usa somente referência1/2 ou ausência, sem mutação de cursor.

Callers diretos retornados por REA: b04e4c e b06b38. Em b04e4c, o ramo com
pendingbit limpo e skeleton presente consulta T duas vezes e grava a
diferença em O+d70/+74/+78; o ramo sem skeleton usa global10e6370. Essa é a
dependência ainda excluída do perfil inicial de motion_command.

Em b06b38, sejam a e b os floats recebidos em s0/s1,
`step=f32(L0+5c * O14)` e `v=L0+50`. A primeira consulta usa
`max(v-step,0)`; se v-step<0, substitui v por step; a segunda usa `max(v,0)`.
Forma `D=(T_B-T_A)*b + O[d70:78]*(a*step)`, com operações float32 na ordem
retida. Depois aplica escala por componente O+60/64/68, três rotações por
helpers de tabela568ab8/568a80 e ângulos O+d28/d20/d24, acumula em O+d00/04/08
e copia para O+40/44/48 com quarto word zero. Os Q retornados não são lidos
nesse trecho. O wrapper e os helpers não foram executados aqui; não atribuímos
significado de gameplay aos dois coeficientes.

## Depois do relógio: snapshots e saída seguinte

A leitura da vtable de uPlayer em e6bca4 confirma:
`+4c=95b490`, `+50=95b940`. O callerb06f8c, após9573b8 e seus gates
intermediários, chama os dois virtuais nessa ordem quando O+d64&2==0.
O wrapper genérico95f210 também chama9573b8→virtual4c→virtual50, com outro
helper antes. Isso demonstra ordem nesses callers, não frequência global.

95b490 chama primeiro6d385c, ainda não fechado. Seu corpo seguinte busca
flags100 nas layers; se não encontra, zera O23c e retorna. Se encontra,
percorre B e J. Quando B.layer!=ffffffff, J.flags94&41==0 e
`t=L(B.layer)+60 != 0`, mistura Q corrente com B.Q por lerp de hemisfério
normalizado; T/S por `corrente*(1-t)+snapshot*t`, zerando os paddings.
Não há clamp de t nesse trecho. Logo, a pose amostrada antes do relógio é
consumida junto com snapshots B **depois** do relógio; não é ainda a pose
final da transição. O helper inicial e o corpo não foram executados aqui.

95b940 chama1a63fc; quando há recurso, chama
`95de18(O,*(O+22c),N,0)` e marca O230|1. Em um gate de flags também grava
O+c4c=2. Faltam os contratos desses helpers e da tabela22c antes de afirmar
construção final de matrizes/paleta. Não se deduz uma hierarquia glTF desse
ponteiro.

As leituras adicionais95b9d8 e95881c estão retidas, mas não foram usadas para
substituir os virtuais:95b9d8 limpa índices/flags e chama95c020 por joint;
95881c marca ancestrais via índice `(J94>>8)&ff`. Sua ligação a esta cadeia
completa e o comportamento de ciclos/índices inválidos continuam desconhecidos.

## Oracle154 e menor próximo recorte

`verify_slice.py` executou bytes originais em Unicorn2.1.4/ARM11MPCORE,
FPSCR0, com allowlist de código, leituras inicializadas, escritas delimitadas,
retorno real, SP e registradores callee-saved conferidos. Hooks observam ou
rejeitam acessos; não reescrevem PC, retorno ou amostra. Foram **78 casos de
joints e76 de consulta**, 26.737 instruções, 13.084 leituras e8.286 escritas.
O esperado separado compara todos os bytes de sete arenas; a conversão de
matriz reutiliza o dono de equações fixado do oracle de pending, não o output
ARM. Os resultados e hashes estão em `arm-third/arm-validation.json`.

Cobertura: N0..3; zero ou uma layer ativa, inclusive após inativas;
Q/T presentes ou ausentes; seis matrizes; templates explicitamente não
triviais; gates de flags; saídas nulas; frames antes/depois do domínio e pesos
−1/0/.25/.5/1. Q não unitário é preservado na primeira cópia. Seis controles
negativos rejeitam ausência de count, matriz, templateS, track, orçamento
insuficiente e escape para d12f8c. Dois ensaios iniciais interrompidos ficam
preservados: descobriram a tabelae80620 e o literal95e5e4 ausentes da primeira
allowlist. São diagnósticos do harness, não falhas observadas no jogo.

O ensaio levou0,0841s de parede e0,0905s de CPU, RSS máximo do processo
42.598.400bytes. Esses números são do pequeno oracle já carregado; não incluem
a importação Ghidra (~152,7s), memória da família REA nem tokens.

Reprodução em diretório novo:

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/joint-update-v1/verify_slice.py --output NOVO_DESTINO
```

O menor próximo contrato executável é a folha95df38 com uma layer e tracks
reais, compondo o sampler proprietário, mantendo J/A/referências separados.
Para liberar a dependência de motion_command, um segundo contrato isolável é
958884 com uma layer e suas consultas sucessivas, preservando cursores.
Antes de ligar a saída ao controlador, fechar6d385c→95b490 e
1a63fc→95de18→95b940, além das rotas de transição necessárias. Mistura entre
layers, espelho, attr800, scheduler e política de gameplay permanecem fora
desta cobertura executada. Nenhum runtime novo foi implementado neste recorte.
