# Execução de referência

Azahar é uma ferramenta separada de validação do comportamento original.
O renderer e o motor nativo não ligam seu código nem dependem dele para executar.
Os pacotes nativos não o incluem. A referência emulada também não substitui
comparação em um console real, que não está disponível neste projeto.

Foi instalada localmente a versão arm64 **2126.2**, obtida da
[release oficial](https://github.com/azahar-emu/azahar/releases/tag/2126.2).
O arquivo de 99.740.365 bytes foi conferido contra tamanho e SHA256 publicados
na API da release. O pin fica em `tools/dependencies/azahar-reference.lock.json`;
o ZIP original e a instalação ficam em `.tools/`. Nenhum firmware, chave ou
conteúdo de jogo foi baixado.

O perfil portátil pertence a esta investigação:

```text
.tools/azahar-reference-2126.2/azahar-macos-arm64-2126.2/user/
```

Seu `qt-config.ini` foi criado com a verificação automática de atualizações
desligada. Saves, logs e substituições de recursos ficam nesse perfil. O caminho
`research/private/reference-runtime/Monster Hunter 4 Ultimate.cci` é um symlink
para a ROM fornecida, com extensão de contêiner NCSD; os bytes originais não
foram modificados.

Na primeira tentativa com `open .../Azahar.app --args -w ...`, o processo ficou
mais de quatro minutos antes de criar uma janela. A amostragem da pilha mostrou
`UserDataMigrator → FileUtil::SetUserPath → getcwd → open`. A causa do bloqueio
não foi determinada. O processo da investigação foi encerrado após conferir
caminho e horário de criação. Não houve alteração de permissões do macOS.

A execução direta abaixo abriu o aplicativo e carregou a ROM:

```sh
cd .tools/azahar-reference-2126.2/azahar-macos-arm64-2126.2
./Azahar.app/Contents/MacOS/azahar -w \
  '../../../research/private/reference-runtime/Monster Hunter 4 Ultimate.cci'
```

Use um caminho absoluto para a ROM se iniciar de outra pasta. A aplicação
apresenta um aviso de que a execução direta pode limitar funcionalidades como
a câmera; ele foi reconhecido na interface. Observamos a criação dos dados
locais, a abertura em vídeo e a tela de título. A interface reportou o backend
Vulkan/MoltenVK. Isso confirma execução da ROM, não fidelidade de todos os
serviços ou desempenho do futuro motor.

As ações de interface foram realizadas via CUA. O perfil padrão observado
mapeia `A` do 3DS para a tecla `a`, `B` para `s`, Start para `m`, D-pad inferior
para `g` e Circle Pad para as setas. Não há conexão online nem conta configurada
nessa referência.

Para validar a tradução, uma cópia derivada de `eng/data/core_title.arc` pode
ser colocada em `user/load/mods/0004000000126100/romfs/eng/data/core_title.arc`.
Esse espelhamento de caminhos é descrito no
[guia de modding mantido pelo projeto Azahar](https://citra.azahar-emu.org/help/feature/game-modding/).
O original fica intacto. O log observou explicitamente o uso dessa substituição
após reiniciar a ROM. Consulte a evidência de texto em
`research/private/localization/title-reference/`.

As evidências de instalação, tentativa inicial e lançamento ficam em
`research/private/reference-runtime/`. Arquivos do perfil e recursos derivados
não são artefatos de distribuição pública.

## Preferências e capturas

Abrir as preferências de Azahar bloqueou outra execução. A amostragem mostrou
`ConfigureAudio::SetConfiguration → AppleAuthorization::CheckAuthorization`
esperando em uma variável de condição. Não foi concedida permissão de microfone
nem determinada a causa interna. O processo identificado da investigação não
encerrou com SIGTERM; após reconferir caminho e nascimento, foi encerrado com
SIGKILL. A tentativa e a escalada estão em `preferences-stall.json` e nas
amostragens ao lado dele.

O caminho de capturas estava vazio e uma primeira tentativa de salvar na raiz
falhou. Para evitar reabrir a janela de preferências, ajustou-se somente
`Paths\\screenshotPath` do perfil portátil, com backup
`qt-config.before-screenshot-path.ini`, para
`research/private/localization/title-reference/captures/`. Após reiniciar,
Command+P produziu os PNGs de título e opções. As capturas foram conferidas e
seus hashes ficaram em `title-reference/runtime-validation.json`.

A referência foi pausada após essas observações. Essa execução não testou
combate, missões ou rede. O laboratório nativo permaneceu separado e aberto.

Uma observação posterior abriu a criação de personagem e validou 14 fragmentos
do rascunho CharMake. As capturas e os hashes estão em
`research/private/localization/title-charmake-reference/runtime-validation.json`.
O perfil de referência teve Circle Pad Pro desativado pela interface; não foi
criado nem concluído um personagem salvo. Teclas e menus AX funcionaram, mas
toques por coordenadas retornaram `windowNotFoundAtPosition` no CUA, impedindo
completar o teclado de nome nessa rodada. A origem dessa falha de automação não
foi estabelecida. A referência ficou pausada novamente, com 0%/0 FPS observado.

O teste posterior de [botões compartilhados](localization-common-buttons.md)
identificou Common60/61 por marcadores temporários. Após a restauração do teste,
um override final separado de `core_common.arc` passou a exibir **Voltar** e
**Confirmar**. O arquivo foi adicionado somente ao perfil portátil do projeto;
o override Title/CharMake foi preservado byte a byte. A captura final mostra as
palavras completas, e a interface confirmou pausa na criação de personagem.
Nenhum personagem foi criado; 20 arquivos de SDMC/NAND comparados antes e depois
ficaram idênticos. Relatórios e cópias congeladas estão em
`research/private/localization/common-buttons-reference/`.
