# Consulta dos prefixos de eventos

O consumidor original `0x94be10` consulta a tabela de 32 valores u16 do grupo 0
usando o menor bit ativo da máscara atual. Isso confirma uma transformação
numérica bit → valor do prefixo em MH4U. Não identifica ataque, som, efeito ou
callback. A leitura independe da máscara anterior dentro dessa função; seu
caller e a política que decide quando chamá-la continuam desconhecidos.

Também foi corrigida a identificação de `modelo+0x220`: o campo vem da contagem
de juntas do MOD. O antigo nome `event_binding_count` não descrevia a origem.
Os relatórios congelados que declaram esse campo igual a zero continuam úteis
para o caminho executado, mas a interpretação correta é **zero juntas no
caminho original isolado**, não ausência de callbacks. O runtime usa agora
`original_joint_count`; a composição glTF de juntas permanece separada.

## Fontes e encadeamento

O alvo é o ELF ARMv6 local com SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
As respostas REA da sessão `1792283675798`, os bytes extraídos, assembly LLVM,
hashes e limitações estão em `research/private/motion-event-dispatch/`.
`rea-provenance.json` mantém os Evidence IDs e as identidades de alvo.

| Origem | Observação de código | Consequência |
| --- | --- | --- |
| Loader `0x6806c0`, REA 066/067 | Lê header MOD de 0x80 bytes, magic `0x444f4d`, versão `0xe6`; u16 em header+6 → rModel+0x68; aloca count×0x18 registros e count×64 matrizes | O valor é a contagem de juntas do recurso, consistente com o parser MOD e o esqueleto exportado |
| Produtor `0x960b50`, REA 019/020 | Lê rModel+0x68; aloca count×0xa8 em modelo+0x224 e count×0x34 samplers por camada; grava count&255 em modelo+0x220 | Preservar o estreitamento observado para o campo do objeto |
| Pre-update `0x95b790` | Com +0x220 não zero e gate correspondente aberto, chega a `0x95df38`/`0x95e6ec` | Caminhos de juntas/TRS; não comprova dispatcher de callbacks |
| Clock `0x9585f0..0x958708` | Copia máscaras atuais para anteriores, zera atuais e combina registros por intervalos | Produz quatro máscaras; não lê os prefixos nem seleciona callbacks nesse recorte |
| Consulta `0x94be10..0x94be78` | Lê máscara atual e prefixo do grupo 0; publica um u16 | Transformação numérica fechada e executada isoladamente |
| Player `0xb14888..0xb148f0`, dentro de `0xb1407c` | Lê o mesmo grupo 0 e publica até dois u16 em ordem dos bits | Segundo consumidor, confirmado estaticamente, ainda sem semântica de gameplay |

Evidence principal do loader:
`ev_ac970a9c539982d77f9cc30697ff6c3cf31a3c4571372e2c3c08690ea735e2e9`.
Do produtor:
`ev_fe1fee4e44bf7c1ab7be006c382337fed1fb70a05bde91f2b561ddc9c84ed542`.
Da leitura de bytes do consumidor:
`ev_86a71c773eeb47ef6e0302828d076c46122086d077b511eb4430bb46672d1331`.
O Ghidra não reconheceu `0x94be10` como função nessa análise; o contrato foi
obtido dos bytes completos até `bx lr`, incluindo literal, e verificado com
execução isolada. Não se fabricou uma fronteira de função no banco REA.

## O que o consumidor pequeno faz

Para a primeira camada, `M=modelo+0x240`. A função usa `modelo+0x348`, isto é,
`M+0x108`, como máscara atual do grupo 0 e `modelo+0x338`, isto é, `M+0xf8`,
como ponteiro do grupo. O destino é um u16 em `*(modelo+0xe28)+0x7910`.

1. Zera o destino a cada chamada.
2. Examina os bits 0 a 31 em ordem crescente.
3. Para um bit ativo, lê `prefixo[bit]` se a tabela existe; ponteiro nulo produz
   valor zero nesse consumidor original.
4. Publica o valor do primeiro bit ativo. Um segundo bit ativo tem seu prefixo
   lido antes de a função terminar, mas seu valor não é publicado.

O valor u16 zero pode ser selecionado por um bit ativo. O destino original
também termina em zero quando nenhum bit está ativo ou quando a tabela é nula;
essa igualdade no destino não demonstra equivalência dos estados de origem.
A máscara anterior em `modelo+0x358` não é lida. Portanto a operação interna
é uma consulta do estado atual (*level*), sem teste de subida ou descida.
Não foi demonstrado que o caller a executa todo tick ou somente numa borda.

O trecho do player em `0xb1407c` zera dois destinos em
`*(player+0xa29c)+0x97c` e `+0x97e` e preenche os valores dos primeiros dois
bits ativos, em ordem. Ele pode ler um terceiro prefixo antes de sair. Esse
trecho também não consulta a máscara anterior. As funções chamadas depois
não foram demonstradas como consumidoras desses IDs: `0xb44ad8`, investigada
em REA 072/073, trata posição/velocidade no recorte, e não fecha esse vínculo.

