# Ligação de joints, paletas e desenho

O código original fornece a ligação que faltava entre `J+8` e uma paleta de desenho: `959d00` multiplica a segunda tabela de matrizes do MOD pelas matrizes hierárquicas, serializa 12 floats por joint da paleta e acrescenta um vetor do contexto. `9613b8` escolhe essa paleta pelo byte `primitive+25` e passa seu handle ao binder `6bdc44`. O dispatcher `1a6724` chama a preparação pela virtual `+60` antes do desenho pela virtual `+64`, recebendo o mesmo recurso `O+f0`.

Um oracle executou **43 casos completos de `959d00`, seus helpers reais no perfil declarado e `6bdc44`**, sem stubs ou retornos fabricados. As onze arenas, incluindo bytes opacos, coincidiram exatamente com as equações independentes. O corpus inclui tabelas reais de `em062` e `m_body001`, com poses de referência calculadas pelo parser já existente e perturbações explicitamente sintéticas. O dispatcher e o desenho foram examinados estaticamente; o shader, pesos efetivamente consumidos pela GPU, deformação de normais e projeção não foram executados neste recorte.

Os offsets/endereços abaixo são hexadecimais; os números de casos e tamanhos indicados em bytes são decimais. `O` é o objeto do modelo, `R=*(O+f0)` o recurso MOD, `J[i]=*(O+224)+i*a8`, `C` o contexto de desenho, `G` o gerenciador apontado pelo global `10572e8`. Estes nomes descrevem o uso observado, sem atribuir tipos fonte recuperados.

## Proveniência

ELF privado SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Sessão REA `1792292721003`, Ghidra 12.1.4, ARMv6, heap 3 GiB e Decompiler Switch Analysis desativada. Bundle/snapshot e índices ficam em `research/private/joint-render-binding-v1/`; todas as leituras completas foram comparadas ao ELF. O manifesto fixa relatórios, scripts, recursos e fontes reutilizadas. Nenhum arquivo do runtime, REA ou launcher foi modificado.

| Elo | Evidência |
| --- | --- |
| `959d00`, corpo e assembly | `ev_e0f5d01885b2f8e8ca168966183ede8645fa396558e60f7d12560ddf0e277684`, `ev_be23db2c9ac9ae3704ed78e445b2fe3462fa80eba73a91b26507b71756740343` |
| Seus bytes, incluindo literais | `ev_76c01fe415465a782e119d6622389544d168ebd32b0af5bacb351765f09f3564` |
| `9613b8`, seleção por primitiva e comandos de desenho | `ev_28c828a5a8fe76a7e0bb8667e88f59f257cd532d610d3275cdd8f1dcbfa0f2d4`, `ev_4515a4dee47e4fbf624112d4520638201e2225d91a24bfa56c390d5bce9d3d18` |
| `6bdc44`, binding ao contexto | `ev_0789e15aab503049cf6433b68f9873ab03bfb3256fa0c4c8daff83fcb87d992f`, `ev_53c86d9594aa6945eed676160c78e8bc271ec3fca594a5468f1c90a7cb3e0a52` |
| `1a6724`, ordem das duas virtuais e argumentos | `ev_cb6847be7d0ccb835fa97a6150d9e0e2e8b15c0800a229bf6c55281a5c2be2e3`, `ev_c705d4e7b1ba796fa83cb38c8fc4609bef3fa433e222eac1c8613a7dd420b692` |
| Loader `6806c0`, reaproveitado | sessão `1792283675798`, 066/067; readback112 `ev_051efc9267ab20dc36383e15b13fd8d52421baea5b2a01e43856646479f30ebb` |
| Produtor `960b50`, reaproveitado + complemento | sessão anterior 019/020, readback113; complemento atual043 `ev_96eac62c83675b9ba1369cebd79bc71b5f193d95546ca5ed6eb875729029885d` |

A busca começou por candidatos ARM alinhados `LDR +224`; eles incluem falsos positivos, como literais PC-relative e campos de outros objetos. Os catálogos privados preservam esse limite. O alvo útil foi `959d00`, corroborado pelo encadeamento `O+f0 → R+70`, `R+7c`, `O+224`, `J+8`. Uma consulta de pseudocódigo para `1a46f8` falhou após o provedor não reconhecer função nesse endereço; esse ramo foi descartado, sem importar novamente ou inventar função. A falha está retida e não é ocultada pelo manifesto.

## Tabelas do MOD e associação ao recurso

