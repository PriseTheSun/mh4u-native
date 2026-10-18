# Catálogo e diagnóstico de input do laboratório

A CLI aceita `--catalog PATH [--resource N]` no lugar de `--model` e das opções
de textura. O índice começa em zero; o padrão é zero. Um índice ausente ou fora
da lista é erro, sem escolha automática de outro recurso. `--inspect-only`
valida somente a entrada selecionada, sem criar uma janela.

O catálogo é um arquivo UTF-8 com linhas LF, até 64 KiB e 64 entradas. A primeira
linha é `MH4U-LAB-CATALOG 1`. Cada linha seguinte contém exatamente seis campos
separados por TAB, inclusive os campos opcionais vazios:

1. Rótulo único de 1–120 bytes.
2. Caminho do modelo OBJ/glTF/GLB.
3. Textura PPM opcional.
4. Alpha PGM opcional, que exige a textura anterior.
5. Textura PPM alternativa opcional.
6. Alpha PGM alternativo opcional, que exige a textura anterior.

Os caminhos são relativos à pasta que contém o catálogo. Não há busca automática
de recursos. Caminhos absolutos, componentes `.`/`..`, separadores Windows `\`,
`:` e symlinks que escapem da pasta são rejeitados. A existência dos arquivos e
seus formatos são validados ao selecionar a entrada; uma entrada ausente não
impede a inspeção das outras. Use `/` nos caminhos para manter o pacote portátil.

O seletor na janela carrega a cena, texturas e buffers OpenGL candidatos antes
de substituir o recurso ativo. Se o carregamento falhar, o recurso e a câmera
anteriores permanecem e a interface mostra o erro. Após sucesso, a câmera é
enquadrada novamente e a animação começa no tempo zero. A troca é síncrona e pode
pausar brevemente a interface durante a leitura. Cada recurso possui seus dados
CPU e objetos GL; a substituição libera o recurso anterior com o contexto GL
vivo. O catálogo não combina atores, cenário e colisão em uma simulação.

`--trace-input` ativa um diagnóstico opt-in em stdout, limitado a 1024 registros.
Registra botões down/up, IDs de janela/foco, aceitação pelo backend SDL/ImGui,
estados de clique/liberação/captura e mudanças do botão de reprodução. Não
registra texto de teclado. Sem a opção, o diagnóstico não produz registros.

No macOS, o teste em bundle separado instrumentado confirmou via CUA que o
clique chegou ao backend, pausou a animação e abriu/trocou o combo de ações.
Isso não reproduziu a falha anterior observada na janela de outro pacote e não
estabelece sua causa. Nenhuma alteração especulativa em SDL ou ImGui foi aplicada.
A mesma janela instrumentada trocou ator → cenário → colisão e manteve a colisão
após selecionar um arquivo ausente. Logs e validação da CLI estão em
`research/private/input-diagnostic/`. O teste não usou nem encerrou a janela
persistente do usuário.

## Visibilidade das primitivas

`--hide-primitive N` pode ser repetido com índices distintos da cena glTF
inicial. Valores negativos, duplicados ou fora da cena são erros. A opção não
é aceita para OBJ. `--inspect-only` confere esses índices sem abrir janela;
as contagens continuam representando a geometria carregada inteira.

O painel **Partes da geometria** permite ocultar primitivas para inspeção e
restaurá-las com **Mostrar todas**. A seleção acompanha o recurso carregado;
ao trocar de recurso com sucesso, todas as suas primitivas começam visíveis.
Falhas de troca preservam a seleção atual. A câmera e a pose continuam calculadas
com a geometria inteira, permitindo comparar a mesma vista antes/depois. Nenhum
arquivo de recurso é modificado ou regra de visibilidade original é inferida.

O produtor de geometria carrega a identidade da primitiva até cada intervalo
de desenho. A UI não deduz identidade pela posição na lista de desenhos, que
pode mudar conforme a hierarquia de instâncias. Ocultar uma primitiva afeta
todas as suas instâncias na cena selecionada.

Cinco casos de CLI e uma captura nativa estão em
`research/private/material-diagnostic/visibility-cli-validation.json`.
O teste CUA isolado ocultou a primitiva 3 do torso, restaurou todas e trocou
colisão → torso para confirmar o reset. Relatório e capturas:
`research/private/input-diagnostic/primitive-ui-validation.json`.
