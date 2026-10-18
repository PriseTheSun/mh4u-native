# Vista, projeção e deslocamento do contexto

O produtor `0x5dfe80` compõe **vista × projeção**, em arrays de 16 floats organizados por linhas. O envio transpõe esse array; o shader `VS_MaterialStdObj.asm` consome suas colunas. A equação resultante usa vetor-linha: `p × View × Projection`. O deslocamento `C+0xe0..0xe8` é somado à posição antes dessa transformação e já aparece na translação das paletas investigadas. Não há uma segunda aplicação de `source_to_model` nesse trecho.

Esta investigação fecha o contrato de dados entre produtor, contexto e consumidores. Não recupera o controlador de câmera, a política de estéreo ou um frame completo. Os ensaios executam somente código ARM original isolado, com entradas e memória sintéticas declaradas; não executam GPU nem fazem parte do motor entregue.

## Fonte e evidência

- ELF local: `mh4u-analysis-bss.elf`, SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
- MFX local: `LtShaderPackage.lfx`, SHA-256 `4403b94b6340acd9f654a5c821699661dcbb0a9dd6a8d9535ee73375ac97e6b6`. Os descritores, símbolos e programa32 são os já fixados em [joint-shader-consumption.md](joint-shader-consumption.md).
- Sessão REA/Ghidra `1792295977777`, perfil ARMv6 focado, heap3GiB. Exportação `064-export_evidence_bundle.json`, encerramento `065-close_binary.json`: **61 Evidence**. Bundle, snapshot, resultados completos e índices de bytes estão em `research/private/model-view-projection-v1/`.
- Nenhum módulo nativo, parser público, viewer, launcher ou fonte do REA foi alterado. A identidade completa do launcher23513 foi comparada antes/depois.

Os registros centrais são: produtor `005/013/014` (`ev_9f86eb690c6523062b1a44618c96d4ce3883b8e6d5ec385e8ac4bf5b0b4ad4c5` e `ev_e6754ce372a6e2f4e2c19722c42545d9db16d8112eda71fb041c9653945407f9`); serializador `006/021`; projeção CPU `029/043/052`; inicializador do contexto `034/061/062`; origem dos argumentos `010/018/019/033/042/058`.

## Produtor e armazenamento

Em `0x5dfe80`, `r0=C`, `r1=A`, `r2=B`, `r3=viewport`. Os nomes `fView` e `fProj` vêm dos descritores MFX, não da aparência das matrizes. `inspect_bindings.py` preserva os sete words de cada registro e verifica nomes, contagens, offsets e bounds, usando o dono MFX anterior.

| Ponteiro/contexto | Payload | Nome da fonte |
|---|---|---|
| `C+0x410` | `A × B`, 64 bytes | `CBViewProjection.fViewProj` |
| `C+0x420`, payload+`0x00` | A, cópia dos 16 floats | `CBViewProjectionEx.fView` |
| payload+`0x40` | transposta da parte3×3 e translação inversa rígida | `fViewI` |
| payload+`0x80` | B, cópia dos 16 floats | `fProj` |
| payload+`0xc0` | inversa geral de B | `fProjI` |
| payload+`0x100` | inversa geral de A×B | `fViewProjI` |

Para arrays por linhas, o produto é:

```
VP[4*r+c] = (((A[4*r] * B[c]
              + A[4*r+1] * B[4+c])
              + A[4*r+2] * B[8+c])
              + A[4*r+3] * B[12+c])
```

O assembly faz MUL e três VMLA de precisão simples. No ARMv6/VFP2 ensaiado, cada multiplicação e soma arredonda para binary32, sem FMA. A inversa rígida de A usa `Rᵀ` e `−tRᵀ`; ela só é uma inversa matemática quando o3×3 de A é ortonormal. Não se deve substituir essa operação por uma inversa geral sem uma mudança de contrato explícita. Inversas gerais com determinante zero copiam o template em `0x10e6440`; o ensaio usa um template identidade **sintético declarado**, sem deduzi-lo de BSS zerado.

