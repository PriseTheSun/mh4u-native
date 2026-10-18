# Frame original de animação do modelo

A composição original `9573b8` foi executada até o retorno, com pending, inicializador, sampler, aplicação, joints e relógio reais. O oracle canônico contém **2.586 frames em 862 sequências de três ticks**, sobre as 122 ações presentes dos LMT 54913/92608 e os 19 joints do MOD body001. O módulo C++ coincidiu em **36.188.964 bytes de estado**, com 797.985 verificações por execução normal e ASan/UBSan. Nenhum resultado de sampler ou chamada ARM foi interceptado.

Isso recupera a atualização numérica de um modelo. A seleção de ações, colocação em banks, estado inicial de joints/modelo, step de tempo e templates são entradas diagnósticas explícitas. Não é prova do bootstrap do caçador, de FPS, de scheduler, de controle de gameplay, nem de execução da GPU.

## Evidência e origem

Todos os endereços e offsets abaixo são hexadecimais. O ELF tem SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. A nova sessão REA `1792297437998`, Ghidra 12.1.4/ARMv6, heap 3 GiB, perfil focused, só reteve o helper que faltava: `cd5db4`. Foram reutilizados os bundles e trechos anteriores de pending, sampler, aplicação, joints, clock e floor.

`research/private/model-animation-frame-v1/readback.json` confere 26 readbacks existentes contra o ELF, registra Evidence IDs e enumera os intervalos sem readback REA. `source-slices.json` retém **todos os 69 intervalos de código/dados efetivamente carregados**, completos e com offsets/hashes do ELF. Assim, ausência de um readback anterior não é apresentada como cobertura do provedor; os bytes executados continuam fixados ao artefato. Os arquivos `rea-evidence.json` e `rea-snapshot.json` preservam a nova sessão, exportada por `006` e fechada por `007`; o bridge saiu com código 0.

O helper `cd5db4` tem corpo de 68 bytes, seguido de 12 bytes de literais. Readback completo de 80 bytes: `ev_a7c2b176006cc6b03e263c3fc355ca4f5feb2cba76d5d348ee2f455044453cd9`; assembly: `ev_4db94e021506364390d9fc96e83f50f5bccf8761cd95655fa94ce1859013970e`. O pseudocódigo desta função retorna vazio porque não exprime o cálculo VFP/retorno em `s0`; essa resposta é uma limitação de análise, não evidência de função sem efeito.

Não foi alterado nenhum launcher, processo do jogo, GUI ou byte executável. O PID44975 indicado no início não estava presente na consulta; isto foi comunicado ao responsável pelo launcher, sem tentativa de reinício por esta investigação.

## Ordem e gates de9573b8

Defina `M` como modelo, `L(i)=M+240+i*130`, `N=M.word220`, `C=M.word234`, `J=*(M+224)` e snapshots `B=*(M+c00)`. As tabelas de canais por layer estão em `*(L+128)`. Strides: J=`a8`, B=`38`, canais por joint=`34`.

| Ordem | Operação original | Condição e efeito |
| --- | --- | --- |
| 1 | Gates da entrada | Retorna se `M+f0` é null ou C=0. Não prepara frames nem avança clock. |
| 2 | Pending9577ec | Varre todas as layers; se qualquer `u16(L+6)&2`, chama uma vez e encerra a busca. Não exige selector ativo. |
| 3 | União de atributos | OR dos `u32(L+c)` de todas as layers, inclusive selectorffff; `M238=(oldM238&ff000000)\|((ORattrs\|M23c)&00ffffff)`. |
| 4 | Pre-update95b790 | Prepara amostras, aplica movimento e atualiza joints conforme os gates abaixo. |
| 5 | Tail957f80 | Avança clock, transições e máscaras de eventos depois da amostragem da pose. |

C é carregado antes de pending e preservado em `r5` durante a união. As funções seguintes releem suas contagens. O recorte não muda contagens durante pending e não presume capacidade de alocação a partir desses valores.

### Preparação95b790

Se `M238&20000000` está limpo, a função procura qualquer `u16(L+6)&100`. Se encontrar, chama `95c7dc`; caso contrário retorna. Este ramo não prepara frames nem chama aplicação/joint leaves. O caller ainda segue para o clock.

Com esse bit ativo, para cada selector diferente de `ffff`:

1. Carrega `current=L4c`, `end=L54`, `speed=L5c`, `delta=M14`.
2. Calcula `next=f32(current + f32(speed*delta))` a partir do **current original**, antes de seu clamp.
3. Limita current a end quando `current>end`, depois a +0 quando `<0`; limita next a end quando `end<next`, depois a +0 quando `<0`.
4. Quando `Lc&800`, aplica `d12f8c` somente a current.
5. Escreve `L64=current` e `L68=next`. Selectorffff preserva ambos os campos.

