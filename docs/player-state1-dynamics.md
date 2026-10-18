# State1: campos compartilhados e dinâmica numérica

`player_fields::Fields` é o dono dos campos extras antes declarados no intérprete de política. O intérprete, o controlador e o ator recebem esse mesmo tipo; não existe alias `player_policy_interpreter::Fields`. Campos de seleção, transição, input e política que já tinham dono permanecem nele. Uma `optional` vazia significa desconhecido, nunca zero.

Os campos acrescentados são PBW `+10/+18/+354` como bits de float32 e PW `+1f8/+220/+838` como palavras de32 bits, além do byte PW`+7f0`. PBW`+14` já pertencia aos extras e mantém o mesmo dono. PBW`+358`, a lista de20 u16 em`+2fc`, os ângulos`+20/+24/+28`, PW`+1f4` e os campos globais usados por `d04924` vêm do `player_controller_step::PlayerFields` canônico. Não há cópia independente deles no novo estado.

## Contrato

`player_state1_dynamics::cost(player,fields,delta_bits,context_ad50)` implementa o perfil `b464fc(-0.5) → b2a58c(mode0)`. `advance(player,fields,delta_bits)` implementa `b2a318(2.5)`. Ambos retornam `variant<Output,Error>`, com `Output{Fields fields,bool updated}`. `updated` registra execução dos stores originais; pode ser verdadeiro mesmo quando os bits finais não mudam. Os argumentos permanecem imutáveis em sucesso ou erro. Nenhuma chamada publica estado parcial.

`delta_bits` representa exatamente o float de `player+14`; `context_ad50` representa a palavra completa lida pelo helper`6b3630`. Não são um FPS, timer de laboratório ou byte truncado. O produtor contextual desses valores continua separado. Os erros distinguem campo desconhecido, operando não finito, overflow e ambiente float incompatível. Falhas da busca de IDs preservam o `player_selection::Error` original como causa.

`player_selection::contains_id` é o único dono da busca`d048c4`, também reutilizado pelo seletor de área. Percorre os20 halfwords em ordem e retorna na primeira igualdade. Um desconhecido anterior a essa igualdade é erro; um sufixo desconhecido após a igualdade não é consumido.

## Ordem recuperada

O custo verifica primeiro o gate canônico`d04924`. Para o argumento−0.5, flags`+1f8` dão prioridade a`0x80` sobre`0x40`, com multiplicadores 3 e 2. IDs111 ou272 aplicam 0.5 uma única vez; ID112 aplica o literal float32`0x3f99999a`. O helper interno repete o gate e pode retornar quando`context+ad50==100`, ou quando PW.float220 ou PW.float838 são positivos. Esses retornos não exigem os operandos posteriores.

Quando PW`+1f4&0x10000000`, ID37 tem prioridade sobre36, aplicando 0.5 ou 0.25. Depois vêm, nesta ordem, delta×valor, soma ao PBW.float354, clamp inferior em+0 e clamp superior pelo PBW.float358. A ordem literal conserva, por exemplo, o sinal de−0 de um limite superior. O perfil limita operandos aritméticos consumidos a valores finitos; não afirma reproduzir todos os resultados originais de NaN/Inf.

O avanço usa low16 dos três ângulos e os helpers`568ab8/568a80/aacf0c`. A tabela e36e98 contém256 linhas de quatro floats. Seus4096 bytes foram comparados com readback REA e ELF fixados. O código não chama seno/cosseno da biblioteca host. Interpolações e combinações usam operações float32 separadas; a componente intermediária Y é forçada a zero antes da multiplicação pelo delta. Ainda assim, os três valores PBW.float10/14/18 são lidos, somados e escritos, inclusive quando delta é zero. Nenhuma transformação glTF, colisão ou unidade física foi atribuída a esses números.

## Prova e limites

A evidência nova está em `research/private/player-state1-id2-native-v1/dynamics-owner/`. `source-readback.json` confere10 readbacks retidos, total5288 bytes de código/literais/tabela, contra o ELF SHA-256`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. A tabela tem SHA-256`f2433a2419e313eb7d18fc5f2682c7be465840bf0f120094ab2b00c79dcca7a8`. O produtor do header registra sua derivação; a tabela não é aproximada analiticamente.

Os testes de fronteira verificam91 condições: prioridades, clamps, zero delta, sinal de zero, campos ignorados versus consumidos, causa de busca, imutabilidade após erro tardio, overflow, arredondamento e flush de subnormais. A comparação completa das continuações ARM é do adapter independente ID2, que usa fixtures históricas e manager-final, não esta implementação como esperado. Nenhum original ARM novo foi executado por este componente.

O perfil exige binary32 IEEE754, arredondamento ao mais próximo e subnormais preservados; operações são compiladas com `-ffp-contract=off`, e fast-math é recusado. A checagem do ambiente só acontece em ramos que consomem float. Os testes nativos não comprovam bootstrap, produção física de input, scheduler, colisão, validity do cache espacial ou nomes de ações de gameplay.