O helper real `0x5e23c0` compara/copia quatro words de viewport para `C+0x12c..0x138` e marca `C+0x128 |=8` quando necessário. O produtor obtém tamanhos dos descritores, copia seus defaults e escreve os seis arrays. O perfil executado fornece arena suficiente; alocação por `0xbf8274` é rejeitada pelo guard.

## Origem dos argumentos: fronteira recuperada

`0xaf01fc` fornece as matrizes ao produtor em `0xaf1740`. No ramo que copia o par atual, A vem do objeto+`0x98..0xd7` e B de +`0xd8..0x117`; viewport vem de +`0x24..0x33`. A função também possui blend com outro par (+`0x118`/+`0x158`) e ajustes condicionais. Esses ramos foram retidos estaticamente, mas não compõem o perfil executado nem autorizam uma política genérica de câmera. O helper anterior `0xaef6f4` trata o viewport, não prova a produção dessas duas matrizes.

`0x624c98` salva a vista/projeção já existentes no contexto em objeto+`0xd8`/+`0x118`; `0x6249d0` restaura esse par chamando `0x5dfe80`. Portanto esse wrapper é uma restauração, não a origem principal da câmera. `0x624b7c` usa um par alternativo +`0x168`/+`0x1a8`. O caminho `0xb6e87c`, que contém a string `ShadowCast`, recebe suas matrizes do chamador. Não se presume que todos esses caminhos usem a câmera principal.

O construtor de projeção `0xb710cc` foi retido como testemunha estática, não como prova de seleção para em062/body001. Para argumentos VFP `(a,b,c,d)`, chama duas vezes `H=0xd14118(c/2)` e escreve `P00=(1/H)/d`, `P11=1/H`, `P22=−b/(b−a)`, `P23=−1`, `P32=−ab/(b−a)`, `P33=0`, copiando os demais elementos do template global. O nome matemático de H e a seleção desse construtor não foram necessários para o contrato e não foram promovidos a fatos executados. Condicionalmente a um template afim usual e `0<a<b`, essa álgebra dá `z/w=0` em `z=−a` e `z/w=1` em `z=−b`, antes da negação do shader. Isso não determina sozinho clipping, depth-test, viewport ou near/far da câmera ativa.

## Envio aos registradores e posição

`0x5e2d1c` cria comando de classe11/subtipo5, com `cmd+8=C410`; `0x5e4200` também guarda esse ponteiro num comando da classe11, subtipo3. O dispatcher derivado dos bytes em `0xbfa19c` usa a classe em bits24..27 e o subtipo em `cmd+4`. Para subtipo5, a tabela em `0xbfac0c` leva a `0xbfb0b8`; depois do helper de estado `0xbf6008`, o bloco `0xbfb0f0` compara `cmd+8` ao cache `D+c14` e envia os16 floats se mudou. Essa cadeia é estática, não uma execução completa do dispatcher.

O prefixo original `0xbf8d88..0xbf8e64`, usado por outros ramos desse mesmo dispatcher, executou isolado. Ele envia:

1. configuração `0x80000000`, header `0x000f02c0`;
2. payload16 floats na ordem `M[0],M[4],M[8],M[12],M[1],…`, header `0x00ff02c1`;
3. configuração `0x80000004` e quatro floats adicionais, header `0x003f02c1`.

Cada comando preserva seu word de alinhamento. Submissão posterior `0xd81d08` não foi executada. O envio de `fView` para c78..81 usa a mesma transposição4×4 no uploader de uniformes já verificado na etapa anterior, com ponteiro `C420`, offset0 e quatro vetores.

A combinação do envio e das instruções do VS18, retidas anteriormente, é:

```
p = (position_after_palette_and_context, 1)
v = p * View * Projection
clip = (v.x, v.y, -v.z, v.w)
```

A negação de z pertence a esse shader. Não deve ser convertida em uma regra global de câmera nem aplicada automaticamente a uma projeção de laboratório com outra convenção. Para uma API de vetores-coluna, a matriz equivalente à composição por linhas é `(View*Projection)ᵀ = Projectionᵀ*Viewᵀ`. Essa conversão de representação não adiciona outra transformação de modelo.

## Vetor do contexto e consumidor CPU

