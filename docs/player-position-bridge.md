# Posição do controlador e posição do modelo

A posição PBW10/14/18 não deve ser copiada para o renderer como um ajuste
isolado. O código original passa por um estado intermediário em player+d00,
aplica animação e só então publica novamente a posição do modelo e do PBW.
O fluxo foi identificado estaticamente e depois executado no código ARM original,
com recursos reais e estado de diagnóstico explícito. A composição nativa e o
update completo que a envolve ainda estão pendentes.

| Função | Escritas observadas |
| --- | --- |
| `b07c08` | Copia PBW20/24/28 para player+d20/d24/d28 e PBW10/14/18 para player+d00/d04/d08; escreve d0c=0. |
| `b06f8c`, antes de `9573b8` | Após gates, reduz d20/d24/d28 a16bits, chama b05350 e copia d00/d04/d08 para model40/44/48, com model4c=0. |
| `b06f8c`, após `9573b8` | Se d38 tem bits de1.0, copia model XYZ para d00/d04/d08 e zera d0c. Senão calcula separadamente `old + (model−old)×d38` e restaura d38=1.0. |
| `b06f8c`, publicação final | Se d64&2 é zero, grava model XYZ como d00/d04/d08 + d10/d14/d18, zera model4c e chama os slots virtuais4c/50. |
| `b07bb8` | Copia d20/d24/d28 de volta para PBW20/24/28 e d00/d04/d08 para PBW10/14/18; escreve PBW1c=0. |

Todos os offsets são hexadecimais. A transformação inicial é uma cópia dos
bits XYZ, sem troca de eixos nesse trecho. O deslocamento d10/d14/d18 só entra
na publicação posterior do modelo; ele não é automaticamente a posição PBW.
O ramo d64&8 retorna antes da cópia inicial. Há ainda manipulação de clocks,
flags e orientação em b06f8c: o recorte não pode ser tratado como um setter
incondicional de posição.

O update b1407c chama o dispatcher b66a20 antes da sequência
b07c08→b06f8c→b07bb8, mas executa outras rotinas entre essas etapas e faz uma
segunda sequência de placement/cópia mais tarde. O código não autoriza
substituir o update inteiro por um único dispatch seguido de render. O
modelo40/50 já pertence a `motion_application::Model`; os campos PBW pertencem
aos donos do controlador. A futura integração deve compor esses donos sem
criar outra cópia persistente da pose ou da posição PBW.

`b26f50`, chamado no fluxo de posicionamento inicial, monta um payload de40
bytes e chama2465c8. Não é o elo direto de cópia para model40. Essa distinção
evita inferir o papel de uma função apenas pelo caller.

As três funções principais têm1416bytes de readback REA conferidos novamente
contra o ELF fixado. Evidence:

- b06f8c: `ev_7aed0bd4e4782f1870e78a3af44fb8ef1dd82c94d05f2e3b759c29c2b52dd5ad`;
- b07c08: `ev_b27a6b4d5c7d77f05da4f13fa2f3d5f73ce59ae2fe7bbcbecd495369c1e8c35f`;
- b07bb8: `ev_5a13ba24cf6b66425b93ab7da45a1179b66513a47a043ef7d1e00124ff797a4b`.

O relatório `research/private/spatial-triangle-native-v1/position-bridge-source.json`
fixa arquivos, hashes e cobertura dessa primeira auditoria estática. Ela reutiliza
a sessão fechada1792300233949 e não executa ARM.

## Execução original da ponte

O conjunto separado `research/private/player-position-bridge-v1/` preserva150
execuções completas em66 sequências de `b07c08→b06f8c→b07bb8`, sem substituir
helpers ou alterar o PC. Usa body001 com19 juntas e os LMT54913 e92608.
Os bancos, globals, PBW e estado inicial do modelo são explícitos de diagnóstico;
não representam o startup recuperado do jogo.

Em144 casos o fluxo passou pela animação9573b8; em141 houve publicação final
via95b490→95b940. Em51 casos, tanto o PBW quanto a posição do modelo mudaram.
Seis casos retornaram pelo gate `d64&0x8`, e três pularam a publicação final
por `d64&0x2`. Os150 casos preservaram os campos opacos observados do PBW.
Uma auditoria posterior conferiu cópia, interpolação, offsets e consumo
condicional dos campos. Esses resultados são observações do ARM e uma auditoria
das saídas; ainda não são equivalência de uma implementação C++ da ponte.

As vtables `e6ed8c` e `e6bca4` selecionam95b490 no slot4c,95b940 no slot50
e b05328 no slot80. O último retorna zero nesse perfil; b05350 continua pela
conversão original6d4080 de ângulos para matriz e quaternion. Os oito readbacks
REA da investigação somam3528bytes, conferidos contra o mesmo ELF.

A futura implementação precisa conservar os donos existentes:

- `motion_application::Model` guarda posição e rotação do modelo;
- `motion_command::Fields.fieldsd48[1..6]` guarda d4c/d50/d54/d58/d5c/d60;
- `player_controller_step::PlayerFields.fieldsd10` guarda d10/d14/d18;
- o modo Euler nos16bits baixos de model38 deve ser distinto do attachment mode
  nos16bits altos usado pela saída das juntas.

A direção atualizada pelo ramo `d64&0x1` e a rota ativa de b06b38 continuam
fora do perfil admitido. Também faltam a implementação nativa da ponte, sua
integração no Player e a ordem completa de placement/scheduler. A cena do
laboratório mantém sua política de apresentação explícita.
