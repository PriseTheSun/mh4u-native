# Eventos dos dois LMT do caçador

O inventário cobre **122 ações preenchidas em 204 slots**, incluindo as sete
ações pareadas. O formato de duração/máscara tem suporte nos bytes, no loader,
no binder e em dois trechos ARM executados isoladamente. Há 66 ações com
máscaras não nulas. Nenhuma máscara recebeu nome de ataque, dano, som ou efeito.

O produtor é `tools/resources/lmt_events_probe.py`. `animation_probe.py` continua
dono do LMT, dos headers de ação e dos tracks. O novo módulo interpreta somente
os blocos apontados por `events_offset_unparsed`, verificando o hash contra os
bytes usados pelo dono. Os pacotes de juntas, curvas e sampler nativo v2 não
foram alterados; o catálogo de eventos não está habilitado no runtime.

## Fontes e cobertura

| Entrada | Arquivo ARC | Recurso | Slots | Preenchidos | Ações com máscara não nula |
| --- | --- | --- | ---: | ---: | ---: |
| 54913 | `/data/pl_mot_lco.arc` | `player\mot\co\lco_0` | 68 | 19 | 8 |
| 92608 | `/loc/data/pl_base_0.arc` | `player\mot\co\co_0` | 136 | 103 | 58 |

Os SHA-256 dos LMT são, respectivamente,
`dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a` e
`c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8`.
O produtor extrai novamente pelo índice e verifica nome, ARC, tipo, tamanho e
hash contra o manifesto pareado fixado. Essa operação valida o membro e seus
bytes comprimidos; não é uma nova leitura de hash da ROM inteira.

O resultado privado está em `research/private/lmt-events-v1/inventory.json`,
1.539.505 bytes, SHA-256
`375e25420b7f12cf5e21763424ecfef0e053e4b976d9a4d36dc4381b8bfb8fd0`.
Ele inclui todos os slots, ausências/rejeições explícitas, os sete pares com
índices de clip, offsets absolutos no LMT, spans e hashes de bytes, campos raw,
intervalos derivados, set bits e valores do prefixo correspondentes.

| Grupo ordinal | Grupos | Registros | Registros com máscara não nula | Ações com máscara não nula |
| --- | ---: | ---: | ---: | ---: |
| 0 | 122 | 128 | 3 | 3 |
| 1 | 122 | 122 | 0 | 0 |
| 2 | 122 | 329 | 108 | 66 |
| 3 | 122 | 122 | 0 | 0 |

São 488 grupos e 701 registros. Todos os grupos têm durações positivas cuja
soma é exatamente `N`, o frame count do header da ação. Não houve rejeição,
intervalo fora do arquivo, soma divergente ou flag de relocação especial nesse
corpus. Excluindo somente o ponteiro absoluto, os conteúdos de grupo têm 120
valores únicos e 368 instâncias repetidas. Isso é deduplicação de conteúdo,
não identidade compartilhada de endereço. Grupos 1/3 zerados aqui não provam
ausência de uso em outros LMT.

## Layout observado e consumidor

Todos os inteiros abaixo são little endian. O ponteiro em ação+`0x34` referencia
um bloco com quatro grupos consecutivos de `0x48` bytes, total `0x120`.

| Offset no grupo | Representação | Estado da interpretação |
| --- | --- | --- |
| `+0x00` | 32 × u16 | Bytes preservados; hipótese de remap por bit, apoiada em fonte primária |
| `+0x40` | u32 | Contagem dos registros, confirmada pelo consumidor |
| `+0x44` | u32 | Offset absoluto no LMT para registros de 8 bytes |
| Registro `+0` | u32 | Máscara combinada por OR, confirmada pelo consumidor |
| Registro `+4` | u32 | Duração somada cumulativamente em coordenadas de frame |

O loader `0x23e3d8` reloca os quatro ponteiros em bloco+`0x44`, `0x8c`, `0xd4`
e `0x11c`. A ação com flag `0x02000000` pula essa relocação interna; o perfil
rejeita essa variante em vez de presumir offsets iguais. A flag observada em
todas as 122 ações é `0x00800000`.