As comparações são ordenadas VFP. No perfil finito, empates preservam sinal de zero. A multiplicação e a adição arredondam separadamente a binary32, sem FMA. A abreviação clamp não autoriza substituir o tratamento de NaN ou normalizar −0.

Depois da preparação, `95cab0` é chamado. A atualização de joints só ocorre se N!=0 e **atributos da primeira layer** não contêm `200`, mesmo se essa layer está inativa. O bitmask global `M238&82004140` escolhe `95e6ec` quando não zero; caso contrário escolhe `95df38`. Os contratos próprios de aplicação e joints preservam suas ordens, normalizações, matrizes e mutações de cursor.

### Floor e curva de transição

`d12f8c` não chama helpers nem escreve memória. O original preserva −0 e retorna os bits de entrada para expoente pelo menos23, inclusive Inf/NaN. Os outros ramos fazem floor por manipulação dos bits; uma soma com sentinela positiva pode alterar flags FPSCR. O ensaio separado executou 18 entradas, incluindo subnormais assinados, ±0, frações, limiar2^23, máximos finitos e padrões Inf/NaN. Outras 28 entradas finitas passaram por95b790 real e verificaram preparação, inclusive o fato de só L64 receber floor. O C++ rejeita operandos consumidos não finitos; não promete reproduzir os flags FPSCR nem o caminho não finito completo.

`cd5db4` recebe `r0=L+6c` e `s0=t=remaining/interpolation`, lê os dois floats a,b e devolve um polinômio. Forma algébrica real: `3*a*t + (3*a-3*b+1)*t³ + (3*b-6*a)*t²`. A ordem binary32 original é: `t2=t*t`; `a3=a*3`; `b3=b*3`; `v=t*a3`; `t3=t2*t`; `q=b3-a*6`; `k=(a3-b*3)+1`; `v=v+t3*k`; resultado=`v+t2*q`. Cada multiply/add/sub arredonda separadamente. Não faz stores ou chamadas externas. Esta fórmula não confere nome de curva ou política de blend ao chamador.

## Dados reais e estado diagnóstico

O payload `assets/local/player-probe/body001/source/entry-44780.bin` fornece registros, remap256 e matrizes A. O vínculo loader `R+6c → S+24*N` já está documentado em [joint-render-binding.md](joint-render-binding.md). `model_scene.parse_skeleton` é o dono offline desses fatos; esta investigação não reinterpreta vértices, materiais ou o contrato glTF.

São fontes reais imutáveis:

- LMT54913, SHA `dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a`.
- LMT92608, SHA `c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8`.
- MOD, seu SHA e fatos completos estão no relatório canônico.

O dono `animation_probe` fornece offsets de ações/tracks; `lmt_events_probe` fornece os quatro grupos e pointers de records. A cópia privada em RAM reloca apenas esses pointers conhecidos. Prefixos e records de eventos permanecem os valores reais. As páginas dos LMT são somente leitura e seu conteúdo é conferido após cada entrada.

A escolha inicial explicita: mapa real; referências T derivadas de A[12..14]; Qidentity e Sone; flags parent nos bits8..15 de J94; mapping98=índice próprio; snapshots inicialmente sem owner; banks0/1 atribuídos aos dois LMT; posição modelo(10,20,30); orientação identidade; escalas1; delta.75. Templates explícitos são Tzero, Qidentity, inverse-fallback Qzero e Sone. Esses valores não são inferidos de RAM zerada. O vínculo dessas escolhas ao bootstrap e a persistência dos templates após startup permanecem desconhecidos.

As 610 sequências iniciais cobrem cada ação com attrs0/800/100/40000/4, uma ou três layers e pesos/interpolações distintos. Outras244 começam junto ao fim da ação, em loop e clamp. Oito sequências adicionais exercitam recurso null, contagem zero, gate global desligado, first-layer200, selectorffff, velocidade negativa, delta zero e step300. Cada sequência preserva o estado ARM entre três entradas; não reescreve flags, cursores ou máscaras entre ticks.

O audit independente dos bytes confirma 1.724 ligações contínuas entre ticks, as 122 ações, 209 mudanças de blocos de máscaras e a ordem das entradas observadas. Executaram-se 105.010 chamadas do sampler original, 1.476 entradas da folha normal, 1.098 da especial, 1.098 do floor e 1.348 do helper de curva. Os masks são calculados; nenhum callback de gameplay ou remap downstream é executado.

## Oracle, consumidor nativo e reprodução

