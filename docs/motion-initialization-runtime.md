# Inicialização completa das ligações de animação

`motion_initialization::initialize` executa a semântica recuperada de
`0x95aa90` até o retorno. Compõe o prefixo `motion_binding::prepare_tracks`
com o sampler canônico `motion_sampler::sample`; não mantém outro cursor,
parser LMT, frame ou interpretação de event bindings. O resultado contém as
ligações e os transforms base/anterior. A rotina ainda não consome os pending
bits do chamador `9577ec`, avança o relógio, mistura joints ou produz gameplay.

## Estado e propriedade

O chamador entrega o estado anterior, mapa de bone ID para joint, ação
verificada, parâmetro de tempo bruto do modelo e snapshots explícitos dos
templates globais. `State::layer` continua sendo `motion_request::Layer`;
`State::tracks` continua sendo `motion_binding::Tracks`. Os campos adicionais
são os quatro floats nos offsets relativos ao layer:

| Campo | Offset | Tratamento |
| --- | --- | --- |
| prediction_rotation | `+20` | Preservado; consumido por atributo10000 |
| prediction_translation | `+30` | Preservado; somente XYZ consumido |
| previous_rotation | `+78` | Escrito pelo sufixo |
| base_rotation | `+88` | Escrito pelo sufixo |
| previous_translation | `+a8` | Escrito; W recebe+0 |
| base_translation | `+b8` | Escrito; W recebe+0 |

Esses campos representam o storage original e não uma segunda cópia do
estado de playback. Uma futura composição com o relógio deve usar esses
mesmos valores em sua projeção de estado. Os quatro templates são opcionais
para representar desconhecimento, mas cada ramo exige os que efetivamente
consome: vetor`10e62c0`, vetor`10e6370`, quaternion`10e6560` e fallback de
inversa`10e6570`. Seus valores escritos no startup foram demonstrados em
[root-motion-defaults.md](root-motion-defaults.md); isso não prova imutabilidade
em runtime. A API não os preenche silenciosamente.

O resolver retorna `TrackView` emprestado ou `ResolutionError` com código e
mensagem próprios. Um track presente não resolvido falha e conserva sua
`TrackReference` exata no erro; jamais se transforma em canal ausente.
O dono `motion_resource::Resource` conserva bytes imutáveis, SHA256 e identidade
local não reutilizada. Suas APIs `binding_action` e `sampler_track` são usadas
na comparação com os recursos reais. Recursos/views devem sobreviver à chamada;
o initializer não retém pointers, spans ou callbacks depois do retorno.

Erros de binding, resolução, sampler, template ou aritmética não publicam estado
parcial. `prepare_tracks` valida os limites antes de copiar os joints. Há no
máximo seis consultas sequenciais ao sampler; cada uma respeita o orçamento de
keys fornecido, portanto o teto agregado é seis vezes esse limite. Uma ação
explicitamente ausente preserva tudo, respeitando somente o orçamento de cópia
herdado do prefixo, sem consumir mapa, templates, resolver ou estado numérico.
O perfil mantém a exigência de storage original possuído, não nulo e disjunto;
não modela aliasing ou overflow de pointers ARM.

## Ordem e equações recuperadas

O prefixo define `end=float32(uint32(frameCount-1))` e
`loop=float32(max(loopSigned,0))`. O sufixo lê os atributos após os ORs do
prefixo. Os testes de bits não são nomes de estados do caçador.

1. Com bit8, copia o template vetorial para baseT; caso contrário, amostra T
   em loop e depois em end, guardando `T(end)-T(loop)` e W+0.
2. Com bit1000, copia o template quaternion para baseQ; caso contrário, amostra
   Q em loop e depois em end, guardando `inverse(Q(loop))*Q(end)`.
3. Se qualquer bit de8100 estiver presente, troca o sinal de baseT.X e baseQ.Y/Z.
4. A preparação do transform anterior segue a precedência abaixo.

| Primeiro ramo aplicável | Transform anterior |
| --- | --- |
| bit400 | Calcula `f = frame - speed * modelDelta`. Se`f<=0`, copia templates; senão amostra T/Q em f. Depois aplica o espelhamento8100. |
| bit2 | Copia templates e retorna **sem espelhar os templates anteriores**. |
| bit10000 | Amostra T/Q em requested_frame(+10), espelha a amostra se8100, calcula `previousQ = Qsample * inverse(predictionQ)` e `previousT = Tsample - R(previousQ)*predictionT`. |
| nenhum desses | Amostra T/Q no frame(+4c), depois espelha se8100. |

`R(q)` é o polinômio de matriz do quaternion bruto visto nas instruções; não
normaliza q. Para a inversa, a norma quadrada acumula X,Y,Z,W nessa ordem. Se
a norma arredondada for positiva, usa conjugado dividido pela norma. Se for
zero, usa o snapshot`10e6570`; esse fallback é diferente da identidade.
O produto Hamilton preserva a ordem de VMUL/VMLA/VMLS por componente.
Não há normalização acrescentada pelo binder. As regras próprias de um codec
continuam pertencendo ao sampler.

Os caminhos`95ad9c` e`95aeb0` copiam vetores do header se o loop do layer for
negativo. Pela entrada completa, o prefixo sempre torna esse loop não negativo.
Não adulteramos registradores ou estado no meio da função para alegar cobertura
desses caminhos inatingíveis pelo contrato. O atributo400 também não implica
segundos, frames por segundo ou taxa física: modelDelta é uma entrada explícita.

