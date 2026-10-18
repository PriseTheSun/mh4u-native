# Consumo da paleta pelo shader

O ramo normal de `nDraw::MaterialStd` liga os MODs selecionados ao programa **32** de `LtShaderPackage.lfx`, blob **18**, cujo nome interno é `VS_MaterialStdObj.asm`. A cadeia foi recuperada estaticamente do executável e dos recursos. O loop ARM que escreve seus uniforms foi executado isoladamente em **368 casos**, com **9.043.968 bytes comparados exatamente** e quatro controles negativos. Isso não é execução do desenho completo nem da GPU.

Este recorte fecha o formato dos índices/pesos, a ordem dos 12 floats e a álgebra de posição/normal desse VS. A construção das matrizes de câmera/projeção, precisão aritmética PICA, iluminação final e seleção de outros passes permanecem fora da prova. Nenhum parser, runtime, viewer, pacote ou launcher foi alterado.

## Fonte e evidência

O ELF fixado tem SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. A sessão REA `1792294137778` usou Ghidra 12.1.4, ARMv6, heap3GiB e perfil focused com Decompiler Switch Analysis desativada. Bundle/snapshot e manifesto estão em `research/private/joint-shader-consumption-v1/`; export093 e close094 terminaram com sucesso. Readbacks completos são comparados ao ELF pelo retentor local.

| Elo | Registros/Evidence da sessão |
| --- | --- |
| Loader MFX `be59d4` | 024/028/029; `ev_b02cc4970fcefebbb003c9f15bdca65495884a90fe79b57547860b9b0a242bef`, bytes `ev_b55d6b13da3387869ec06cfe8d224f1939afa906166a90b0cc6e0a635c5653cf` |
| Factory MaterialStd e técnica | 050/051/055/090; factory `ev_73b6d31cbee4e112fbc7ca474640f64d6ea95311b74a509e53a8ea43b2c3f871` |
| Seleção `6365f0 → 638260 → 5e2c74 → bfc55c` | 053/058/060/063, readbacks065/068/069/081; `ev_55b096272c4da3b7bc333d76c107ac3900475522934c03f0efadcae659d0b91e` |
| Cache de comandos `5e4e60` | 010/011/017; `ev_79aa5b94ea0aca24ddaa8bd55761335a706264fe368fceb399149253f6d648b9` |
| Uniform uploader `bf6c60` | 046/047/049; `ev_6074cd4920a19ae4a9413f06d26387c9c1e4bfd2819c8302164048637113a73e`, bytes `ev_b79500b678ca22a61359f5c520d7087274d2ea1892560c8c0e2f6dc3a198b319` |
| Construtor InputLayout `62983c`, helper `628c20`, tabela | 076/079/080/082/084/085; `ev_c7c32bf8724d9f1735ea57e355286543b915534759d83b8f72f547971c6278e1`, `ev_fe4a1fa941738330f8e77beaa384b46211f60a867270ca306e243edaa32adf01` |

Reutiliza também `joint-render-binding-v1`: `9613b8` e seu readback, `1a4904`, loader MOD `6806c0` da sessão1792283675798. Ghidra não inclui todo o switch de `628c20` no conjunto de ranges do corpo; o readback085 cobre o intervalo fonte `[628c20,628ee4)`, incluindo os destinos/tabelas interpretados. O recorte não infere completude a partir do tamanho de função reportado.

O recurso `/system/ltshader/LtShaderPackage.lfx` foi lido na ROM no offset decimal2692424960, tamanho318300, SHA-256 `4403b94b6340acd9f654a5c821699661dcbb0a9dd6a8d9535ee73375ac97e6b6`. A identidade integral da ROM foi reutilizada da inspeção anterior, não recalculada. O payload foi verificado novamente; a proveniência conserva caminho, range e digest.

## Seleção do shader e limites do caminho

Os MRL de em062 e m_body001 contêm classe `5fb0ebe4`, JAMCRC32 de `nDraw::MaterialStd`. A factory `cf9cec` chama `638b90` com `25fcf2f0`, que corresponde ao descriptor **TMaterialStd**, ID752. O descriptor tem programa-base31 e dois passes.