`0x5e440c` copia `0x10e6370/74/78` para `C+e0/e4/e8` e escreve zero em `C+ec`. A inicialização original desses três globals como zero já foi executada em `root-motion-defaults/isolated-run-v2.json`. Isso prova o valor inicial sob aquele startup; não prova imutabilidade do global ou do contexto depois. A busca local neste recorte não recuperou um setter posterior nem uma relação desse vetor com posição da câmera.

O consumidor completo `0x5e3018` confirma a posição dessa soma, independentemente da paleta:

```
q = (input.xyz + C[e0:e8], 1)
h = q * VP
out.x = (h.x/h.w + 1) * width/2
out.y = (1 - h.y/h.w) * height/2
out.z = h.z/h.w
out.w = h.w
```

`width=C134−C12c`, `height=C138−C130`. Esse helper não soma o início do viewport à saída. Se `*(G+6c28) !=0` e `*(G+6ba4)&8`, substitui `out.x` por `width−out.x`. Os globals e flags são entradas explícitas do ensaio. Ele não aplica a negação de z do VS. Não se confunde essa saída CPU com clip-space do shader.

Em `0x959d00`, a soma E já entra nas translações da paleta `W×B`, conforme [joint-render-binding.md](joint-render-binding.md). Um renderer que consome essa paleta aplica E **uma vez**. `B` já contém a transformação fonte→modelo observada; reaplicar Q aos vértices ou após skinning duplicaria essa transformação. A vista é aplicada à normal depois da parte linear da paleta, sem translação, como já descrito para VS18; este recorte não acrescenta iluminação ou inverse-transpose.

## Ensaios e limites

| Perfil original | Casos | Verificação |
|---|---:|---|
| Prefixo `5dfe80→5e0948`, helpers viewport/cópia reais |48|A×B, A, inversa rígida e B bit a bit; inversas gerais contra Gauss-Jordan independente|
| Prefixo de envio `bf8d88→bf8e64` |47|sequência de words, transposição, padding e estado bit a bit|
| Função CPU `5e3018` inteira |188|posição, offset e quatro combinações dos gates de mirror; referência independente binary64|

Os três perfis somam105.780 instruções e9.273.344 bytes de estado comparados, mais13 controles negativos de memória/bounds/orçamento/alocação. Nenhum helper foi interceptado. Erro máximo das inversas gerais: `5.167722991750168e-7`, tolerância `3e-5*max(1,maxabs(referência))`. Erro absoluto máximo da projeção CPU: `0.0008042279423534637`, tolerância `2e-5*max(1,maxabs(saída))`. Isso não é equivalência bit a bit das inversas/projeção CPU com o cálculo binary64 independente.

`verify_matrices.py --output <novo-diretório>` e `verify_consumers.py --output <novo-diretório>` usam o Python/Unicorn2.1.4 já retido em `research/private/root-motion-runtime/venv/bin/python`. `inspect_bindings.py --output <novo-json>` só lê o MFX local. Saídas congeladas: `matrices-first/`, `consumers-first/`, `mfx-camera-bindings-verified.json`. Relatórios incluem hashes, ranges, guards, traces comprimidos, tempos e RSS do processo. Os ensaios mediram aproximadamente0,20s e0,10s; não incluem importação Ghidra. CPU/pico da família REA e tokens totais não foram medidos; contagens de consultas e latências ficam no índice privado.

A sessão registra a tentativa de pseudocódigo de `FUN_005e43f8` que falhou por alvo incorreto, corrigido após resolução para `FUN_005e440c`. As resoluções de `bfb150/bfa000` retornaram `not_in_procedure`; a cadeia do dispatcher foi derivada de bytes locais e LLVM, sem modificar a análise. Leituras iniciais parciais de `5e3018` e dos literais de `b710cc` foram complementadas e estão explicitadas no índice de cobertura.

Permanecem fora desta entrega: produtores dinâmicos dos pares de câmera do objeto `af01fc`, blend/estéreo ativos, setter/mutabilidade de E, valores originais de FOV/planos da câmera ativa, seleção desse construtor de projeção por cada passe, comandos GPU/depth/clip completos e precisão PICA float24. O próximo elo, se necessário, é o writer das matrizes do objeto+`0x98/+0xd8` consumido por `af01fc`; não é necessário expandir iluminação para usar o contrato atual.
