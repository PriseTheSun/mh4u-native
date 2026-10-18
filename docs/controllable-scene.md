# Cena nativa controlável — marco 0.7

Registro histórico do marco 0.7. O preset atual usa o
[caçador nativo MOD/LMT](native-world.md), inclusive na sessão local compartilhada.
Os testes e caminhos abaixo descrevem a versão original desta integração.

A cena reúne o cenário `v00a01`, seu acessório na primeira chave SDL, o candidato
de caçador com sete peças e as malhas SBC de piso e paredes. Os recursos são
reais e derivados da ROM local. Ponto inicial, raio, altura, velocidade, direção
e relógio de movimento são parâmetros explícitos de diagnóstico. Não foram
recuperados do controlador de Monster Hunter 4 Ultimate.

## Executar

```sh
python3 tools/launch_lab.py --world --hide-primitive 3
```

O pacote oferece `macOS/Abrir cena controlavel.command` e
`Windows-x64/Abrir cena controlavel.bat`. A build Windows foi compilada, mas
não executada. A primitiva 3 do torso fica oculta somente nesses atalhos; é uma
escolha de inspeção, sem alterar os recursos ou afirmar a visibilidade original.

WASD e o analógico esquerdo fornecem uma intenção horizontal relativa à câmera.
A soma é limitada a magnitude um. Mouse/analógico direito orbitam e a câmera
acompanha o caçador por padrão. Entrada é consumida somente quando a janela tem
foco e o painel não está capturando teclado. Deadzone e eixos configuráveis
usam o armazenamento de preferências existente, com backup ao salvar. Ainda
não houve teste com joystick físico. O botão de retorno ao ponto inicial está
implementado, mas seu clique via automação não foi validado.

## Ownership e transformação

`world_config.cpp` é o único leitor da receita; `tools/world_recipe.py` é seu
produtor compartilhado por launcher e empacotador. A receita referencia os
recursos do catálogo existente, em vez de duplicar seus caminhos. O cabeçalho
é `MH4U-DIAGNOSTIC-WORLD 1`, seguido de pares nome/valor separados por TAB, com
LF final. Campos ausentes, desconhecidos ou duplicados, números não finitos e
arquivos maiores que 64 KiB falham. Os caminhos são relativos à receita e passam
pelo mesmo resolvedor do catálogo: sem travessia, caminhos absolutos ou escape
por symlink. A receita e sua proveniência local ficam em
`assets/local/village-diagnostic-v1.*`.

O viewer possui recursos separados para cenário e caçador. Ambos usam o mesmo
carregador e a mesma rotina de desenho. O cenário conserva a transformação
exportada; o caçador recebe translação e yaw rígidos através de `uModel`. O
recurso GPU do cenário é criado e destruído dentro da vida do contexto OpenGL.
A biblioteca `mh4u_world` depende da colisão, que depende de cena; não há uma
segunda implementação de leitura glTF ou de colisão no viewer.

`DiagnosticWorld` possui configuração, mundo geométrico e estado do caçador.
O núcleo [de locomoção](locomotion-runtime.md) recebe deslocamentos explícitos,
sem relógio ou SDL. O viewer o chama em passos de 1/120 segundo; o tempo de um
frame fica limitado a 0,05 segundo. Essa convenção não estabelece o FPS original.
O yaw acompanha a intenção, mesmo quando há bloqueio. O piso fornece Y. O
offset visual `2.09625` foi escolhido a partir do mínimo Y da pose inicial
inspecionada, não da origem da bind pose nem de uma regra original de pés.

O preset usa raio 22, altura 175 e velocidade 150 unidades fonte por segundo.
Seu ponto inicial é `(-61.14208710193634, -17.212244669596355,
11963.744791666666)`, derivado de uma face de piso com folga. O carregador confere
que o Y declarado corresponde ao piso em até 0,01 unidade. Nenhum desses valores
é apresentado como spawn ou constante original.

## Verificação executada

- CTest no macOS: 13/13 suítes passaram. Depois de remover uma dependência
  redundante de link, as quatro suítes afetadas passaram novamente; ambas as
  builds terminaram sem avisos nos logs finais de link.
- CLI pública: 15 casos cobrem apoio inicial, movimento livre, bloqueio em
  deslocamento longo, prioridade de entrada explícita sobre catálogo padrão,
  conflitos de argumentos e receitas/estados inválidos. Também passaram usando
  o binário e os recursos do pacote.
- Geometria real: 86 faces de piso, oito bordas e 90 faces de parede convertidas
  em 45 segmentos; os testes do núcleo incluem 64 sweeps radiais longos e
  1.000 passos contínuos com apoio/folga conferidos.
- Regressão visual: a captura de recurso isolado no mesmo instante conservou
  os 744.000 pixels comparados da região 3D após centralizar a rotina de desenho.
  A comparação não abrange todos os materiais, poses ou ambientes gráficos.
- Pacote `build/packages/20261017-201847`: 176 arquivos conferidos por tamanho
  e SHA-256; recursos macOS/Windows idênticos; nove entradas, catálogo textual
  e receita carregados pelo launcher real do Mac.
- Na janela do pacote, W moveu o personagem e a câmera; ao insistir na direção,
  a interface informou bloqueio e o movimento ficou limitado. S e D permitiram
  retornar e deslocar lateralmente. O fragmento “Criação de personagem” foi
  visível com acento. As posições observadas e capturas estão no relatório
  `research/private/scene-validation/world-ui-validation.json`.

Os relatórios de processo, pacote, render e builds estão em
`research/private/scene-validation/`. A janela do pacote foi deixada aberta.
CPU, memória e desempenho da cena combinada não foram medidos neste marco.

## Limites e próximo consumidor

Animações de inspeção ainda avançam independentemente do deslocamento, podendo
parecer deslizar. Não há seleção original de andar/correr, combate, interação
com móveis, colisão entre atores, degraus descontínuos, salto ou gravidade.
A colisão cobre somente o perfil geométrico aceito das malhas selecionadas;
não valida a física original nem impede toda interpenetração visual.

A biblioteca de [root motion](root-motion-runtime.md) agora reproduz aplicação
e avanço sob seus contratos isolados, mas não controla esta cena. O próximo
passo exige esclarecer pre-update, componentes ausentes, atributos e seleção
de ações antes de ligar as [curvas exportadas](root-motion-curves.md) ao caçador.
O laboratório de rede também permanece separado: presença e seleção de clip
em loopback não sincronizam posições desta cena nem constituem coop jogável.