No desenho com joints, `9615c0..9615dc` fixa `(C+188 bits3..10)=1` e marca dirtybit0. O método `6365f0` recebe o contexto e, no ramo normal permitido pelas flags de desenho, passa esse valor a `638260`. Este preserva r2 até `5e2c74`, que chama `bfc55c`. O getter lê `resource.programs[technique.word(+14)+pass]`: **31+1=32**. O programa32 aponta blob18, com SHA-256 `89e6141929dd866282b0c42b066330ccf6fce97f54953001f58667fac3441383`.

Esta é uma seleção estática condicionada aos gates do ramo normal. `6365f0` também possui caminhos alternativos `63803c`/`638808` e retornos sem desenho. Não foi observado um frame live para provar qual conjunto de flags é usado em cada situação. Não se atribui VS18 a todos os passes, sombras, materiais ou partes visíveis.

O campo fonte `primitive+14` é hash de **input layout**, não hash de programa VS. O loader remapeia esse hash por `bfc42c`; `1a4904` resolve o handle no contexto e grava o layout em `C+184`. O loader também substitui `primitive+26` por um contador de grupos construído a partir de comparações de layout, buffers, material, campos compactados e paleta; o valor no arquivo não deve ser usado como identidade do grupo carregado.

## MFX e layouts efetivos

A variante observada começa `MFX\0`, versão0x34, plataforma100. Header+0c conta796 descriptors; +10 conta167 programas; +14 conta110 blobs; +18/+1c/+20/+24 apontam tabela de programas, registros de blobs, strings e início dos blobs. O array de descriptors começa **+28**, incluindo slot0 null. A tabela de blobs é não alinhada (offset0x2845a), stride24; o parser usa leituras formatadas, não casts. Todos110 ranges foram checados, mas somente o VS18 foi semanticamente examinado.

| Fonte | Hash de layout | Descriptor | Stride | Vértices examinados |
| --- | --- | --- | ---: | ---: |
| em062 primitiva0 | 01b36016 | 22 IAObj4wt | 36 | 360 |
| em062 primitiva1 | f606f017 | 23 IAObj4wtT | 36 | 1516 |
| body001 primitivas0..3 | 01b36016 | 22 IAObj4wt | 36 | 1280 |
| one133, testemunho já disponível | d6784014 | 20 IAObj2wt | 28 | 503 |

Os dois MODs alvo só possuem stride36. O contrato de28 tem testemunho separado em one133; não é atribuído a body001 por suposição. SHA MODs e MRLs, hashes fonte, campos desconhecidos+26, histogramas e bounds ficam em `contract-second/source-models.json`.

`62983c` agrupa bytes pelos descriptors; `628c20` converte tipos1→float32,7/9→signedbyte,8/10/13→unsignedbyte. Depois o construtor produz códigos GPU0/1/2/3 para byte/u8/i16/float. Para estes layouts:

| Offset fonte decimal | Conteúdo | Entrada VS | Conversão na aquisição |
| --- | --- | --- | --- |
| 0..11 | posição XYZ float32 | v0.xyz | float |
| 12..15 | normalXYZ + oclusão/alpha | v1.xyzw | quatro signedbytes, sem divisão por127 |
| 16..23 | UV float32 | v2.xy | float |
| 24..27 | índices0,1; pesos0,1 | v3.xyzw | quatro unsignedbytes, sem normalização |
| 28..31, somente36 | Reserved ou TangentXYZ/BiNormal | v4.xyzw | quatro signedbytes |
| 32..35, somente36 | índices2,3; pesos2,3 | v5.xyzw | quatro unsignedbytes |

O bytecode usa `.xy` como índices e `.zw` como pesos. `62983c` reconhece o nome `Joint` e a contagem/base do campo; define flag0x800 quando há mais de2 componentes e0x1000 quando mais de4. IAObj4wt(T) resulta em b11=true, b12=false. O uploader escreve flags de layout no byte1 do registrador booleano (header0x202b0); a escrita de booleans do material usa máscara0xd, preservando esse byte. IAObj2wt não ativa b11. Não se transfere a flag compactada do MOD para um booleano de shader sem passar por esse layout.

Todos os 3659 vértices dos três testemunhos somam255 nos pesos fonte. Até os índices de peso zero ficam dentro das respectivas paletas. O jogo faz os acessos de matriz antes da soma, sem pular peso zero nesse VS; um futuro consumidor não deve usar peso zero como justificativa para esconder índice inválido.

