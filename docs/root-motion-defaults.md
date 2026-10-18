# Defaults de movimento demonstrados no binário

Esta investigação resolve duas entradas antes desconhecidas: o retorno do
sampler quando não existe um track e os valores escritos pelo inicializador
dos templates globais. O código da ROM foi analisado com REA/Ghidra e os
trechos selecionados foram executados isoladamente em Unicorn. Isso não é uma
captura da memória do jogo rodando nem prova de que esses globais nunca mudem.

## Canal ausente e referência constante

O binder `0x95aa90` zera os 16 bytes de cada sampler de modelo antes de ligar
os tracks de tipo 3 e 4. Se nenhum track existir, o ponteiro continua nulo.
Esse comportamento está no assembly retido em
`ev_536563d3f719d8414b35f84687804ac09d3f3d0ced61bd6608dd9e918e4a0bda`.

O dispatcher `0xaea0e4` verifica esse ponteiro em `0xaea0fc`. O ramo nulo
`0xaea19c–0xaea1b0` retorna os registradores `s0..s3 = [0,0,0,1]`, carregando
zero e um de literais próprios. Para translação, o consumidor usa XYZ zero;
para rotação, usa XYZW identidade. Isso não depende de uma convenção do glTF,
do bone ID 255 ou do conteúdo inicial da BSS. O caminho foi observado antes em
`ev_02441f313eda39bd06c1494f9ebfa8bec85876427ca64be0dde2515738c9d660`
e agora corroborado por leitura dos bytes e execução.

Codecs 1 e 2 têm outro caminho: retornam literalmente os quatro floats do
track em `+0x10`. Referências não unitárias não são normalizadas nesse ponto.
Um canal ausente e um canal presente com referência constante permanecem
representações distintas. Os [pacotes pareados](paired-actions.md) conservam
essa ausência; um futuro consumidor pode aplicar o fallback demonstrado,
registrando-o como regra do runtime em vez de inventar um track fonte.

## Inicialização dos templates

O início do programa chama o dispatcher Thumb `0x1013b8` em `0x100010`.
Esse dispatcher percorre `[0xebee78,0xebfd60)`: 954 entradas de quatro bytes,
cada uma somada ao endereço da própria entrada. O slot `0xebf64c`, ordinal
501 a partir de zero, contém o deslocamento assinado `−1712628`; o destino é
`0xd1d458`. A soma relativa explica por que buscar apenas ponteiros absolutos
para o inicializador não o encontrava.

O trecho `0xd1d4b8–0xd1d9a8` desse inicializador carrega seus próprios ponteiros
e constantes e escreve, entre outras estruturas matemáticas:

| Endereço | Valor escrito | Uso no subconjunto investigado |
| --- | --- | --- |
| `0x10e62c0` | `[0,0,0]` | Template vetorial |
| `0x10e6370` | `[0,0,0]` | Template vetorial de reset |
| `0x10e6560` | `[0,0,0,1]` | Quaternion identidade de reset |
| `0x10e6570` | `[0,0,0,0]` | Fallback de quaternion zero |

Os dois últimos templates são diferentes. Os fixtures anteriores de aplicação
e avanço declaravam globais sintéticos e não afirmavam ter recuperado esses
valores. Continuam preservados. A API nativa ainda pode rejeitar quaternion
degenerado em seu contrato; conhecer o template zero não equivale a implementar
todos os ramos que o usam.

Esta cadeia estabelece o destino selecionado pela tabela e o que o trecho
escreve quando executado. A execução isolada começa após a construção de duas
tabelas matemáticas e termina antes do epílogo; não executa o startup inteiro.
Também não exclui sobrescritas posteriores ou uso antes da inicialização.

## Evidência e validação

