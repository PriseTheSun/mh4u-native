# Relógio nativo de animação: `0x957f80`

`motion_clock::advance` executa o perfil numérico recuperado de `0x957f80` até o retorno, incluindo a curva de transição `0xcd5db4`, travessia de snapshots, repetição em ambos os sentidos e máscaras de eventos. O código consome e devolve as camadas canônicas de `motion_pending`; não mantém um segundo relógio nem reinterpreta LMT.

A validação desta entrega usa o ARM original completo. O antigo experimento `root_motion` com aritmética double, zero joints e interceptação de preparação não é prova deste módulo. O relógio isolado não demonstra o agendamento, FPS, combate ou comportamento completo do jogo.

## Donos e contrato

- `motion_request::Layer` continua dono dos campos de tempo e das quatro identidades de eventos. Foram acrescentados `previous_step` (`+18`), `prepared_frame`/`prepared_next_frame` (`+64`/`+68`), os dois controles da curva (`+6c`/`+70`) e as quatro máscaras anteriores (`+118..124`). O campo `offset` existente já representa `+14`.
- `motion_initialization::State` permanece dono de T/Q anteriores e base. `motion_pending::Layer` conserva os scratches T/Q `+98/+c8`, além dos canais do binder/sampler. Este relógio não amostra canais nem altera os frames preparados.
- `Snapshots` separa a contagem original de joints, a presença desconhecida/nula da tabela e um `span` conhecido. Uma tabela conhecida é copiada para o resultado; seu conteúdo pertence ao chamador. A expiração de uma transição só exige a tabela se a contagem de joints for diferente de zero, e só troca para `FFFFFFFF` os owners iguais à camada corrente.
- O resolver devolve um grupo emprestado de `motion_resource::Resource`, com a mesma identidade completa de `motion_request::Binding`. Um binding nulo produz zero; erro de resolução ou identidade divergente nunca vira ausência. Erros preservam o binding e a razão do resolver. O callback e seus recursos devem permanecer válidos durante a chamada.
- `Output` é transacional: contém todas as camadas e snapshots, com métricas de trabalho. Erros não publicam as alterações intermediárias. Nenhum endereço ARM participa da API nativa.

## Ordem observada

O passo é `binary32(speed * delta_time)` para cada camada, inclusive quando o seletor é `FFFF`. A transição `flags & 100` ocorre antes do teste desse seletor. Sua curva usa os dois controles presentes na camada, na ordem das instruções de `cd5db4`; não há clamp inventado. Uma transição expirada zera tempo restante e peso, limpa o bit `100` e invalida os owners correspondentes dos snapshots.

O seletor `FFFF` acrescenta os bits `5` e ignora o avanço, máscaras e `previous_step`. Nas demais camadas, o avanço preserva o byte alto de flags, calcula o campo `offset` com os flags anteriores e usa a constante fonte `0x3a03126f` nas conversões de frame. O atributo `10` impede avanço do frame e atualização das máscaras, mas não impede as demais operações realmente consumidas pelo código original.

Loops positivos subtraem o intervalo `end - loop`, subtraem a translação base e aplicam `inverse(baseQ) * previousQ`. Loops negativos somam o intervalo/translação e aplicam `baseQ * previousQ`. Cada iteração segue binary32, sem redução por módulo ou multiplicação do deslocamento pelo número de loops. O quarto componente de T é preservado. O caminho positivo de intervalo não positivo copia o fim conforme a fonte; um loop negativo sem progresso é recusado.

As consultas de eventos preservam intervalos signed32, inclusive negativos/invertidos. O clock constrói e limita os intervalos; `motion_event_kernel` é o único dono da acumulação. As quatro máscaras anteriores recebem as atuais antes de limpar/consultar. O relógio não interpreta bits como ações de gameplay nem despacha callbacks.

## Matemática compartilhada

`motion_transform_math` ganhou `add`, `inverse_rotation` e `normalize_rotation`. A inversão usa a soma de quadrados X,Y,Z,W, `1/norm` e multiplicação, com sinal negativo em XYZ. Norma não positiva copia somente o template explicitamente fornecido para `10e6570`; template ausente gera erro. Nenhum zero de BSS é presumido.

