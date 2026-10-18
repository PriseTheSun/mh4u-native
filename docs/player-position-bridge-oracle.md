# Comparação nativa da ponte PBW/modelo

O comparador `player_position_bridge_tests.cpp` valida `player_position_bridge::apply` contra as 150 execuções originais preservadas de `b07c08 → b06f8c → b07bb8`. Os resultados esperados vêm exclusivamente de `player-position-bridge-v1/arm-expanded/execution.json.gz`, SHA256 `2a96bebd872c2e69a7c7ed10482eca4dac5e9180df95a99fabcdc0997f5c2622`. Este recorte não repete ARM nem calcula expectativas com a implementação C++.

A prova original contém o `9573b8` completo, orientação e trigonométricas reais, `95b490` e `95b940`, sem stubs ou reescrita do PC. Seu manifesto congelado é `d730d0e8942dd20a89062b3d2e14c49a358903abbf000f55195eb0d4fad7d007`. O estado e a colocação de recursos são diagnósticos, com body001 de 19 juntas e sete ações de dois LMT. Não demonstram bootstrap, scheduler ou posicionamento inicial do jogo.

## Projeção e identidade

O produtor privado `oracle-adapter/produce_fixture.py` verifica o manifesto, o relatório, o arquivo de execução e os dois LMT. Retém arenas completas antes/depois e o trace original em um fixture limitado: `retained-final/oracle.bin`, SHA256 `093867384d90a85b54b40d085e65a5cd9d02e52f416f48bd883c421e789eeaef`. Não importa o runtime C++ nem gera poses esperadas. As entradas têm uma ou duas camadas ativas; o suporte de até oito camadas da API não é atribuído a esse corpus.

O adapter usa `motion_resource::Resource` como único parser LMT nativo. Offsets de tracks e eventos fornecidos pelo dono estabelecem a projeção reversível dos endereços originais. Bancos são lidos da tabela original de ponteiros e ligados às identidades vivas dos recursos. A identidade lógica de track/evento, incluindo banco, é comparada separadamente: o controle nativo que retém banco 7 e depois corrompe apenas para banco 8 detecta uma divergência que a projeção do ponteiro ARM apagaria.

A projeção comum de modelos, canais, poses e snapshots foi extraída de `model_animation_frame_tests.cpp` para `test_support/model_animation_frame_fixture.hpp`. O corpo do teste anterior, seu fixture e suas expectativas permanecem iguais. A extração evita um segundo decoder de estado no novo comparator. O relatório `projection-refactor.json` verifica essa preservação estrutural, e a regressão executa o corpus anterior completo.

`model_animation_frame::State` continua dono de camadas, canais, poses, snapshots e transformação do modelo. `model_animation_output::Hierarchy` guarda matrizes/metadados, sem segunda pose. PBW10/14/18/1c ficam no dono `player_fields`; PBW20/24/28 e d10..18 no controller; d4c..60 continuam em `motion_command::Fields`; os novos campos auxiliares pertencem à ponte. O modo Euler opcional é somente low16(model38), em `bridge::Fields`; `Hierarchy::attachment_mode_high16` possui somente a metade superior. A projeção para `joint_output` desloca essa metade sem reter outro modo Euler. O teste verifica esses donos, todos os campos opacos preservados nas arenas e os campos tipados alheios ao recorte, inclusive ausência opcional. Callback/owner de cada junta é verificado explicitamente, sem ocultar alterações copiando os ponteiros anteriores.

A ponte não devolve o scratch ARM d00/d20. O adapter o projeta a partir do PBW final e registra separadamente esses 28 bytes por caso; não cria outro dono persistente. Esse tratamento não é contado como saída nativa armazenada.

## Resultado medido

Ambos os perfis macOS passaram, com stderr vazio nos builds e execuções positivas: normal `-O2` e ASan/UBSan/float-cast-overflow `-O1`, `-fno-sanitize-recover=all`, `-ffp-contract=off`. As fontes transitivas foram coletadas por `clang++ -MM`, copiadas e verificadas antes/depois (59 inputs). Nenhum artefato anterior foi sobrescrito.