## Perfil numérico

A conversão inteira do prefixo exige nearest-even. Aritmética consumida pelo
sufixo exige binary32, nearest-even e gradual underflow; o guard usa operandos
volatile e rejeita FZ/FTZ/DAZ sem modificar o ambiente. Cada multiplicação,
soma/subtração e divisão é arredondada separadamente, como o VFP2 observado,
sem FMA do host. Entradas ou resultados aritméticos não finitos falham. Uma
norma positiva tão pequena que a inversa transborda também falha. Copiar bits
ou trocar sinal não impõe validação a campos que o ramo não lê.

O domínio admite quaternion não unitário e norma arredondada zero com fallback
explícito. Não substitui isso por identidade. Não modela FPSCR exception flags,
traps ou outros modos de arredondamento. `-ffast-math` é rejeitado no build.
A comparação bit a bit abaixo vale para o corpus executado sob FPSCR0, não
uma equivalência universal de todas as representações IEEE ou do hardware3DS.

## Evidência e validação

ELF fixado: `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
REA sessão1792287542293/057,058,060 retém o prefixo/helper/literal. A leitura
1792288837845/004 retém todo o sufixo`95acd8..95b260`,1.416bytes,
Evidence`ev_0495d68b15414b6f6c5e7ffc080d711c5fd0b533ba3d3460675fb8d21f58e9e2`.
As quatro leituras,2.040bytes incluindo sobreposição do literal, coincidiram
com PT_LOAD inicializado. O readback do dono do sampler cobre dispatcher,
getters e decoders usados. Pools de literais e tabela de branches são dados,
nunca intervalos executáveis permitidos no harness.

Dois oracles privados executam a função inteira e os helpers originais em
Unicorn2.1.4/ARM11MPCore, sem stubs, código alterado ou retorno fabricado:

- **2.444 casos** com canais ausentes e codecs referência1/2: combinações dos
  atributos, precedência, frame anterior negativo/zero/positivo, quaternions
  não unitários/zero/tiny, templates sintéticos não triviais, u32 frameCount e
  s32 loop extremos. Máximo763instruções por caso.
- **1.098 casos**:122ações reais dos LMT54913/92608, nove configurações cada.
  Todos os canais de modelo desses recursos usam1/2/4/5 ou são ausentes. Há
  713casos com baseT nãozero. Máximo3.702instruções por caso. As queries
  sequenciais percorrem o mesmo cursor inclusive ao regressar do fim para
  um frame anterior. Os canais dos joints são ligados, não amostrados aqui.

Os mapas de ossos, storage disjunto, associação ao banco0, estado anterior e
snapshots de templates são entradas sintéticas declaradas. Os descritores,
headers, keys e extremes vêm dos bytes reais fixados. Isso não afirma que
esses mapas/estados sejam os da instância do caçador em gameplay.
Leituras precisam estar inicializadas; writes ficam limitados ao layer,
bindings de joints e stack. Verificam retorno/SP/callee-saved registers,
globais e recursos imutáveis e bytes de modelo fora do layer. Nove controles
negativos cobrem dados ausentes, write guard, orçamento e execução de literal.

O C++ passou **237.147 checks de referências +150.091 de recursos reais** em
cada build normal e ASan+UBSan. A segunda suíte carrega os LMT pelo dono nativo
`Resource`, não por um parser paralelo do teste. Compara transforms por bits,
campos preservados, bindings/identidades e cursores. Rejeições públicas incluem
resolver ausente, template desconhecido, NaN/overflow consumido, ambiente
numérico e erro transacional. Uma alteração proposital do resultado ARM no
fixture foi detectada. As fontes e entradas foram fixadas antes/depois.
A validação local executa macOS arm64; Windows e integração ficam com o build
central. Não houve execução do jogo original, scheduler ou validação visual.

Artefatos privados: `research/private/motion-initialization-v1/` contém
`reference-final/arm-validation.json`, `real-arm-validation.json`,
`native-validation.json`, `source-readback.json`, inputs/outputs, snapshots e
manifesto. A primeira versão somente referência permanece em
`reference-only-v1/`; ensaios anteriores à correção da probe do sampler ficam
em`pre-sampler-probe/`. São evidências históricas, não o relatório final.
Os ensaios ARM finais mediram1,94s/2,85s de parede e2,17s/3,21s de CPU,
RSS máximo62,3MB/101,9MB, respectivamente. Métricas dos processos nativos e
argv completos estão no relatório; não medimos tokens nem memória da família
inteira de processos, nem comparamos um workflow anterior equivalente.

Reprodução com destinos novos, preservando os relatórios existentes:

```sh
research/private/root-motion-runtime/venv/bin/python research/private/motion-initialization-v1/verify_arm.py --output NOVO_DIRETORIO
# Para os demais geradores, use uma cópia privada com destino novo; eles recusam
# sobrescrever relatórios finais. Os argv dos testes estão em native-validation.json.
```

O próximo vínculo é o chamador de pending layers e, depois, os consumidores de
pose/clock usando este estado e o mesmo Resource. Nenhum canal faltante deve
ser convertido em track inventado para contornar erro de resolução.