O loader aceita MOD versão `e6` e lê o header de 128 bytes. Para `N=header.u16(+6)` e `S=header.u32(+28)`, as tabelas observadas são:

| Campo do recurso | Origem no arquivo | Tamanho/stride |
| --- | --- | --- |
| `R+64`, count `R+68` | `S` | N registros de 24 bytes |
| `R+6c` | `S+24*N` | N matrizes de 64 bytes, locais A |
| `R+70` | `S+88*N` | N matrizes de 64 bytes, chamadas B pelo parser |
| `R+108` inline | `S+152*N` | 256 bytes de remap |
| `R+7c`, count `R+84` | `S+152*N+256` | paletas de 28 bytes; count do header+24 |
| `R+74`, count `R+78` | `header.u32(+34)` | primitivas de 48 bytes; count header.u16(+8) |

O loader lê as duas tabelas de matrizes sequencialmente sem invertê-las ou multiplicá-las. A tabela consumida por `959d00` é exatamente `R+70`, corroborando a identidade dos dados `source_skin_matrix` do dono `tools/resources/model_scene.py`. O comportamento não depende de chamá-la inverse bind: a equação abaixo usa os valores originais.

O produtor `960b50` atribui `O+bc4=R+84`, aloca o array de handles `O+bc0` e, para cada paleta, chama `6bf374(G, low16(palette.word0))`. O índice do handle coincide com o índice da paleta. Os registros de paleta têm word de count seguido de índices u8 de joint a partir de `+4`; o produtor de matrizes usa o count estreitado a u16. O parser atual aceita até 24 índices porque o registro tem 28 bytes; o código original examinado não contém essa validação de formato. Não atribuímos ao jogo as verificações de bounds do parser/harness.

`6bf374` pesquisa 64 words de ocupação em `G+22c`, buscando um intervalo de `count` bits dentro de 32 posições. Seu handle contém bank nos bits 0–11, início nos bits 12–23 e count nos bits 24–31; esgotamento retorna `ffffffff`. Os caminhos de mutex e allocation/lifetime não foram executados aqui. Os handles do oracle são parâmetros sintéticos consistentes com esse formato, não resultados de um alocador executado.

## Matriz produzida por `959d00`

O perfil executado usa `O+c50 < 0`, `byte(O+c4b)=0`, `byte(R+214)=0`; portanto não usa histórico de paleta, compartilhamento pelo pai nem o bloco auxiliar de recurso. Esses ramos permanecem visíveis no assembly, mas não estão incluídos na equação operacional admitida pelo oracle.

Para cada paleta `p`, o código obtém um destino de `6be688(G, O.handles[p])`. Se o helper devolve zero, não escreve essa paleta. Para cada índice u8 `j` no registro, lê:

```text
B = float[16] em R.matrix70 + 64*j
W = float[16] em O.joints + a8*j + 8
v = float[3] em C+e0, C+e4, C+e8
```

Interprete **somente para esta fórmula** cada array de 16 floats como quatro linhas consecutivas. A multiplicação usa essa ordem de memória:

```text
T[r,c] = B[r,0]*W[0,c]
T[r,c] = T[r,c] + B[r,1]*W[1,c]
T[r,c] = T[r,c] + B[r,2]*W[2,c]
T[r,c] = T[r,c] + B[r,3]*W[3,c]

output[c*4+r] = T[r,c]                  para c=0..2, r=0..2
output[c*4+3] = T[3,c] + v[c]            para c=0..2
```

São 12 floats/48 bytes por índice de paleta: três linhas de quatro componentes da transposta do produto. Cada multiplicação e soma estreita a float32 separadamente; VMLA ARMv6 não é FMA. O código calcula todas as quatro parcelas, incluindo as componentes homogêneas fornecidas; não pressupõe implicitamente matrizes afins. Não normaliza quaternion, matriz ou escala nesta etapa. O vetor de contexto é adicionado após o produto, apenas nas posições 3/7/11. Sua identidade física (por exemplo, compensação de câmera) permanece desconhecida neste recorte.

Na convenção column-major usada pelo parser glTF, a mesma álgebra corresponde a `W_column * B_column`. Isso é uma conversão de notação, não prova de execução do shader glTF. O parser já observava `G_rest_column * B_column ≈ Q` nos recursos selecionados e exportava `inverseBind = B_column * inverse(Q)`, com posições `Q * posição_raw`. A equação original agora corrobora esse produto fonte. Não há inversão de B em `959d00`; B inclui Q, de modo que copiá-la diretamente como inverse bind para posições já transformadas duplicaria Q.

