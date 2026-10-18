# Packs de textura do laboratório

O renderer glTF aceita um pack local por execução. A identidade é o SHA-256
dos bytes PNG codificados da imagem original já lida pelo carregador. O pack
substitui essa imagem em todos os materiais que a utilizam, preservando os
samplers. Imagens sem correspondência continuam usando a original. O recurso
extraído e a ROM não são modificados.

```sh
python3 tools/texture_pack.py --name "Meu pack" \
  --replace /caminho/original.png /caminho/substituta.png \
  --output /caminho/novo-pack
python3 tools/launch_lab.py --model appearance \
  --texture-pack /caminho/novo-pack/pack.tsv --texture-mode alternative
```

`--replace` pode ser repetido. O produtor exige diretório novo e registra os
caminhos e hashes dos arquivos de entrada. Ele confere assinatura e tamanho;
o carregador nativo valida a decodificação e os limites de memória. O painel
oferece original, nome do pack e sem textura, com a contagem de imagens
correspondentes. A escolha não é persistida nesta versão. A alternativa PPM
dos modelos OBJ permanece disponível, mas não pode ser combinada com o pack
na mesma execução. Não há pack de melhoria artística incluído.

O manifesto usa UTF-8 com LF final e campos separados por tabulação:

```text
MH4U-TEXTURE-PACK 1
name<TAB>Meu pack
replace<TAB>hash_original<TAB>hash_substituta<TAB>tamanho_codificado<TAB>arquivo.png
```

Hashes são hexadecimais minúsculos de 64 caracteres. Caminhos são relativos à
pasta do manifesto; travessias e links que escapam dessa pasta são rejeitados.
O hash e o tamanho são conferidos sobre os mesmos bytes enviados ao decoder.
Uma identidade original repetida é erro. Um pack sem correspondência na cena
é permitido e usa o fallback integral.

Limites: manifesto de 1 MiB, nome de 120 bytes, 4.096 substituições, 64 MiB por
PNG, 256 MiB agregados codificados e decodificados. O decoder verifica dimensões
até 16.384 e o orçamento RGBA antes de chamar SDL. Esses limites protegem o
carregamento local; não representam limites dos formatos do jogo original.

## Validação do marco 0.6

O teste público `texture-pack-process-smoke` executa o binário real e cobre
correspondência parcial e ausente, duplicação, versão, UTF-8, término do
manifesto, hash/tamanho, travessia, symlink e expansão PNG. Também verifica a
recusa de sobrescrever o diretório do produtor. Os dez testes CTest do projeto
passaram no macOS; o código foi compilado para Windows, sem execução nesse SO.

Capturas determinísticas do binário empacotado mostram a textura verde de
diagnóstico aplicada ao torso, com cabelo, rosto e outras peças mantendo os
recursos originais. O pack substitui uma das nove imagens. Essa cor é uma
fixture de teste, sem pretensão de representar um material do jogo.
`research/private/texture-pack-validation/torso-pack-render.json` preserva a
chamada; `torso-pack-en.png` e `original-ptbr.png` registram os modos.

A janela aberta foi observada via CUA. A alternância por cliques automatizados
não foi confirmada: o trace recebeu down na posição pedida e up em outra
posição, com foco ausente em parte da execução. Não foi aplicada correção
especulativa no SDL/ImGui. A renderização dos dois modos foi verificada por
argumentos de inicialização; isso não comprova a interação manual em Windows.

Materiais, iluminação, seleção durante gameplay e persistência dos packs
continuam pendentes. O PNG convertido é a identidade desta versão do pipeline;
uma nova codificação da mesma imagem produz outro hash e requer outro vínculo.
