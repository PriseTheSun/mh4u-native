# Atributos e eventos após a atualização do modelo

`Player::prepare_and_update_events` estende a mesma atualização persistente
iniciada em `b1407c` até o ponto **anterior a `b148f0`**. O modelo continua
avançando uma única vez. O novo trecho consulta atributos, atualiza flags,
limita o ângulo armazenado e publica os dois primeiros valores da tabela
associada aos eventos ativos da camada zero. Não é o despacho de combate.

## Fonte e representação

O REA/Ghidra confirmou os 28 bytes de `af7378` contra o ELF fixado
`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
Essa função lê `base + 0x230 + 672*ID + 4*seletor`; os seletores alcançados
são 1 e 7. O índice usa PBW33, sem busca por slots nem leitura de contagem.
`PlayerRegistry::indexed_rows` representa essa visão do mesmo registro já
usado pelas etapas anteriores, com extensão explícita e palavras opcionais.
Campo desconhecido permanece diferente de zero e de ponteiro ausente.

A consulta inicial distingue os valores `0x236`, `0x246` e `0x238`.
Os dois primeiros podem consultar o seletor 7; o caso especial `0x56` e
PBW31 em 5 ou 6 seleciona a atualização de bits sem chamar o predicado.
A ordem das duas escritas em PBW808 é preservada. Se PlayerD44 vale 1,
PBW24 é copiado para PWb0; em seguida PWb0 sempre fica limitado a 16 bits.

A varredura usa a máscara corrente e o binding do grupo 0 da camada 0,
na ordem dos bits 0..31. Zera PW97c/PW97e, armazena os dois primeiros valores
e ainda lê o terceiro antes de parar. Binding nulo produz zero; identidade
estranha, recurso não resolvido ou prefixo desconhecido produzem erro tipado.
O resolver entrega registros e prefixo do mesmo LMT imutável. Não existe
uma segunda tabela de recursos ou um callback que inventa o resultado.

## Integração e correção relacionada

A composição usa a Registry, o limite de trabalho, o delta e o resolver
já pertencentes ao fluxo anterior. Todos os candidatos, incluindo um Player
estrangeiro alcançado pelo gerenciador, só são publicados após as renderizações.
Uma falha tardia permite tentar novamente sobre os mesmos donos.

A investigação também corrigiu o consumidor de `b07e80` no gerenciador:
o original retorna verdadeiro **somente para o byte 1**. O código anterior
aceitava qualquer byte não zero. Um predicado mínimo compartilhado agora
atende os sete consumidores comprovados dessa função. A continuação que
lê diretamente o mesmo campo com `CMP0` conserva a sua regra diferente.
Os controles com 2 e 255 distinguem esses caminhos e fazem o gerenciador
antigo falhar. A nota histórica incorreta foi preservada com errata separada.

## Verificação e limites

O primeiro diferencial contínuo confirma 22 saídas completas nativas entre
23 retornos originais; o caso de delta 0,5 conserva a rejeição numérica já
conhecida, sem alterar sua entrada. Há também um prefixo anterior, totalizando
24 receitas. O replay compara 100 escritas em 288 bytes do novo trecho, além do
estado anterior e da animação. A documentação de entrega fixa os relatórios
finais de ambos os perfis quando concluídos.

O teste persistente com as sete partes e 19 ossos do caçador usa o LMT real,
banco 0/slot 50: a máscara ativa chega ao valor 10 em PW97c. Foram exercitadas
291 falhas de alocação e novas tentativas; os perfis normal e com
ASan/UBSan passaram 1.205 verificações cada. O teste anterior do modelo manteve
suas 8.821.895 verificações e 291 falhas. A compilação instrumentada recompôs
84 unidades e verificou 439 entradas estáveis. A folha nova passou 349
verificações por perfil; o predicado corrigido tem regressões próprias.

Tudo isso usa inicialização explícita de diagnóstico. A implementação ainda
não fecha `b1407c`, bootstrap, scheduler, todos os estados, combate, missões
ou saves. O laboratório publicado 0.17 continua aberto, mas ainda não chama
essa nova operação nos controles jogáveis. Os testes do Windows nesta máquina
são de compilação; execução nessa plataforma permanece não verificada.

Evidência local: `research/private/player-update-events-v1/`, incluindo
`direct-source/rea-corroboration.json`, `events-native/manifest.json`,
`b07e80-fix/manifest.json`, `actor-integration/native-final/validation.json`
e `composition-independent-review/manifest.json`.