O binder `0x95aa90`, confirmado também pelo assembly `0x95aaf8–0x95ab20`,
grava os endereços dos quatro grupos em `M+0xf8+4*i`, com
`M=modelo+0x240+camada*0x130`. O avanço usa esses ponteiros, produz quatro
máscaras em `M+0x108` e guarda as anteriores em `M+0x118`; atributo de runtime
`0x10` desvia esse processamento. Os parâmetros temporais e a escolha de
intervalos ficam fora do novo produtor.

O núcleo inteiro em `0x958638–0x958678` e sua segunda cópia em
`0x9586ac–0x9586ec` fazem, para uma consulta inclusiva `[begin,end]`:

```text
cumulative = 0; result = 0
para cada registro, em ordem:
    cumulative += duration
    se begin < cumulative: result |= mask
    se cumulative > end: terminar
```

Com as durações positivas observadas, isso equivale a combinar os registros
cujos spans `[start,endExclusive)` intersectam os frames inteiros consultados.
O parser retém zero duration, se aparecer, com aviso: nesse caso a analogia
com um intervalo vazio não substitui o comportamento do loop. Soma acima de
`INT32_MAX` é retida, mas bloqueia `range_mask`, pois o wrap/comparações signed
do ARM exigem outro perfil. A política local limita 250.000 registros por
fonte, oito fontes e 4.096 slots; não são limites declarados do formato.

**`N` armazenado e limite temporal são fatos diferentes.** Os eventos cobrem
`[0,N)`, mas o binder define `frameMax=N−1` e o avanço retido reduz endpoints
de consulta `>=frameMax` para `frameMax−1`. O teste do núcleo inclui o ponto
`N−1` para verificar o parser, sem afirmar que o avanço normal o consulta.
Repetição, avanço reverso, predição, despacho e eventos atravessados em saltos
dependem da política do consumidor e não são selecionados por este catálogo.