## Binding e upload dos 12 floats

Descriptors138/139/140 são `CBJointMatrix`, `CBJointMatrixSB`, `CBJointMatrixPF`, correspondendo a `C+438/+43c/+440`. O programa32 usa somente138; a leitura anterior do binder não demonstra uso dos outros dois por este VS.

`5e4e60` copia os ponteiros do contexto para a lista do comando segundo `program+40`, count`+44`. No programa32, `CBJointMatrix` é slot1. Seu word de mapa de uniform é **0x09018001**:

```text
binding slot = word & 31                           = 1
source float offset = (word >> 5) & 511            = 0
target vector register = (word >> 14) & 127        = 6
vector count cap = (word >> 21) & 127              = 72
```

`bf6c60` lê esse slot. Se low4 do ponteiro é não zero, separa endereço`ptr&~15` e tag. O binder anterior produz `tag=ceil(joint_count/2)`. São enviados `min(tag*6,72)` vetores, quatro words cada, em blocos de até128 words. Emite0x80000006/header0xf02c0, seguido de data/header0xf02c1. A ordem dos floats da paleta é preservada, sem transposição nesse ramo. Cada bloco deixa uma word de alinhamento intacta e avança `dataWords+2`.

**Paletas ímpares exigem padding real:** 11 joints produzem132 floats, mas tag6 envia144;19 produzem228, mas tag10 envia240. O oracle forneceu explicitamente a entrada extra; um controle negativo sem essa região inicializada foi rejeitado. Não foi recuperado neste recorte como allocation/reset original inicializa essa entrada. Não usar zero implícito. Nos recursos examinados nenhum vértice referencia o joint extra.

O cache compara ponteiros, não conteúdo. Se o slot coincide com o comando anterior, pula a escrita; política de dirty/reset e dupla memória não foi reconstruída. Tag0 usa outro caminho: count4 transpõe4x4; outros counts copiam em sequência. Os testes incluem esses caminhos para evitar generalizar a regra de paleta ao upload de view/projection.