Não há prova equivalente para os consumidores dos grupos 1, 2 e 3. Em
particular, o grupo 2 contém a maioria dos eventos não zero do corpus, mas
isso não autoriza usar a política do grupo 0 para executá-los. A hipótese de
remap em [lmt-events.md](lmt-events.md) fica confirmada para as leituras acima,
sem promover toda a interpretação de dispatch.

## API nativa numérica

Somente `src/motion_events.hpp/.cpp` e seus testes receberam a consulta:

```cpp
struct PrefixHit {
    std::uint32_t bit_index;
    std::uint16_t value;
};
Result<std::optional<PrefixHit>> Package::first_prefix(
    const ActionIdentity&, std::uint32_t group, std::uint32_t mask) const;
```

`group()` continua dono da resolução de identidade e presença. O novo método
propaga seus erros antes de examinar a máscara. Tabela presente + máscara zero
retorna sucesso com `nullopt`; um bit selecionado cujo valor seja zero retorna
`PrefixHit{bit,0}`. Ausência, rejeição, slot vazio, grupo inválido e identidade
divergente continuam distintos, inclusive quando `mask=0`.

A consulta não depende dos registros de duração: funciona para uma tabela
presente com zero registros e para uma tabela cujo acumulador temporal foi
marcado como não suportado. O consumidor escolhe explicitamente o grupo; a
API não aplica callbacks, fallback zero, clock, comparação com máscara anterior
ou alteração de estado. Ela não foi conectada automaticamente às ações do
caçador. O produtor e o pacote `motion-events-native-v1` permanecem idênticos.

## Verificação executada

`probe.py` executou somente os 104 bytes de instruções originais mais o literal
de quatro bytes de `0x94be10`, usando Unicorn 2.1.4/ARM11MPCORE. Não executou
o jogo, helpers ou GUI. Registros e RAM começam contaminados; somente campos
declarados podem ser lidos. Escritas são limitadas aos oito bytes de stack e
ao destino u16. PC fora do trecho, leitura não inicializada ou escrita fora
do escopo interrompe a execução. Nenhum helper foi substituído por no-op.

Foram **1.026 execuções**: cinco tabelas sintéticas presentes e uma nula,
57 máscaras (incluindo zero, bit 31, todos os bits e múltiplos bits) e três
valores da máscara anterior. As saídas e os índices de prefixo lidos bateram
com o contrato; nenhuma execução leu a máscara anterior.

Há **855 casos comparáveis com tabela presente**, correspondentes a 285 pares
distintos tabela/máscara repetidos com três estados anteriores. Os **171 casos
com tabela nula** confirmam o zero escrito pelo ARM, mas são excluídos da
equivalência nativa: a API preserva erro de ausência. O método nativo não
reproduz a leitura antecipada do segundo prefixo, pois retorna somente o
resultado numérico de uma tabela imutável já validada.

`verify_evidence.py` confirmou os 112 bytes da resposta REA contra o ELF e os
836 words de seis extratos LLVM (3.344 bytes). Reproduziu byte a byte o header
privado `prefix-fixtures.hpp` a partir das saídas ARM, sem gerar esperados pelo
runtime nativo. O relatório ARM tem SHA-256
`2cefdd1141225118cfb6b95d08761f5075678a79cb3ca757629e506cde4bebd6`;
o header, `b44b79f88b8261a2be6fe4ad5cc3d34075d873957c9ff95a4b57a06ade1f15f0`.

O teste C++ standalone normal e o teste com AddressSanitizer/UndefinedBehaviorSanitizer
passaram **305.145 assertivas** cada. Incluem os 855 casos de prefixo, 89.697
consultas estritas e 7.320 signed do kernel anterior, 10.538 truncamentos reais
e casos de identidade, ausência, zero selecionado e tabela sem registros.
Não houve divergência de máscara ou prefixo. Logs, comandos, binários locais e
hashes estão em `native-validation.json` neste diretório privado. Não se
mediram desempenho frio, memória da família de processos ou tokens.

## Fronteira da investigação

Falta localizar o caller de `0x94be10`, os leitores dos IDs do player e os
consumidores dos grupos 1–3. A busca estática retida em `static-candidates.json`
é uma lista de instruções candidatas, com falsos positivos conhecidos; não é
um mapa completo de dispatch. Nenhum registro de callback, relação com dano,
som ou efeito, nem condição universal rising/falling foi comprovado.

As próximas consultas REA úteis são os leitores de `+0x97c/+0x97e` do subobjeto
do player e a ligação de grupo 2 (`M+0x100`, máscara `M+0x110`) a um consumidor
validado. Elas podem ser solicitadas ao dono da sessão sem importar o ELF
novamente. O launcher e todos os snapshots/pacotes congelados foram preservados.
