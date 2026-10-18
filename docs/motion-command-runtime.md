# Pedidos de animação sobre estado em reprodução

`motion_command::apply` compõe os wrappers originais de uPlayer, o setter `9575dc` e as duas consultas `958884` do ramo que lê a animação em reprodução. A implementação usa `motion_request`, `motion_query` e `motion_sampler` como donos de seus contratos. Não duplica parsing LMT, mistura de camadas, codecs ou cursores.

A implementação anterior que recusava esse ramo está preservada em `research/private/motion-command-live-v1/before/`, junto com seu manifesto. As fixtures e relatórios históricos não foram reescritos.

## API e propriedade

```cpp
motion_command::apply(layers, active_count, fields, model, command,
                      globals, sampling, limits);
```

`layers` é um `span<const motion_pending::Layer>` emprestado do dono persistente, normalmente `motion_pending::State`. `Fields` contém apenas d48..d60, d68 e d70..d7c. O resultado contém novas camadas, esses campos auxiliares e observações dos pedidos/consultas. Não existe uma segunda representação de camada dentro do command.

- Os frames, máscaras e identidades de eventos continuam em `animation.layer`.
- Os dois words de camada +6c/+70 pertencem somente a `interpolation_curve_bits`. O antigo `LayerState.fields6c` foi removido.
- Tracks, cursores de modelo, bindings de joints, T/Q de inicialização e scratches `+98/+c8` permanecem nos donos canônicos existentes.
- O campo do controlador `player_transition.field3b8` é exatamente `layers[1].animation.layer.weight`. O compositor deve projetar esse valor novamente depois do pedido, antes da transição.
- `active_count` representa model +234 e é distinto da capacidade do storage. Uma query consome somente esse prefixo. Um pedido explícito pode acessar uma camada existente fora do prefixo ativo. O corpus inclui storage de oito camadas com quatro ativas.

O módulo não retém spans. Recursos/templates precisam permanecer vivos durante a chamada. Erros devolvem somente `Error`; camadas, campos auxiliares e cursores parcialmente atualizados não são publicados. Efeitos externos de um callback resolver pertencem ao chamador e não são desfeitos.

## Wrappers e ordem

| Entry | Fonte | Semântica |
|---|---|---|
| dispatch | b27c1c | modo 0 → if_changed; 1 → restart; 2 → variant; outros sem efeitos |
| restart | b0c060 | flags 0, velocidade 1; interpolação negativa troca sinal e acrescenta 2 |
| if_changed | b0c198 | compara selector uint32 completo com selector u16 atual antes dos gates |
| variant | b0c170 | flags 11, velocidade 1; negativo acrescenta 2 |
| common | b07480 | camada 0, depois sentinel na 1 se active_count unsigned >1 |
| layer | b04e4c | pedido na camada explícita existente |

Virtual getter +84 é o original b085bc; setter +70 é 9575dc. Não se aceitam callbacks virtuais arbitrários. Um modo desconhecido ou selector completo igual retorna antes dos dados que o fluxo não lê. `FFFF` e `1FFFF` continuam distintos antes do narrowing do setter.

No pedido de camada, interpolação positiva recebe 2 em binary32. Na camada 0 o valor é salvo em d68 antes da divisão por velocidade não zero. O teste que decide a query sempre lê flags da camada 0, inclusive num pedido para outra camada. A escolha do atributo 10000 preserva a comparação signed32 dos bits com 358637bd, seguida da comparação float negativa.

Após sucesso do setter, flags 8 escolhem o snapshot 104e050; senão flags 10 escolhem 104e058; senão 104e048. Esses dois words são copiados para os controles da curva. Flags 20 substituem atributos por 2084. No sucesso da camada 0, o módulo grava frame, endpoint, selector anterior e flags em d48..d60 na ordem recuperada. Nenhum snapshot global desconhecido vira zero.

O pedido sentinel da camada 1 usa selector FFFF, flags 0, interpolação/frame 0 e velocidade 1, mesmo após falha normal do primeiro setter. Ele observa os flags da camada 0 já modificados. Pode escrever o peso compartilhado com a transição mesmo quando seu setter retorna false. `ObservedRequest` registra chamadas já aplicadas; não é uma fila para reaplicação nem o retorno booleano dos wrappers.

## Ramo de consulta live

Com bit 2 em layer0.flags, d70..d7c e todos os cursores são preservados. Sem esse bit:

1. Se o esqueleto está explicitamente ausente, copia o snapshot fornecido de 10e6370 para XYZ e escreve W=+0. A leitura REA histórica desse BSS foi incompleta; o template não possui default presumido.
2. Com esqueleto presente, calcula `previous_frame - 1` em binary32. Se o resultado for negativo, usa frames 0 e 1. Caso contrário, usa `previous_frame - 1` e `previous_frame`.
3. Executa `motion_query::query` duas vezes, selecionando saída T e omitindo saída Q. O contrato original 958884 ainda amostra/mistura Q, mesmo sem escrevê-lo. Uma rotação fora do domínio numérico não é ignorada.
4. A segunda query usa os cursores canônicos resultantes da primeira. No sucesso final, d70.xyz recebe T2−T1, cada componente em binary32, e d7c recebe +0.
5. Só então aplica o setter. Uma falha normal desse setter preserva os efeitos reais das queries. Um erro tipado posterior não publica estado parcial.

