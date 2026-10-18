# Posição do controlador e posição do modelo

A posição PBW10/14/18 não deve ser copiada para o renderer como um ajuste
isolado. O código original passa por um estado intermediário em player+d00,
aplica animação e só então publica novamente a posição do modelo e do PBW.
Este documento fixa o fluxo observado estaticamente para a próxima composição
nativa; não afirma execução desse update completo.

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
fixa arquivos, hashes e cobertura. Esta auditoria reutiliza a sessão fechada
1792300233949 e não executa ARM. Ainda faltam o contrato de b05350, os alvos
dos slots virtuais no perfil concreto, a composição executável com9573b8 e a
ordem completa de placement/scheduler. A cena do laboratório mantém sua
política de apresentação explícita até essas dependências serem verificadas.