O [layout](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/event.inl)
e o [consumidor de referência](https://github.com/PredatorCZ/RevilLib/blob/e17f9718ea571f040a7e1f331c28a4d45a6066b5/src/mtf_lmt/event.cpp)
do RevilLib chamam o prefixo de remaps e indexam cada bit ligado nessa tabela.
Esse é apoio independente para a hipótese; ainda falta confirmar essa leitura
no dispatcher MH4U. As fontes fixadas, seus hashes e licença estão retidos em
`research/private/lmt-events-v1/references/`; nenhum código delas é executado.

## As sete ações pareadas

Os demais registros têm mask zero. Valores do prefixo continuam numéricos e
não identificam comandos executáveis no novo motor.

| Entrada:slot | Grupo | Span armazenado em frames | Máscara | Bit → prefixo u16 |
| --- | ---: | --- | ---: | --- |
| 54913:3 | 0 | `[17,18)` | 1 | 0 → 0 |
| 54913:3 | 2 | `[6,7)` | 1 | 0 → 0 |
| 54913:3 | 2 | `[26,27)` | 2 | 1 → 1 |
| 92608:75 | 2 | `[20,21)` | 1 | 0 → 13 |
| 92608:75 | 2 | `[40,41)` | 2 | 1 → 12 |
| 92608:101 | 2 | `[36,38)` | 1 | 0 → 7 |

54913:5, 92608:18, 92608:82 e 92608:1 têm somente máscaras zero, mas seus
blocos/contagens/ponteiros continuam presentes. **Prefixo zero não foi tratado
como ausência**: ele está ligado a máscaras ativas nos exemplos acima. As sete
ações totalizam 28 grupos, 40 registros e seis registros com máscara não nula.

## Verificação e reprodução

Nove testes sintéticos cobrem limites de headers/payloads, count hostil, limite
agregado, ponteiro nulo, variante de relocação, soma divergente/overflow,
duração zero, bit 31, consultas de fronteira, identidade e reconstrução dos
campos. O roundtrip real recompôs todos os headers e registros interpretados:
**40.744 bytes idênticos**. Os LMT completos foram extraídos e retidos intactos;
não foi produzido um patch nem regravado o arquivo inteiro.

`research/private/lmt-events-v1/verify_arm.py` usa Unicorn 2.1.4 já instalado e
o ELF fixado em [root-motion-findings.md](root-motion-findings.md). Somente 128
bytes de código original, em dois núcleos, são mapeados para execução. Os
registros recebem estado explícito; leituras fora de count/pointer e qualquer
escrita em memória interrompem o teste. Somente o ponteiro do header de grupo
é relocado para a memória sintética, seguindo o layout confirmado.

Resultado em `arm-validation.json`: **89.697 consultas e 179.394 execuções ARM**,
zero diferença de máscara e zero acesso fora do escopo. Inclui todo ponto
inteiro, todos os pares de frames vizinhos e consultas junto aos limites de
registros, nos 488 grupos. Os 701 registros foram alcançados. Foram executadas
3.233.868 instruções e verificadas 798.368 leituras. Isso prova os dois núcleos
isolados para os dados consultados, não a execução do jogo ou seu dispatcher.

A verificação de ranges encontrou 610 spans de eventos, todos dentro dos dois
LMT e sem interseção entre si ou com os headers, tracks, buffers e extremos
declarados pelo parser proprietário. O verificador não presume que bytes
desconhecidos restantes são livres. Reprodução com nova extração gerou um
`inventory.json` byte-idêntico; caminhos de extração diferem na proveniência.

Em macOS ARM64, a reprodução aquecida consumiu 0,181 s de parede, 0,178 s CPU
e pico RSS de 59,5 MB no processo. O oracle levou 8,418 s de parede, 8,411 s
CPU e pico RSS de 436,1 MB. Não se mediram execução fria, processo-família,
Windows ou custo em tokens; não há baseline de desempenho anterior.

```sh
python3 -B -m unittest discover -s tools/resources/tests -p test_lmt_events_probe.py -v
python3 -B tools/resources/lmt_events_probe.py \
  --index research/private/resources.sqlite \
  --paired-manifest assets/local/paired-player-actions-v1/manifest.json \
  --expected-sha256 30f85c1c2707cad8406b42d3e07bc2be2945f20ee781dbec3c05de075ffca261 \
  --output research/private/lmt-events-new
research/private/root-motion-runtime/venv/bin/python -B research/private/lmt-events-v1/verify_arm.py
```

API do novo dono: `parse_events(bytes, parsed_lmt)` devolve o inventário,
`range_mask(group, begin, end)` devolve somente o u32 acumulado e
`verify_raw_roundtrip(bytes, inventory)` compara cada span reconstruído. Não
há normalização de prefixos, conversão para segundos nem callback de gameplay.

## Evidência e pergunta seguinte

Os IDs de Evidence do loader/binder/advance permanecem no JSON. Cópias de
apoio e hashes dos retornos REA estão em `lmt-events-v1/evidence/`. Os IDs
principais são `ev_2341ca4a…` (loader), `ev_0efaabf9…` (binder),
`ev_536563d3…` (assembly do binder), `ev_4c3dbbd0…` (advance) e
`ev_e2259ed8…` (assembly do advance), todos do mesmo ELF fixado. Não foi aberta
outra sessão Ghidra para este trabalho.

A pergunta ainda necessária ao REA é: **qual consumidor lê as máscaras em
`M+0x108/+0x118`, encontra os bits ativos e usa o prefixo u16 do grupo, e como
seleciona seu handler?** `0x95fdd0` é um helper de máscaras por intervalo, não
prova de dispatch. O setter `0x9575dc`, em investigação coordenada separada,
liga fontes e semeia masks; também não atribui por si só nomes aos eventos.
É preciso resolver xrefs/callers do consumidor para demonstrar significado
e momento de disparo antes de ligar qualquer ataque ou efeito ao novo motor.
