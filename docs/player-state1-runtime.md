# Entradas completas dos handlers de estado1

Foram executadas 384 chamadas completas do ARM original desde `b66a20` até o
retorno real, com `act_st=1`, `pbw.byte5=0` e `act_no={1,2,4,9}`. O dispatcher,
os handlers, os helpers, os wrappers `b0c198/b0c060`, o setter `9575dc`, as
consultas `958884`, samplers e o instalador/fill são originais. Não houve
substituição de resultados, retorno fabricado ou reescrita de PC.

Esta entrega fecha a entrada que [a investigação anterior](player-state1-dispatch.md)
parava antes do wrapper. Não fecha a continuação dos subestados1/2, o scheduler,
bootstrap completo do jogador ou o significado físico de nomes de movimento.
`r0` ao retorno é retido como fato de ABI, sem interpretação booleana.

## Contextos explícitos e recursos

O corpus é o produto cartesiano de 4 ações, 4 selectors atuais(0/3/4/ffff), 2 perfis
de pending/live, 4 perfis de banco e 3 poisons(00/a5/ff). Há oito slots de storage
e duas layers ativas. A preparação dos canais executa o `9577ec` completo já
validado, com 8 juntas, recursos e templates explícitos; o ensaio de entrada usa
os canais de modelo preparados. Nenhum endereço ARM integra a proposta de
estado nativo. As outras seis layers e todos os bytes fora das escritas
permitidas permanecem preservados.

O banco0 é explicitamente escolhido entre:

| Perfil | Conteúdo | Pedidos primários aceitos / rejeitados | Sem setter |
|---|---|---:|---:|
| co0 | LMT entry92608, 136 slots | 72 / 0 | 24 |
| lco0 | LMT entry54913, 68 slots | 48 / 24 | 24 |
| ausente | ponteiro de banco nulo | 0 / 72 | 24 |
| tabela nula | wrapper presente, tabela nula | 0 / 72 | 24 |

A layer1 usa o recurso54913 no banco11, peso0,35 e atributos0x40100,
fornecendo uma contribuição real na consulta de múltiplas layers. As escolhas
de banco e estado são inputs do ensaio, não uma inferência de que lco0 seja
co2 ou o banco correto do jogo para essas entradas. O source getter determina
normalmente a aceitação: por exemplo, slot49 é vazio no recurso54913 e presente
no92608. Canais já ligados preservam a identidade de seu recurso, mesmo quando
o banco do pedido posterior é explicitamente ausente.

Os LMT originais e seus hashes são reutilizados do dono já validado. A cópia
privada em memória reloca apenas os ponteiros de ações/tracks/eventos declarados
por esse dono; as fontes ficam somente leitura e são comparadas depois. Os
24 bytes de curvas são o snapshot observado após `d3f6fc`, SHA256
`283bcb5502bd9e7c2daf2245fa36170f8f1f838accd1ed146ff3ba0f3e03b922`,
e não os zeros do ELF anterior à inicialização.

## Efeitos confirmados até o retorno

Em 96 casos os wrappers condicionais emitem zero pedidos porque o selector já
é3 ou4. Os efeitos anteriores e posteriores ainda ocorrem. Nos demais 288,
foram observados um pedido de layer0 e um sentinelffff de layer1: 576 setters
no total, com 120 aceitações. Os 168 pedidos primários rejeitados também completam
seus handlers e os efeitos posteriores, sem transformar rejeição em erro do
ensaio.

- ID1 instala a tabela35 antes do wrapper e escreve `pw+50=20` depois dele.
- ID2 chama o wrapper antes de instalar a tabela35; depois da instalação grava
  `pw+144=30` e copia `pbw+24` para `pw+b0`.
- ID4 copia `pw+b0` para `pbw+24` antes do wrapper e instala a tabela31 depois.
- ID9 grava `pw+50=20` antes do wrapper e instala a tabela35 depois.

Os efeitos do helper de entrada e flags anteriores permanecem os descritos na
investigação anterior. A instalação zera os 48 latches e os campos declarados,
mas preserva os seis bytes de padding `pw+13a..13f`; o replay compara também
esses bytes. A tabela35 está em `f2efb8`, 792 bytes com sentinel; a tabela31 em
`f2f2d0`, 704 bytes com sentinel. A segunda foi salva privadamente em
`research/private/player-state1-runtime-v1/inputs/table-f2f2d0.bin`, SHA256
`c7eacbb0116b97e7f88ad675e109e27675ea93791f4112928ccd1e8fe6f0e310`.
Sua instalação foi executada; seus 31 predicados não foram executados aqui.

## Prova, integridade e reprodução

Alvo ELF SHA256
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Os bytes vêm dos readbacks congelados de `player-state1-dispatch-v1` e dos
owners de motion/query/sampler; cada intervalo executável e de dados é conferido
contra o ELF. Não foi aberta outra sessão REA. O readback original026 da sessão
1792306020796 cobre a tabela31; `source-facts.json` confirma seu hash e stride.

Resultado autoritativo:
`research/private/player-state1-runtime-v1/arm-observed/arm-validation.json`.
Foram 264.792 instruções, 288 consultas de modelo,960 amostragens reais e 384 entradas
no instalador. Os guards verificam código permitido, leitura inicializada,
escrita limitada, páginas de recurso imutáveis, limite de instruções e timeout,
SP e registradores preservados. Nove controles negativos retiram fatos/helpers,
negam escrita ou excedem o orçamento; todos falham explicitamente. A tentativa
de continuar com byte5=1 também recusa o helper fora do perfil declarado.

A fixture contém as sete arenas completas antes/depois, pedido/retorno do
setter, wrappers, instalações e consultas observadas. O readback independente
passou 10.656 checks, comparou 5.376 snapshots e reproduziu cronologicamente
26.376 stores originais fora da stack, totalizando 91.296 bytes escritos. Ele não
lê o C++ nativo. As asserções derivadas da fonte sobre campos/ordem estão
separadas dos snapshots e da execução original.

A fixture MH4US101 tem 122.781.188 bytes e SHA256
`32a907a148ac894dbd51a4b20e8fdee0bea54bc93231ac59db4d47ef5b20264d`.
A derivada MH4US102 acrescenta somente um bit observado de ordem por instalação,
para que o consumidor compare essa ordem sem uma fórmula por ID da ação.
Ela tem 122.782.724 bytes e SHA256
`5a48fa8dcf24f45cfe45456115e122eded7a7abdde7726608af32a198ce0ce0c`.
A derivação é reversível byte a byte. Formatos detalhados e traces gzip ficam
no mesmo diretório privado. Nenhum artefato histórico foi substituído.

O ensaio final levou 5,49 s de parede e 5,62 s de CPU, com pico RSS Python de
274.497.536 bytes. Esses valores são do processo, não da família completa;
tokens não foram medidos. A etapa não modifica código C++, pacote, CMake,
launcher ou janela e não constitui execução em hardware 3DS.

```sh
research/private/root-motion-runtime/venv/bin/python -B research/private/player-state1-runtime-v1/verify_arm.py --output /CAMINHO/NOVO
python3 research/private/player-state1-runtime-v1/verify_fixture.py --folder /CAMINHO/NOVO --sha256 32a907a148ac894dbd51a4b20e8fdee0bea54bc93231ac59db4d47ef5b20264d
```

O prefixo antigo e a primeira execução completa foram preservados; a execução
final acrescentou somente observação cronológica/controles, com fixture
byte-idêntica. O manifesto distingue prova ARM original, readback e derivação.
