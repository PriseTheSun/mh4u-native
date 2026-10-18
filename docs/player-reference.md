# Orientação de referência e classificação relativa

O motor tem uma implementação numérica de `0xb1061c` e de seus helpers
`0x568b54`/`0x5687b8`, além de uma sobrecarga do classificador de entrada que
usa esse resultado. Não há ligação com a câmera de inspeção, com eixos SDL
ou com a seleção automática de ações. A identidade física do objeto de
referência e seu produtor não foram estabelecidos neste recorte.

## Fonte e contrato

O ELF europeu fixado tem SHA-256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
O assembly de `b1061c` e `b2ab7c` foi retido em `player-input-state`.
REA/Ghidra, sessão `1792285120229`, forneceu novos readbacks de 628 bytes:
`5687b8+64`, `568b54+300` e tabela `1057648+264`, iguais aos segmentos
PT_LOAD do ELF. A proveniência está em
`research/private/player-reference-v1/source-readback.json`.
Pseudocódigos dos helpers: Evidence
`ev_638151e7601f656283559d8c69b5aad7e8b99ba5bd36c1cd16044a30ec9513f0`
e `ev_558d1e43e1dcc68b7d592ea67c909aaa77968286db480b73e6346536f448de83`.
Assembly e bytes, incluindo limites entre código e literais, têm precedência
na derivação das operações.

`player_reference::State` contém o flag em `pw+f0`, byte em `pw+150`, palavra
em `pw+14c` e, opcionalmente, os quatro floats do objeto apontado por
`(*(0x1057294))+1c`. A ausência de pontos representa **ponteiro explicitamente
nulo**, não memória desconhecida. O caller precisa estabelecer essa identidade.

1. Se o flag `0x4000` está ligado e o byte é exatamente 1, devolve toda a
   palavra `pw+14c`, inclusive seus 16 bits altos. Não consulta pontos.
2. Se o objeto está explicitamente ausente, devolve zero.
3. Caso contrário, subtrai `+40` de `+60` e `+48` de `+68`, com arredondamento
   float32 separado. Um vetor zero devolve zero, incluindo zeros com sinal.
4. O helper escolhe um octante, divide componentes para obter razão em `[0,1]`,
   e interpola uma tabela de 33 pares de floats. Multiplica o resultado angular
   por 256, trunca para u32 e conserva os 16 bits baixos.

Os 264 bytes da tabela são dados numéricos fixados por hash em
`src/player_reference_table.hpp`, gerados pelo script privado `generate.py`.
Não se usa `std::atan2`: a interpolação original pode dar valores diferentes.
O multiply/add da tabela tem duas operações float32, sem contração para FMA.

O ramo vetorial aceita coordenadas e diferenças finitas, incluindo subnormais,
com arredondamento para o mais próximo e subnormais preservados. Valores não
finitos, overflow nas diferenças ou ambiente flutuante incompatível produzem
erros explícitos. Um probe aritmético detecta modos que zeram subnormais;
`fegetround()` sozinho não detecta FPCR.FZ nem MXCSR FTZ/DAZ.
Os ramos da palavra armazenada e do objeto nulo não validam dados flutuantes
que não seriam consultados. FPSCR original foi fixado em zero no oracle;
não há afirmação de equivalência para todos os modos possíveis do processador.

## Composição com entrada

A sobrecarga `player_input::in_sector(channels, id, scope, reference)` mantém
o dono único da tabela de setores e da aritmética modular. Preserva a ordem:
ID desconhecido retorna falso; modo 1 é rejeitado; só então a referência é
considerada. Com `reference_flag==0`, nenhum campo do contexto é usado. Com
flag não zero e `pw+f0 & 0x4000`, a correção também fica zero.

Nos demais casos, resolve a referência e calcula
`D = u16(pw.orientation_b0 - heading)`. O predicado consulta a magnitude
somente depois e exige `magnitude>=50` e
`u16(angle-D-lower) <= u16(upper-lower)`. Isso preserva bordas inclusivas e
sobrepostas; prioridade pertence ao caller. Um erro da referência conserva
sua categoria no erro composto, inclusive com magnitude abaixo de 50.

A API anterior, sem contexto, continua rejeitando `reference_flag!=0`.
O argumento forçado do helper direto `b2ab7c` e o gate virtual do modo 1
continuam fora desta API. Nenhum retorno constante substitui esses caminhos.

## Validação executada

- 4.536 casos do resolvedor, incluindo todos os octantes, vizinhos float32 dos
  33 índices, coordenadas transladadas, palavra armazenada e objeto nulo.
- 12.819 consultas compostas pelo dispatcher ARM original, preservando limiar,
  bordas modulares, redução da orientação e gates que ignoram a referência.
- 4.096 casos adicionais com padrões float32 finitos, subnormais, extremos e
  diferenças finitas; amostragem determinística, não cobertura exaustiva f32.
- Nenhuma chamada substituída no oracle Unicorn 2.1.4/ARM11 MPCore. Guardas de
  código, memória inicializada, escrita somente na pilha, identidade e orçamento.
- 64.935 verificações C++ no conjunto principal e 12.339 no adicional, ambas
  sem divergências, em execução normal e ASan+UBSan no macOS arm64.

A revisão independente reproduziu `heading(denorm_min, denorm_min)` mudando de
8192 para zero sob FPCR.FZ, mesmo com `FE_TONEAREST`. A correção rejeita esse
ambiente e conserva os ramos armazenado/nulo. Fonte, binários e logs antes/depois
ficam em `floating-environment/`. A regressão altera e restaura o controle
flutuante somente dentro do processo de teste; a janela do laboratório não é
alterada. Testes equivalentes de FTZ/DAZ estão no código x64, mas não foram
executados em Windows.

Relatórios e comandos ficam em `research/private/player-reference-v1/`.
O primeiro gerador de casos extremos convertia os padrões de bits em valores
numéricos; foi corrigido antes desta conclusão. Aquela execução fica preservada
em `superseded-extreme-values/`, sem sustentar a cobertura acima.
Os arquivos anteriores de `player_input` foram retidos em `prior-source-snapshot/`
antes da extensão; os hashes históricos não descrevem os novos arquivos fonte.

Essa validação estabelece consumidores numéricos isolados. Não estabelece a
ordem do scheduler, o estado inicial de câmera/jogador, o ritmo original,
fidelidade de gameplay ou execução em Windows.
