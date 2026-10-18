# Pedido nativo de animação

`src/motion_request.cpp` reproduz o setter numérico **0x9575dc** e o getter
**0xcd7540** em C++ para macOS/Windows. Recebe uma camada, um pedido e bancos
explicitamente descritos; devolve a nova camada e o retorno original de
sucesso/falha. Não chama o binder de tracks nem escolhe uma ação de gameplay.
O viewer continua usando sua política diagnóstica anterior.

## Comportamento recuperado

O selector inteiro é comparado com `0xffff` antes da redução para u16. Outros
valores usam bank `(selector>>8)&15` e slot `selector&255`. O getter lê o count
u16 da tabela; recurso ausente, tabela nula, índice fora dela e slot vazio são
estados explícitos. Banco desconhecido é erro, nunca ausência presumida.

A rotina escreve flags 2 ou 0x102 conforme a camada e o selector anterior,
preserva o offset anterior e grava interpolação, peso e velocidade. Com ação e
esqueleto presentes, calcula `float(frameCount-1)`, aplica o offset quando
`attr&0x20000` e liga os quatro descritores de eventos. Loop negativo acrescenta
o atributo 4. Não se atribui FPS a `speed`.

Os endpoints de consulta têm assimetria observada: o frame solicitado usado
como início é limitado a `[0,end]`; o frame efetivo só recebe limite superior.
Pode continuar negativo e ser anterior ao início. Cada máscara usa comparações
signed estritas no acumulador de durações. Duração zero continua presente.

Falha do getter ou esqueleto nulo retorna false depois de atualizar a camada:
selector vira `0xffff`, frames e máscaras zeram e attr perde `0x104040`. Os
vínculos antigos dos eventos permanecem. O C++ representa cada vínculo por
identidade estável do recurso, banco, slot e grupo; substituir um banco não
redireciona silenciosamente esses vínculos. O dono do recurso deve fornecer
uma identidade estável não zero e manter sua associação com a fonte verificada.

## Fronteira do perfil

A API é transacional: erros do perfil não publicam uma camada parcial. Isso
é diferente do retorno false original, que é uma saída suportada com mutações.
Os descritores são fatos tipados emprestados durante `apply`; não são offsets
ARM reinterpretados nem um parser alternativo de LMT.

Exigem-se operandos finitos, binary32 com arredondamento ao mais próximo e
underflow gradual. O resultado de `frameCount-1` e o frame efetivo devem caber
na conversão signed32 sem saturação. O count de slots tem limite original u16;
counts de eventos e somas de duração ficam no domínio signed32 não negativo.
Esses limites de conversão/acumulador delimitam este perfil, não o formato
completo. Não se emulam flags de exceção FP.

Os quatro descritores de eventos precisam estar presentes, mesmo que seus
counts sejam zero. Um ponteiro `action+0x34` nulo não equivale a quatro grupos
vazios no setter original: a soma de 72 no grupo seguinte produz um endereço
não nulo. Esse caminho é rejeitado, sem inventar uma máscara vazia.

## Evidência e validação

O ELF é `research/private/mh4u-analysis-bss.elf`, SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
O diretório `research/private/motion-request-v1` retém scripts, resultados,
fixtures, proveniência, fontes compiladas e manifesto.

- REA/Ghidra: quatro respostas retidas; dois readbacks, **592 bytes**, iguais
  ao ELF. Setter: sessão 1792281468783; getter: sessão 1792286478819. O getter
  não chama helpers. Evidence IDs e hashes completos em `source-readback.json`.
- **2.416 execuções ARM completas**: 1.684 estados sintéticos e 732 pedidos
  com headers/eventos de 122 ações reais dos LMT 54913/92608. A instalação
  dessas ações em um banco sintético não prova o contexto de gameplay.
- Bytes originais do setter/getter, páginas de código somente leitura,
  leituras inicializadas, escrita limitada à camada escolhida e stack, sete
  controles negativos, sem retorno de helper substituído. Máximo 542 instruções
  por caso. ABI e bytes não modelados da camada também conferidos.
- C++ otimizado e ASan/UBSan: **55.585 verificações cada**, todas aprovadas,
  incluindo retorno, bits float, campos preservados e identidade dos vínculos.
  O oráculo vem da execução ARM, não da fórmula C++.
- As 122 ações vêm do inventário hash-bound já produzido pelo dono dos
  parsers. Os hashes dos dois LMT e os bytes de seus records foram conferidos.
  A primeira rodada sintética está preservada em `initial-synthetic/`.

`verify_arm.py`, `retain_source.py` e `validate_native.py` reproduzem os
respectivos recortes. O primeiro se recusa a sobrescrever um relatório final;
use um diretório de saída separado ao repetir. O relatório mede tempo/CPU/RSS
do harness ARM; tempos de compilação/execução nativa estão separados. CPU/RSS
da família de processos nativos e custo de tokens não foram medidos.

Ainda faltam os wrappers de pedido `b07480/b04e4c`, sua composição com a
seleção de estados, binder `9577ec/95aa90`, scheduler e política física de
controle. Não se afirma que uma partida ou controlador completo foi executado.

A revisão independente não encontrou divergência numérica no perfil. O cálculo
de máscaras foi centralizado em `motion_event_kernel.hpp`, compartilhado com
`motion_events::Package`; cada dono conserva sua validação de duração/count.
O harness nativo agora confere o SHA do oracle com o relatório ARM antes de
executar e fixa os hashes de entradas/fontes antes e depois. As fontes
compiladas estão retidas; a rodada anterior à revisão está em `pre-review/`,
com snapshots verificados contra os hashes daquela compilação. Os testes
nativos normal e ASan/UBSan passaram novamente após essa refatoração.