## Buffers, estado e desenho

`6be688` só retorna destino se handle não for `ffffffff`, `G+32c` não for null e o bit de ocupação de atualização ainda não estiver setado. Define:

```text
bank = handle & 0xfff
slot = (handle >> 12) & 0xfff
address = G.word(+32c) + G.word(+334)*bank + 48*slot
bit = 1 << slot                        (semântica literal de shift ARM)
```

Marca esse bit em `G+128+4*bank` e retorna o endereço. Se o bit já estava setado, retorna zero; o positivo testa essa preservação. `G+334` é stride explícito, não uma constante recuperada de startup. Lock bytes `G+10` e global `1057178` são zero **por entrada declarada**, não por suposição de BSS. A origem/reset por frame desses bits, o banco corrente e a posse/tempo de vida dos buffers continuam fora da execução.

O helper inicial `1a5c10` também executa de verdade: no perfil `C+1bc=0`, reserva espaço já disponível no command buffer, copia `O+154` e atualiza campos do contexto. Os caminhos de material e crescimento de buffer são excluídos por entradas declaradas e controles negativos. `959d00` ainda reduz flags baixas de `O+104` a bit0 quando alguma das seis estava presente. Nenhum desses efeitos foi interceptado.

O dispatcher `1a6724` faz a chamada virtual `+60` em `1a6af8`, antes de `+64` em `1a6d50` ou `1a6ebc` nos caminhos que chegam ao desenho. Nas duas chamadas de `+64`, `LDRD` passa `O+f0` e `O+f4` como recurso e tabela de materiais. A vtable de uPlayer fixa `+60=959d00` e `+64=9613b8`. Isto demonstra ordem local condicionada aos gates desse dispatcher; não prova frequência do scheduler ou que todo frame percorre esse caminho.

Em `9613b8`, joints não vazios usam o caminho de paleta; joints vazios delegam a `1a7198`. O caminho com joints:

- Faz binding inicial de `O.handles[0]` quando o array existe.
- Itera primitivas em `R+74`, count `R+78`, stride de 48 bytes.
- Filtra por campos da primitiva e máscaras do objeto/contexto. O low12 do word+4 indexa bits em `O+108`; o mesmo word fornece material nos bits12–23. Não atribuímos nomes de partes ou políticas de visibilidade a esses valores.
- Dentro da mudança de grupo `primitive.u16(+26)`, se o byte `primitive+25` difere da paleta anterior, chama `6bdc44(G,C,O.handles[byte])`.
- Atualiza material/vertex-buffer/index-buffer e emite comandos de desenho. Esses consumidores foram lidos estaticamente, não executados pelo oracle.

`6bdc44` não transforma matrizes. Calcula o mesmo endereço do handle em `G+32c`; se esse buffer existe, associa-o aos campos `C+438` e `C+43c`. Se `G+330` existe, calcula o endereço correspondente para `C+440`. Os low4 bits armazenam `tag = (high8(handle)+1)>>1`, substituído por zero quando maior que15. Só compara os bits do endereço antes de atualizar, preservando o estado se o endereço já coincide; não inferir semântica do tag. Há fallback para campos `C.word(+10)+450/458/460` quando o endereço calculado é zero. O perfil executado tem `G+330=0`, endereços válidos sem overflow e não alcança fallback.

A fronteira demonstrada é assim: **MOD B + matriz hierárquica → paleta48bytes → handle escolhido por primitiva → campos de binding do contexto → código de emissão de comandos**. Shader, uniform layout final, normalização de pesos, transformação de normais/tangentes e skinning efetivo no hardware ainda exigem evidência própria.

## Recursos reais verificados

| Recurso | SHA-256 do payload | Juntas | B começa | Paletas começam | Primitivas começam |
| --- | --- | ---: | ---: | ---: | ---: |
| `em062.mod` | `2a9e74fe29a24623a0aab96cfd71e918ada70a279355cd232c7799bac1cf2715` | 12 | 1184 | 2208 | 2428 |
| `player/mod/m/pl001/m_body001`, entry44780 | `f63bc643e17f05156f77734e0797ee2e3f135f118fe29bed34e5112a339178a3` | 19 | 1800 | 3272 | 3748 |