Produtor: `research/private/model-animation-frame-v1/verify_arm.py`. Snapshot canônico: `arm-third/verify_arm_snapshot.py`. Fixture: `arm-third/oracle.bin`, SHA-256 `5f0ced4b10b5707d5d0d6e41725f180d4b8145cce49a9873558deebbddb772b1`.

`MH4UFR01` retém before/after completos de modelo8192, J, B e cada tabela de canais; inclui templates, map, matrizes e colocação em banks. `case-pins.json` fixa cada record e a receita. O trace registra entradas de funções com argumentos; `traces.json.gz` contém conjuntos de PCs por caso, não uma lista cronológica de todas as instruções. As guardas observam todas as leituras/escritas e restringem código, constantes, arenas, stack e orçamento. Não corrigem estado nem fabricam retorno.

O adapter `src/model_animation_frame_tests.cpp` lê somente esse formato de fixture. `motion_resource::Resource` continua o único parser nativo dos LMT e resolve ações/tracks/eventos pela identidade; o adapter apenas mapeia essas identidades para endereços do oracle. Compara o modelo inteiro, todos os joints/snapshots/canais e o complemento opaco preservado, mais ordem de getters e escolha de folha. Como o endereço ARM não codifica bank lógico, confere também as identidades completas dos tracks e eventos: bank do selector quando o inicializador cria a ligação; bank anterior quando ela é preservada. Um controle nativo usa bank7 com selectorbank0 e confirma retenção; mutar somente bank para8 é detectado mesmo que a projeção de ponteiros permaneça igual. Esse controle não é contado como nova execução ARM. Sete controles negativos verificam falhas tipadas de resolução, identidade, eventos, modelo desconhecido, limite e frame não finito, com entrada intacta.

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/model-animation-frame-v1/verify_arm.py \
  --output research/private/model-animation-frame-v1/arm-replay
python3 -B research/private/model-animation-frame-v1/validate_native.py \
  --output research/private/model-animation-frame-v1/native-replay
```

Os diretórios de saída devem ser novos. Unicorn2.1.4 só é necessário para refazer o oracle privado; não integra executável ou testes C++ de replay. A execução nativa usa o fixture congelado e os dois LMT com SHA obrigatório. O script nativo compila consumidores reais, roda normal e Address/UndefinedBehaviorSanitizer e confere invariância das fontes.

O original executou 30.542.557 instruções, 9.023.501 leituras e 4.763.597 escritas em62,72s de parede nesta máquina. CPU e pico RSS do processo estão no relatório. Custo da importação REA é separado: a primeira resolução levou158,17s, majoritariamente cold analysis. Não medimos tokens do trabalho nem CPU/pico de família REA. Esta investigação não executou o oracle em Windows.

Tentativas anteriores estão preservadas: arm-first/second têm menos casos; arm-second inclui errata de pin porque o hash do script foi lido depois de uma edição opcional já fora do código carregado. arm-third salva seu snapshot antes de executar e confere o hash final. `native-final` passou os testes, mas o coletor tentou hashear diretório.dSYM; `native-complete` corrige apenas essa coleta e repete a validação íntegra. A revisão independente encontrou depois que a projeção de endereço apagava o bank lógico; `native-bank-final` acrescenta sua comparação, controles e uma cópia independente da entrada para verificar imutabilidade também da identidade, sem mudar runtime ou fixture ARM. Nenhuma dessas falhas é omitida ou atribuída ao código do jogo.

## Fronteira com saída e render

O caller95f210 faz `1a5eac → 9573b8 → virtual4c → virtual50`; para uPlayer, os dois últimos são95b490 e95b940. Evidence da ordem: `ev_c6c4f5fd196b55be982ba80d4ad3971eef66c882313280cd604d9333cee6baa8`. O callerb06f8c também chama9573b8 antes dessas duas virtuais, mas tem gates/intermediários próprios e condiciona a saída a `O+d64&2==0`; Evidence `ev_08098f2a7b71908e43a6873a5a8ed0283bc50812b9b82d1f80d80728dd5eb9d2`.

Portanto95b490 consome o blendL60 **depois** do avanço de relógio nesses callers, e95b940 constrói a saída seguinte. O oracle atual encerra no retorno de9573b8: não executa essas duas chamadas, não demonstra sua frequência no scheduler e não prova políticas de cache/seek/render. A composição seguinte deve preservar o mesmo owner de joints, camadas e snapshots; callbacks de saída não suportados devem falhar explicitamente. Flags de header LMT não são attrs runtime. Bootstrap original, produtores de flags, scheduler, ordenação física de input e política do caçador continuam recortes separados.