A query usa todas as camadas ativas, suas máscaras de flags, pesos, atributos de espelho/aditivo e identidades já vinculadas. Um selector novo não muda retroativamente a identidade de tracks antigos. O command não roda binder, clock ou inicialização; o pending subsequente continua responsável pela nova associação.

## Domínio e limites

O perfil exige operandos consumidos finitos, nearest-even e gradual underflow. Zeros com sinal e dados brutos não consumidos são preservados; FTZ/DAZ/FZ e fast-math são recusados. Aritmética de frames/diferenças reutiliza `motion_transform_math`; a query é dona de seus erros de sampler, identidade, templates e mistura. Razões aninhadas e o índice da query ficam no erro do command. O módulo não presume FPS ou unidade física para d70.

Os defaults de cópia são oito camadas e 2.048 bindings de joints no total; a query tem seu limite separado. Limites são verificados antes das respectivas alocações. Falhas de alocação viram `resource_limit`. Um prefixo ativo maior que o storage é erro quando consumido por query; não bloqueia um caminho que o original omite. Getter e pedido exigem a camada efetivamente acessada. Capacidade não implica atividade.

## Evidência e validação

ELF original SHA256: `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.

As retenções do wrapper, virtuais e globals estão no índice `research/private/motion-command-v1/evidence-index.json`. O trecho decisivo é `command-llvm.asm`, b04edc..b04f64: subtração do frame, dois BL 958884, diferenças XYZ e W zero. Os bytes originais, verificados contra o ELF pelo dono histórico, são usados novamente. As evidências de 958884 e samplers permanecem nos donos `model-motion-query-v1` e `motion-sampler-v1`; nenhuma sessão Ghidra nova foi aberta por este módulo.

`research/private/motion-command-live-v1/verify_arm.py` executa wrappers, router, getter, setter, 958884 e samplers originais sob Unicorn 2.1.4 / ARM11MPCORE / FPSCR 0. Não há stubs nem substituição de outputs. A inicialização dos canais reais vem de 9577ec original; bancos, frames anteriores e globals são snapshots explícitos de teste, sem alegação de bootstrap do jogo.

`source-closure-final/validation.json` registra uma repetição do ARM original com todos os módulos Python locais importados fixados antes e depois. A fixture resultante é idêntica, byte a byte, à fixture canônica usada em `native-final`; isso não substitui o registro original nem altera seus resultados.

O produtor reutiliza o parser/dono de LMT para relocation de tracks e o inventário de eventos para relocation dos ponteiros de registros. Os dois recursos imutáveis são os LMT 54913 e 92608, fixados por SHA. Hooks só observam chamadas ou verificam código, leituras inicializadas, writes, retorno, limites e registradores preservados. As arenas fonte permanecem read-only e são relidas para conferir imutabilidade.

Oracle final: `arm-storage-final/oracle.bin`, SHA256 `7792ff35a871c651225700d0b83c3f9720adbbd9f310e55cd3461eeb577e013f`.

- 707 sequências, 488 delas reais, cobrindo 122 ações e até quatro camadas ativas em bancos distintos.
- 1.326 comandos; 1.914 queries; 7.964 samples; 1.541 setters, 610 aceitos.
- Inclui todos os wrappers, requests para camada 7 com storage 8 / active 4, prefixo ativo 0, selector igual, falha do setter, sentinel, frames abaixo, no valor e acima de 1, pesos, espelho/aditivo e pedidos consecutivos sem fabricar rebind.
- O adapter C++ consome `Resource` diretamente, compara argumentos/retornos observados e 10.862.592 bytes completos de modelo por execução. Confere identidades dos tracks e entradas imutáveis; um teste falha deliberadamente na segunda query após avanço de cursor comprimido, comprovando resultado transacional.
- `native-final/validation.json` retém comandos, tempos, pins de todas as fontes/headers transitivos e fixtures antes/depois. Normal e ASan/UBSan estrito passaram com stderr vazio: 71.075 checks históricos e 49.329 checks live em cada execução. Dez processos negativos alteram outputs ARM, estrutura e comprimento da fixture, com SHA recalculado para alcançar o comparador.
- A fixture histórica de 1.837 casos continua intacta e é reexecutada com um adaptador de teste para o novo storage canônico. As novas fronteiras ficam separadas do comportamento histórico em `live_boundaries()`.

`arm-initial` e `arm-final` são tentativas anteriores preservadas: a finalização tornou explícitos bindings nulos nos casos sintéticos e acrescentou capacidade 8 / atividade 4. Nenhum output esperado foi derivado do C++.

Essas provas cobrem o pedido/consulta numéricos, não o scheduler, dispositivos físicos, combate ou equivalência de execução em hardware 3DS. O launcher e os pacotes permanecem intactos. A composição seleção → pedido live → transição tem validação separada sob responsabilidade do dono do controlador.