Offsets desta tabela são decimais. `em062` tem uma paleta `[1..11]` e duas primitivas com índice 0. `body001` tem uma paleta `[0..18]` e quatro primitivas com índice 0. Os bytes e a associação à ROM são preservados nas proveniências existentes em `assets/local/actor-probe/` e `assets/local/player-probe/body001/`, com ROM SHA `c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`. A ROM inteira não foi re-hasheada nesta etapa; os payloads locais foram.

Com as matrizes de referência calculadas pelo dono existente e vetor de contexto zero, o ARM produziu na primeira entrada da paleta `em062` escala 811,9807739257812 e translação (−230,60704040527344; −142,6981964111328; −544,7486572265625), em três linhas de quatro componentes. Para `body001`, escala 195,46163940429688 e translação (−88,45838928222656; −107,00325012207031; −19,674270629882812). Estes são resultados do oracle sobre pose offline declarada, não uma captura do jogo em execução integral.

## Validação e reprodução

`research/private/joint-render-binding-v1/verify_palette.py` executa em Unicorn2.1.4/ARM11MPCORE somente bytes originais admitidos, com leitura inicializada, escrita delimitada, destinos de código e orçamento guardados. O snapshot canônico é `arm-second/`: relatório SHA-256 `9047959cfd43457a4f732ddc40010b20731f52d96c6d7ccace8267857ab887f5`. Ele inclui os casos, resultados, hashes de arenas, readbacks e traces completos comprimidos. Equações Python independentes, com estreitamento float32 por operação, comparam todos os bytes das onze arenas; não apenas os 12 floats visíveis.

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  research/private/joint-render-binding-v1/verify_palette.py \
  --output research/private/joint-render-binding-v1/arm-replay
```

A saída deve ser nova. O ELF e os dois payloads privados são necessários; não requer Ghidra para repetir. O relatório registra os hashes dos donos de formato e do ELF; o retentor verifica esses pins antes de gerar o manifesto. Não distribui ou executa a ROM como engine.

Foram 43 casos, 47.807 instruções, 15.113 leituras, 6.706 escritas e **7.925.760 bytes** comparados exatamente. Incluem zero joints/zero paletas, 1/2/8/24/256 joints de diagnóstico, índices 255, ordem reversa de paleta, matrizes gerais não afins, quaternions não unitários, escala negativa/zero, dois vetores de contexto, buffers ausentes, bits já setados, flags e tamanho de descriptor. O caso sintético de 256 joints testa o índice255; não é afirmado alcançável pelo produtor original que estreita `O+220` a u8. Os casos reais têm 12 e 19 joints.

Seis controles negativos alcançaram guardas de mutex, crescimento do command buffer, histórico de paleta, índice não inicializado, global não inicializado e orçamento. São rejeições do harness; não verificações atribuídas ao jogo. `arm-first` retém o diagnóstico inicial de configuração do SP antes de trocar CPSR, corrigido no harness antes do run canônico; nenhuma instrução do alvo foi alterada. FPSCR começa zero por chamada, resultados finitos são comparados e flags de exceção não são objeto de equivalência.

O run canônico mediu 0,130 s de parede, 0,128 s de CPU e pico de RSS do processo de 54.034.432 bytes em macOS; não mede a família inteira ou tokens. Não há ensaio Windows nem implementação C++ nesta entrega. O manifesto registra o custo de importação REA separadamente.

## Contrato para próxima implementação

A menor função numérica nativa pode receber arrays imutáveis B e W na ordem fonte, uma paleta de índices, vetor de contexto explícito e destino de 12 floats por índice. Deve preservar a ordem float32 de operações e validar os limites na fronteira do novo motor. A camada de estado deve possuir separadamente handles/banks/dirty bits e binding ao contexto; não ocultar esses dados em defaults ou assumir que são resetados a cada chamada.

Esse contrato admite o ramo normal sem histórico/compartilhamento. Os ramos com `O+c50>=0`, `O+c4b`/pai, `R+214`, locks, crescimento de buffer e fallback continuam fora do perfil executável. O caminho compartilhado pelo pai lê o byte alto de `J+94`, remapeia via parent+228 e pode usar template `10e6440`; não deve ser silenciosamente substituído pelo ramo normal.

Para conectar efetivamente ao renderer, a próxima pergunta estreita é o consumo de `C+438/+43c/+440` e dos 12 floats pelo shader selecionado para o vertex format real. A álgebra `W*B` já é corroborada; a aplicação de pesos, normais e convenções de projeção permanece uma fronteira explícita.