A normalização usa a mesma ordem de norma, `sqrt`, recíproco e multiplicação. Norma zero produz erro tipado no domínio nativo. O helper é usado pela aplicação de movimento; o clock não normaliza automaticamente seus quaternions. A inversão privada equivalente do initializer foi substituída pelo dono compartilhado, mantendo seu mapeamento de erros. As regressões anteriores foram repetidas com as novas fontes.

Aritmética consumida exige binary32 finito, nearest-even e gradual underflow; FMA/fast-math não são usados. Resultados não finitos e conversões para signed32 fora de `[-2^31,2^31)` são recusados, em vez de reproduzir saturação/estado de exceção VFP. Cópias e campos não consumidos preservam NaN, infinito, sinais de zero e padding. Templates copiados não são normalizados. Flags de exceção de ponto flutuante não são parte da API.

## Limites e caminhos recusados

Os limites padrão são políticas explícitas de trabalho/cópia: 8 camadas, 256 snapshots e bindings de joints por camada, 4.096 iterações de loop no total, 250.000 registros por grupo e 1.000.000 registros examinados por chamada. São verificados antes das respectivas alocações/travessias; não são declarados máximos do formato. `bad_alloc` e `length_error` viram erro de capacidade.

Um loop que não altera o frame binary32 é recusado. Grupos de eventos cujo acumulador excederia signed32 também são recusados. Essa validação considera o grupo inteiro, mesmo quando um intervalo particular terminaria antes. Presença desconhecida de tabela só bloqueia quando realmente consumida; capacidade de uma tabela conhecida que deve ser copiada continua sujeita ao limite. O resolver é um adaptador de confiança, não um sandbox de callbacks arbitrários.

## Evidência e ensaios

ELF fixado: SHA256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.

- Corpo retido em `research/private/root-motion-runtime/advance-v1/`: código original `957f80..958730`, com literais separados em `95832c..95833c`. O pseudocódigo antigo agrupa funções e não substitui o assembly como autoridade.
- `cd5db4..cd5df8` e três literais `cd5df8..cd5e04`: sessão REA `1792297437998`, respostas `004-procedure_assembly.json` e `005-read_bytes.json`, Evidence `ev_a7c2b176006cc6b03e263c3fc355ca4f5feb2cba76d5d348ee2f455044453cd9`. O pseudocódigo VFP vazio é uma limitação registrada.
- Produtor independente: `research/private/motion-clock-native-v1/verify_arm.py`. Executa ARM11MPCORE/Unicorn 2.1.4, código original e helper real, sem stubs/interceptação de resultado, guardando limites de instrução, leituras inicializadas, escritas, pilha, retorno e registradores preservados.
- Oracle: `arm-initial/oracle.bin`, SHA256 `862cb906e0bdccb8d3071e12c27bc0decd033d6af0ddc45ee9e3289aeaf6e238`. São 1.520 execuções: 788 sintéticas e 732 casos de 122 ações dos LMT 54913/92608. Incluem múltiplas camadas, controles de curva, expiração/owners, frames nas fronteiras, ambos os sentidos, loops múltiplos, máscaras e campos não consumidos não finitos. Todos os 505 endereços de instrução dos três ranges de código foram observados; isso não significa cobertura de todos os valores possíveis.
- O adapter compara os 304 bytes de cada camada usando apenas campos nativos declarados para as escritas, além dos snapshots completos. Eventos reais são resolvidos pelo `Resource` canônico e comparados aos registros fonte antes do avanço. Entradas e bindings permanecem imutáveis.
- `native-final/validation.json`: macOS arm64, normal e ASan/UBSan estrito, com recuperação desativada, stderr vazio e contagens exatas: 742.552 checks por variante, incluindo 54 checks de fronteira. Dez processos negativos rejeitaram fixtures corrompidas. Os ensaios de corrupção de fixture alteram outputs originais, truncam/adicionam bytes e corrompem cabeçalho/contagem; cada processo deve rejeitar. O adapter permite fixtures sem hash esperado para esses negativos; o produtor de validação e manifesto fixam todas as entradas antes/depois.
- `initializer-regression/validation.json`: 2.444 casos de referência/237.147 checks e 1.098 casos reais/150.091 checks em cada variante normal/estrita. Os relatórios históricos permanecem intactos.

Os snapshots, binários derivados e fixtures são privados. Este módulo não altera o launcher, os pacotes, o render, nem declara equivalência de execução em hardware 3DS ou validação Windows em runtime.
