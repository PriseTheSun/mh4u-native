# Índice completo de recursos da ROM

`tools/resources/index_resources.py` permite pesquisar e extrair recursos locais
por identidade, sem repetir a enumeração de milhares de arquivos. O banco SQLite
contém diretórios, nomes originais, campos brutos, offsets, tamanhos e hashes;
não contém os payloads do jogo.

Na ROM SHA-256 `c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666`,
a indexação examinou os **10.592 ARC** do inventário RomFS e registrou
**112.427 membros**, sem arquivo ARC não suportado. Há **1.466 membros** cujo
ID corresponde a `rMotionList`. Isso mede a cobertura dos diretórios, não a
capacidade de decodificar todos esses recursos. Nenhum membro é descompactado
durante a indexação.

## Comandos

Use uma inspeção produzida pelo pipeline de [recursos](resource-formats.md).
A criação verifica novamente o SHA-256 da ROM inteira e exige um destino novo:

```sh
python3 tools/resources/index_resources.py build --inspection research/private/rom-inspection --output research/private/resources.sqlite
python3 tools/resources/index_resources.py summary --index research/private/resources.sqlite
python3 tools/resources/index_resources.py find --index research/private/resources.sqlite --type rMotionList --limit 20
python3 tools/resources/index_resources.py find --index research/private/resources.sqlite --contains em062 --offset 0 --limit 40
python3 tools/resources/index_resources.py extract --index research/private/resources.sqlite --entry 56903 --output research/private/one133.mod
```

O ID de entrada pertence ao banco criado a partir daquele inventário; confira o
nome/arquivo retornado antes de selecionar. `--name` e `--archive` comparam o
valor exato. `--contains` procura uma substring literal. IDs de tipos aceitam
hexadecimal ou o nome ASCII; o hash segue a derivação documentada nas
[evidências REA](rea-findings.md). Calcular um ID não comprova o formato do seu
payload. Paginação limita a saída a até mil registros por pedido.

A extração verifica o hash dos bytes comprimidos selecionados, os limites do
ARC e o tamanho descompactado. Não recalcula o hash da ROM inteira a cada
extração, e declara isso no resultado. Um destino com bytes idênticos pode ser
reutilizado; um conteúdo diferente nunca é substituído. Um banco incompleto
não é consultável. Arquivos ARC não suportados são registrados com a razão;
eles não são convertidos em entradas vazias de sucesso.

## Verificações executadas

- Hash integral da ROM confirmado durante a criação; identidade/tamanho/data
  verificados antes e depois da leitura.
- `PRAGMA integrity_check`: `ok` no banco real de aproximadamente 39 MB.
- A entrada 56903 produziu bytes idênticos ao MOD `one133` já extraído; repetir
  a extração para o mesmo destino preservou o arquivo.
- Sete testes de CLI passaram: roundtrip, limites e exclusividade, integridade
  da origem, rejeição de banco incompleto, ARC truncado com cobertura parcial,
  alteração do payload, intervalo forjado e conflito de destino.

Relatórios locais: `research/private/resource-index-build.json` e
`research/private/index-validation/`. O índice e esses relatórios são resultados
do parser local; não são Evidence emitidos pelo REA. Tempo completo de execução,
CPU, pico de memória e tokens não foram medidos. O banco evita repetir trabalho,
mas não há comparação quantitativa de desempenho entre workflows equivalentes.