Alvo: ELF sintético de análise
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`, com
bytes de código originais e BSS derivada. A sessão REA focada usou Ghidra
12.1.4, ARMv6 little endian e `Decompiler Switch Analysis` desativado.

| Evidência REA | Conteúdo |
| --- | --- |
| `ev_b588ec9df298dfbd8eb7579b0c3bbde85be8123c40531c0f3b7a4bf2f8621108` | Bytes do inicializador, incluindo literais |
| `ev_16287afe90fc7d0bbc48c21295f4d3d47a35a1c4a21e244aeaad1f615ebb6458` | Helper Thumb de construção de array |
| `ev_8d3bd9456c9eb47a34af255f593122ff873ba526103b01f02ee6e5ad213638db` | Helper de elemento que escreve zero em `+12` |
| `ev_a593f7626c2519b94135a52107009051335df6a82c073229ea552b7c3580ebfb` | Dispatcher de inicializadores e limites da tabela |
| `ev_92bff346c8a2a3b98cb5a138121eee77d7fe0efe2d907a5d58e4d9474916101d` | Chamadas do início do programa |
| `ev_98b94b6025339681e81445a2ad4e77dc1d7ecb7c9dd959dbdd4b409e306d706a` | Entrada relativa selecionada |
| `ev_acc88093f4d8fb85c426282fb83fa12c2d46bf8930884c3d54e81ea80b10d65f` | Dispatcher do sampler, incluindo literais |

Ghidra não reconheceu uma função contendo `0xd1d980`. Duas consultas pelo nome
candidato falharam e estão retidas; não se atribui pseudocódigo a essa região.
A interpretação usa bytes REA, disassembly LLVM e execução isolada com intervalos
de código declarados, distinguindo pools de literais de instruções.

As sete leituras REA, totalizando 1.938 bytes, coincidiram exatamente com os
segmentos inicializados do ELF. O bundle
`research/private/rea-motion-defaults-evidence.json` tem SHA-256
`c2e06f408f5ebc8e25cc840cd23c4517ba532df369d35dc6a2dd51baafc367b3`.
Manifesto, IDs, hashes, erros e snapshot estão relacionados em
`research/private/root-motion-defaults/evidence-selection.json`. A sessão
foi fechada e o cliente terminou com código zero.

O harness `tools/resources/root_motion_defaults_probe.py` executa somente os
intervalos declarados. RAM e registradores começam com três valores distintos
de poison; leitura de memória não inicializada, escrita fora do escopo, saída
dos intervalos, timeout ou orçamento de instruções interrompem o caso. O tail
executa os dois helpers reais, sem interceptações. Não depende dos zeros que
uma biblioteca de emulação possa preencher ao mapear memória.

Passaram 21 casos positivos: três execuções do tail, 12 consultas com sampler
nulo em frames `−7`, `0`, `4.5` e `100000`, e seis consultas de referência
constante com valores não unitários. Cada tail executou 365 instruções e produziu
os mesmos quatro templates. Três controles negativos confirmaram rejeição de
ponteiro não inicializado, leitura de estado de codec fora do perfil e execução
de helper retirado da lista permitida. Os testes não ampliam os limites de
frames aceitos pelas APIs nativas; apenas verificam esses ramos originais.

```sh
research/private/root-motion-runtime/venv/bin/python -B \
  tools/resources/root_motion_defaults_probe.py \
  --elf research/private/mh4u-analysis-bss.elf \
  --output research/private/root-motion-defaults/recheck.json
```

Use um destino novo. O relatório válido é `isolated-run-v3.json`; preserva a
fonte do carregador ELF utilizado, versão Unicorn 2.1.4, CPU ARM11MPCore,
FPSCR zero, rastros e entradas. Hardware real e modos diferentes de FPSCR não
foram comparados. Uma tentativa anterior de análise ampla foi cancelada para
usar o perfil focado existente; outro ensaio incompleto mapeava a imagem toda
por página e foi substituído pelo mapeamento dos intervalos necessários.
Essas tentativas estão retidas em `aborted-attempts.json`, sem alegação de
benchmark ou comparação entre tarefas completas.
