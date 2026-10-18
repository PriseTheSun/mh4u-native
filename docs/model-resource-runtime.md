# Leitura nativa das tabelas MOD

`model_resource::Resource` carrega um MOD230 por SHA-256 esperado e fornece as
tabelas imutáveis necessárias à animação e à preparação das paletas. Os dados
continuam na ordem e nos bits do arquivo: não calcula inverse bind, não troca
convenções de matriz e não normaliza valores numéricos. A implementação fica
em `src/model_resource.hpp/.cpp`.

A fonte do layout é o loader original `6806c0`, preservado na sessão REA
`1792283675798`, registros `066/067/112`, e a
[ligação de paletas](joint-render-binding.md). O ELF é
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Leitura de campos é uma derivação do código; não foi executado o loader inteiro
com seus alocadores, streams, shaders e objetos gráficos.

## Dono e dados expostos

O snapshot possui os bytes e as matrizes interpretadas uma única vez. Cópias de
`Resource` compartilham o mesmo armazenamento e identidade; uma decodificação
independente recebe uma identidade nova, nunca reutilizada no processo. SHA-256
identifica o conteúdo. Views emprestadas exigem que uma cópia do dono continue viva.

Com joints presentes, `skeleton()` fornece matrizes A e B, os registros de 24 bytes
dos joints, remap de 256 bytes e paletas. Cada paleta preserva o word completo do
descriptor, offset fonte e os índices definidos pelo low16. Os outros bytes
continuam disponíveis no snapshot. O limite de 24 índices deriva do registro
de 28 bytes e é uma validação do novo leitor, não uma verificação atribuída ao jogo.
Índices que não apontam para um joint são preservados; o consumidor valida apenas
as paletas efetivamente usadas. Bits NaN, infinitos e zeros assinados também são
preservados na leitura, ficando o domínio aritmético com o consumidor.

Quando o header declara zero joints, `6806c0` não lê a seção de esqueleto/remap/
paletas. A API devolve `nullopt` para o esqueleto e mantém o count declarado de
paletas e os offsets fonte, sem fabricar uma tabela inicializada. O orçamento do
count declarado ainda é aplicado porque o original solicita sua alocação antes
desse gate. Tabelas de primitivas vazias não consomem seu offset.

`primitives()` conserva o hash fonte do input layout, flags, seletores e tanto o grupo
fonte quanto o grupo produzido pelo loader. Não afirma que o hash já virou um
handle de input layout. O campo +0x14 identifica a disposição dos vértices,
não um programa VS; a cadeia foi fechada em [consumo pelo shader](joint-shader-consumption.md). Cada registro fonte de 48 bytes pode ser consultado sem alteração.

## Grupos produzidos pelo loader

O trecho `680ae0..680bd4` atribui o grupo 1 ao primeiro primitivo e incrementa o
grupo quando o seguinte muda algum destes campos: word+14, word+10, material
`(word+4 >> 12)&fff`, `(word+8 >> 24)&3f`, byte+25, `(word+8 >> 16)&ff` ou
`word+0 &104`. Os offsets e máscaras desta frase são hexadecimais. O grupo
carregado sobrescreve um halfword em +26; não vem do valor desse campo no arquivo.
Ele é calculado antes da relocação do hash de input layout.

Essa distinção pertence ao leitor nativo. O renderer recebe `loaded_group` e
`palette_index`, sem reinterpretar o registro ou repetir o algoritmo. O arquivo
original permanece intacto, inclusive o grupo fonte diferente.

## Validação e limites

O oracle privado `research/private/model-resource-v1/verify_source.py` executa
somente o bloco original dos grupos, sem substituir instruções. São **31 casos,
1.532 primitivas e 64.515 instruções**, incluindo dois recursos reais, mudanças
isoladas dos campos relevantes, diferenças ignoradas e grupos repetidos. As
escritas são limitadas aos halfwords de grupo; os demais bytes e o objeto do
modelo permanecem intactos. Três controles negativos verificam leitura não
declarada, escrita proibida e orçamento de instruções.

As matrizes/paletas reais de `em062` e `m_body001` são comparadas com o dono Python
existente, separadamente da execução ARM. O formato fonte é independente do novo
leitor C++; o oracle não contém valores esperados obtidos dele. O arquivo
`source-final/source-facts.bin` tem SHA-256
`de5f0e3faa5a6ef440a6b7182ff702fca10ebb25ee2fadb9ef0704e91224b061`.

O teste público cobre 310 verificações de fronteira e 5.752 comparações de fatos
fonte/grupos. O produtor de validação exige as contagens e saída de erro vazia,
usa ASan/UBSan com interrupção em qualquer diagnóstico e rejeita corrupções tanto
de identidade quanto de conteúdo esperado com hash atualizado. Os relatórios
retidos declaram quais variantes efetivamente passaram.

Os limites de bytes, joints, paletas e primitivas são políticas ajustáveis de
alocação. Cálculos de regiões usam largura ampliada e validam o arquivo antes da
alocação das tabelas. O módulo não é um loader completo: materiais, vertex/index
buffers, shaders, semântica dos registros dos joints, bounds de renderização e
estado auxiliar `R+214` ainda precisam dos respectivos consumidores/donos. Ele
não infere pose de gameplay nem presença de ponteiros a partir de bytes omitidos.