| Verificação por perfil | Resultado |
|---|---:|
| Ponte original preservada | 150 casos; 72.317 checks |
| Chamadas reais observadas e conferidas | 144 frames; 141 publicações |
| Gates | 6 retornos antecipados; 3 publicações puladas |
| Rejeições tipadas nativas | 10 |
| Controles de leitura lazy | 3 |
| Regressão do frame anterior | 2.586 casos; 797.985 checks |
| Total das duas suítes | 870.302 checks |

A comparação final de arenas soma 2.366.706 bytes por perfil, classificados assim:

- 810.240 bytes projetados de fatos do estado nativo;
- 4.200 bytes de scratch projetados via PBW;
- 1.552.266 bytes opacos ou emprestados preservados.

Os bytes adicionais lidos para checar imutabilidade dos inputs e os replays de controle não são somados a esses totais. A classificação evita atribuir ao cálculo nativo todo o tamanho de uma arena. As comparações não substituem identidade lógica por igualdade de bytes.

Dezenove processos negativos por perfil (38 ao todo) recusam digest, schema, contagens, limites, truncamento, bytes excedentes, fonte alterada e outputs esperados corrompidos. Os controles semânticos exercitam ausência consumida, falha de resolver, identidade divergente, sampler/eventos indisponíveis, blend não finito e falha de publicação após o frame real ter processado recursos. Também verificam o ramo `b06b38` explicitamente excluído. Erros não publicam um estado parcial e deixam os inputs intactos; efeitos observacionais do resolver continuam pertencendo ao caller. Esses negativos nativos não são apresentados como novas execuções ARM.

O relatório autoritativo é `research/private/player-position-bridge-native-v1/oracle-adapter/native-borrowed-final/validation.json`, SHA256 `e52ac5fea3ac9841428002c9935bc7fdb062b6b2790ebeea0c66a5bfceae32ac`. Ele preserva comandos, tempos de parede individuais, fontes e binários. Não medimos custo do jogo ou execução Windows neste recorte.

## Comando e limites

O executável aceita `fixture sha256 entry source.lmt sha256 entry source.lmt sha256`. As duas fontes são entry 54913 (`dbe3492d31191f8dff3dcf891b6e801d167b6928ab782769040960cc643c202a`) e 92608 (`c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8`), em `research/private/lmt-events-v1/sources/`. Os comandos completos ficam no relatório.

A comparação fecha o perfil numérico admitido da ponte, incluindo timers, posição interpolada, orientação, channels/cursors, poses, matrizes e publicação. O fluxo ativo de `b06b38` e a atualização de direção por `d00628` continuam fora do domínio e retornam erro tipado. Não há descoberta de startup, chamada automática de scheduler, input físico nem composição de colisão. A integração no Player/renderer é uma entrega separada.

A revisão de ownership removeu uma cópia redundante do low16(model38) de `Hierarchy`. O relatório inicial `native-final` permanece histórico, com seus snapshots exatos; `native-owner-final` revalida os donos corrigidos. A queda de 319 checks corresponde à remoção do invariant de igualdade da representação duplicada em cada projeção, sem redução de casos, bytes ou negativos. `owner-amendment.json` preserva os pins antes/depois e o diff desta correção.

O corpus original usa high16(model38) igual a zero em todos os 150 casos; low16 cobre os seis modos. A separação dos donos high/low foi revisada na fonte, mas este oracle não prova execução ARM de attachment high16 não zero. A revisão final também acrescentou 8.716 checks por perfil de releitura das arenas emprestadas e do vetor real de matrizes, após sucesso e falha. Esses checks não aumentam os bytes atribuídos à saída. O relatório `native-owner-final` permanece preservado, e `native-borrowed-final` é o autoritativo.