PICA recebe vetores na ordem de memória w,z,y,x. O VS usa `.wzyx` nos uniforms de matriz, portanto recupera exatamente cada linha de quatro floats emitida por `959d00`. Essa convenção é corroborada por fontes primárias públicas de citro3d: [C3D_FVec](https://github.com/devkitPro/citro3d/blob/9f21cf7b380ce6f9e01a0420f19f0763e5443ca7/include/c3d/types.h) e [upload direto de uniforms](https://github.com/devkitPro/citro3d/blob/9f21cf7b380ce6f9e01a0420f19f0763e5443ca7/source/uniforms.c). Não é medição em hardware.

## Álgebra derivada do VS18

A disassembly do blob e a decodificação adicional de MAD/IFU estão retidas. `nihstro` não imprime os operandos desses dois opcodes no seu executável; `inspect_contract.py` lê os bitfields/swizzles originais, sem preencher lacunas por nomes. A referência é [nihstro a969b1a](https://github.com/neobrain/nihstro/tree/a969b1a6849b02679a1858847ba18bedd3a19d44), BSD3, conservada privada com licença.

Sejam `P[j][r]` as três linhas de quatro floats da paleta, índices u8 `j_i`, pesos u8 `w_i` e:

```text
k = float24 literal 0x370100 = 0.0039215087890625
row[r] = (k*w0)*P[j0][r]
row[r] = row[r] + (k*w1)*P[j1][r]
se b11: acrescenta (k*w2)*P[j2][r], depois (k*w3)*P[j3][r]
```

O shader multiplica os índices por3 para endereçar c6+3*j, c7+3*j, c8+3*j. O primeiro produto usa MUL, parcelas seguintes MAD na ordem acima. Essa sequência é comprovada em0x00..0x5c do programa. Não há divisão pela soma dos pesos. Para soma255, o fator ideal é **0.9999847412109375**, distinto de1. A semântica de arredondamento/fusão do MAD e float24 em hardware ainda não foi executada/medida; as equações são algébricas, não promessa bit a bit.

Para o perfil sem morph (b14=false), a posição usa os floats **raw** do VBO:

```text
p.xyz = [dot4(row[0], [rawPosition,1]), ... row[2]]
p.w = 1
normalSkinned.xyz = [dot3(row[0].xyz, signedNormal), ... row[2]]
normalView.xyz = [dot3(viewRow[0].xyz, normalSkinned), ... viewRow[2]]
normalUnit = normalView * rsq(dot3(normalView, normalView))
clip = [dot4(viewProjRow0,p), dot4(viewProjRow1,p),
        -dot4(viewProjRow2,p), dot4(viewProjRow3,p)]
```

Essas operações ocupam0xa8..0xfc; a projeção c0..c3 está em0xd0..0xdc. View usa c78..c80, sem translação na normal. A normal não usa inverse-transpose no programa examinado. Normal nula e não finitos não possuem política de fallback demonstrada aqui.

O input v4, inclusive Tangent/BiNormal do layout4wtT, **não é lido por VS18**. Isso não prova que tangentes são descartadas por todos os programas. O VS transforma a normal unitária em quaternion para saída de iluminação; iluminação/fragmento e transformações de UV não foram portados ou certificados por este recorte.

A matriz fonte B já inclui a conversão Q do MOD. `959d00` produz `W*B`, acrescido do vetor explícito do contexto; o VS aplica isso à posição raw. Para usar posições previamente transformadas porQ, a equivalência algébrica exige `W*B*inverse(Q)` e os mesmos pesos. Aplicar novamenteQ antes de `W*B` duplica essa conversão. A projeção vem **depois** do skinning. O sinal negativo de clip.z faz parte desse VS; não deve ser copiado para uma projeção OpenGL/Metal escolhida pelo novo renderer sem demonstrar as convenções da matriz original.

O parser glTF atual divide pesos por255 e normaliza normais. Não foi modificado. Sob a relação ideal de referência `W*B=Q`, só a diferença de escala dos pesos gera diferenças máximas por componente de0,0083122(em062),0,00163274(body001),0,000930236(one133) nas unidades fonte convertidas. São comparações algébricas em binary64 sobre dados reais, não erro medido da GPU, nem limite global para qualquer pose.

## Validação, custo e fronteira restante

`verify_upload.py --output <diretório-novo>` roda com o Python privado de `root-motion-runtime/venv`. Executa somente `[bf6ecc,bf7108)` e memcpy`[10eb24,10eb98)` originais, com estado declarado no início do loop, sem stubs. Guardas delimitam código, memória lida inicializada, escrita e orçamento. `upload-second/validation.json` é o run canônico:368 casos,47.434 instruções,9.043.968 bytes exatos; tags1..15, caps3/4/6/32/33/60/72/90, cache igual/diferente, tag0, transposição e offsets. Quatro negativos cobrem padding, fonte, destino e orçamento.

Medido:0,312s parede,0,303s CPU,pico de RSS do processo57.688.064bytes. A importação fria REA durou144,394s; tempo/memória da família REA e tokens não foram medidos. Não houve execução Windows/hardware. O script de contrato checou os796 descriptors e110 bounds de blobs, um programa semanticamente e3659vértices, com três negativos de magic/truncamento/offset. Isto não é cobertura semântica de110 shaders.

Os primeiros ensaios estão preservados: falha de compilação por especialização libc++ no nihstro, erro de transporte rejeitado antes do provedor, oracle esperado que omitia a word de padding, e sentinel de nome ffffffff que acionou bounds. `incidents.json` registra também a consulta duplicada e o primeiro bridge sem TTY que encerrou antes de abrir binário. Nenhum desses incidentes foi resolvido alterando bytes do alvo.

O contrato para integração pode receber posição/normal raw, índices/pesos originais, paleta3x4, view e viewProjection explícitos, mantendo Q apenas na paleta. Falta fechar, antes de afirmar fidelidade visual completa:

- produtor e ordem das matrizes c0..3, c78..81 e do vetor `C+e0..e8`;
- callback indireto que despacha `bf6c60` e tempo de vida/invalidade dos caches;
- precisão PICA de aquisição/float24/MAD/RSQ, incluindo degenerados;
- padding de paletas ímpares, outros passes e programas SB/PF;
- material de fragmento, visibilidade original e detalhes de iluminação.

A próxima pergunta menor é localizar o comando que escreve **c0..3** (uniform global fViewProj) e ligá-lo à câmera/contexto, sem ampliar novamente para todo o pipeline GPU.
