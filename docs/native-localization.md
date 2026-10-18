# Catálogo de localização para o motor nativo

O módulo `src/localization.hpp/.cpp` lê um pacote UTF-8 local e resolve texto por
**documento, SHA-256 original e índice de fragmento**. Não lê LMD nem executa
seus comandos. `tools/localization/native_catalog.py` usa `lmd.py` como único
dono da interpretação LMD e produz os dados consumidos pelo C++.

O pacote de desenvolvimento inclui 439 fragmentos originais de Title (136),
CharMake (179) e Common (124), com **282 traduções diferentes do original**.
Os outros 157 fragmentos retornam o texto original. Os catálogos selecionam 283 entradas;
HR é idêntico à fonte e permanece em fallback, sem inflar a cobertura traduzida.
Este conjunto não representa todos os textos do jogo.

## Construção reproduzível e identidade

A partir da raiz do projeto:

```sh
python3 -B tools/localization/native_catalog.py build \
  --document research/private/localization/Title.unchanged.lmd localization/pt-BR/title-expanded.seed.json \
  --document research/private/localization/CharMake.unchanged.lmd localization/pt-BR/charmake.seed.json \
  --document research/private/localization/common-button-probe/Common.source.lmd localization/pt-BR/common-expanded.seed.json \
  --output assets/local/localization-native-v2
```

Cada par passa por `lmd.decode`, roundtrip inalterado, `lmd.load_catalog`,
reconstrução e leitura comparada dos fragmentos. O produtor verifica os fatos
opcionais do seed contra a fonte: key, offset, referências, tokens, tamanho e
marcadores de idioma. Controles, placeholders, quantidade/ordem de quebras,
linhas vazias e quebra final são preservados. Não há normalização Unicode nem
fusão de fragmentos. Seeds em outro idioma, duplicatas de documento e mudanças
nos fatos de identidade falham explicitamente. O idioma da fonte é selecionado
pelo chamador; não é inferido pelo conteúdo textual. Os três seeds utilizados
possuem marcadores de origem inglesa verificados.

Saídas atuais em `assets/local/localization-native-v2/`:

| Arquivo | Função |
| --- | --- |
| `catalog.mhtxt` | Dados UTF-8: originais, traduções e metadados opacos |
| `catalog_identity.hpp` | Header gerado de nomes, hashes e contagens; nenhum texto do jogo |
| `manifest.json` | A mesma identidade em JSON para empacotamento/proveniência |
| `provenance.json` | Fontes selecionadas, hashes do produtor/parser/seeds e validações |
| `lookup-oracle.tsv` | Oracle privado de teste, com bytes UTF-8 em hexadecimal |

O pacote v2 tem 251.466 bytes e SHA-256
`e1edd7defeef717fd7a07a1088a9ee62377a068cadf4417f6b197f0ec8007d0d`.
O pacote anterior permanece preservado em `assets/local/localization-native/`.
As saídas que contêm textos originais permanecem locais/ignoradas. O header
contém somente metadados de identidade, mas também é gerado localmente. Uma
revisão de tradução exige regenerar o pacote/header e recompilar o consumidor;
não exige editar manualmente o código C++. O produtor aceita reexecução
idêntica e recusa substituir saídas diferentes. Para outra revisão, use outra
pasta de saída ou preserve explicitamente a revisão anterior antes de removê-la.

**O hash do pacote deve vir do header/manifest selecionado independentemente.**
Apenas repetir dentro do pacote o hash do LMD original não prova que os textos
correspondem àquele LMD. O loader verifica o SHA-256 dos bytes completos contra
o pin esperado e depois verifica documento/hash/contagem/locale. Um manifesto
adversarial fornecido junto de um pacote adversarial não cria confiança por si
só; este mecanismo verifica consistência com a revisão escolhida pelo projeto.

## API C++20

Namespace `mh4u::localization`, sem dependência de SDL, ImGui ou Ghidra:

```cpp
struct DocumentIdentity {
    std::string document;
    std::string source_sha256;
    std::uint32_t fragment_count;
};
struct ExpectedCatalog {
    std::string locale;
    std::string package_sha256;
    std::vector<DocumentIdentity> documents;
};
template<class T> using Result = std::variant<T, Error>;

Catalog::load(path, expected, limits = {}) -> Result<Catalog>;
Catalog::decode(bytes, expected, limits = {}) -> Result<Catalog>;
catalog.lookup(identity, index, Language::translated) -> Result<TextView>;
catalog.lookup(identity, index, Language::english) -> Result<TextView>;
catalog.producer_metadata(identity) -> Result<std::string_view>;
catalog.identities() -> const std::vector<DocumentIdentity>&;
catalog.locale() -> std::string_view;
catalog.package_sha256() -> std::string_view;
catalog.fragment_count() -> std::size_t;
catalog.translated_count() -> std::size_t;
```

`TextView` contém `text`, `original`, `locale`, `source_offset` e `translated`.
Os primeiros três campos são `std::string_view`. O catálogo possui as strings;
as views exigem que ele continue vivo e não seja movido. A origem de offsets é
o LMD original, não a reconstrução traduzida. Nome/hash/contagem são comparados
exatamente, sem aliases ou normalização.

`Language::english` retorna sempre o original. `Language::translated` retorna
a tradução quando presente ou o original com `locale="en"` e
`translated=false`. Tradução explicitamente vazia é diferente de ausência.
Documento, variante ou índice desconhecido retorna erro: não há original
confiável para inventar um fallback nesses casos.

Integração mínima com o header gerado:

```cpp
#include "localization.hpp"
#include "catalog_identity.hpp"
#include <algorithm>

namespace loc = mh4u::localization;
auto loaded = loc::Catalog::load(catalog_path, loc::generated::expected_catalog());
if (const auto* error = std::get_if<loc::Error>(&loaded)) {
    // Reportar error_name(error->code), error->message e error->byte_offset.
    // Não carregar silenciosamente outra variante.
} else {
    auto& catalog = std::get<loc::Catalog>(loaded);
    const auto& ids = catalog.identities();
    const auto doc = std::find_if(ids.begin(), ids.end(), [](const auto& item) {
        return item.document == "CharMake";
    });
    if (doc != ids.end()) {
        auto result = catalog.lookup(*doc, 5, loc::Language::translated);
        // Manter catalog vivo; tratar Error antes de usar TextView.
    }
}
```

Para o painel de criação, a seleção inglês/PT-BR muda `Language` na consulta;
não requer recarregar o arquivo. Passe o texto ao renderer como texto literal
com comprimento explícito, por exemplo `TextUnformatted(begin,end)` no ImGui.
Não o use como uma format string nem interprete placeholders, códigos privados
ou a metadata como instruções. Os fragmentos continuam separados; o módulo não
resolve sequências de labels/controles nem simula o fluxo original da tela.

O header público é `src/localization.hpp`. A implementação e os testes precisam
de `vendor/picosha2` como include privado; o aplicativo inclui `src` e a pasta
do header gerado e liga `localization.cpp`. `picosha2` já está vendorizado no
projeto. A integração CMake e o painel pertencem ao responsável pelo motor;
os pacotes congelados anteriores e o perfil de referência não são modificados.

## Formato e limites

Magic exato: `MH4U-NATIVE-TEXT 1\n`. Todos os campos seguintes usam netstrings:
`comprimento_decimal:bytes_UTF8,`. O comprimento conta **bytes**, não caracteres
nem unidades UTF-16. O decimal é sem sinal, sem zeros iniciais, dentro de uint32.
Vírgulas, dois-pontos e quebras dentro do payload não são separadores externos.
UTF-8 é validado estritamente, sem sequências excessivamente longas, surrogates
ou valores acima de U+10FFFF. NUL embutido é rejeitado como no perfil LMD atual.

Ordem dos campos:

1. Locale alvo (`pt-BR` ou `en`), locale da fonte (`en`), perfil do produtor
   (`mh4u-lmd-0001010b-contiguous-v1`), quantidade de documentos.
2. Para cada documento em ordem lexical: nome, SHA-256 original, quantidade de
   fragmentos, metadata opaca do produtor.
3. Para cada fragmento do documento: índice, offset original, texto original,
   presença de tradução (`0`/`1`) e texto traduzido. Com presença 0, o payload
   traduzido deve estar vazio. A sequência de índices é exatamente 0..N−1.
4. Nenhum byte extra após o último campo.

O JSON opaco conserva cabeçalho, labels, controles, descritores, padding,
referências fornecidas pelo parser, tokens e unknowns. O C++ apenas preserva
esse campo; não implementa um segundo parser LMD nem atribui significado aos
opcodes. O formato nativo não comporta alterações no LMD fonte: qualquer nova
variante exige outra identidade e um produtor que a suporte explicitamente.

Limites locais padrão: 8 MiB por pacote, 64 documentos, 65.536 fragmentos no
conjunto, 256 KiB por texto e 4 MiB por metadata de documento. São políticas de
recurso, não alegações de limites do jogo. O produtor contabiliza a expansão
UTF-8/framing antes de montar o pacote e limita o agregado enquanto lê os
pares. O consumidor valida limites antes das alocações correspondentes; não
reserva vetores a partir de contagens de registros ainda não examinados.

Erros tipados: `io`, `resource_limit`, `invalid_expected_identity`,
`package_hash_mismatch`, `unsupported_variant`, `invalid_utf8`,
`malformed_package`, `duplicate_identity`, `source_identity_mismatch`,
`missing_document` e `missing_fragment`. Não há fallback silencioso após erro
de identidade ou formato. O modelo cobre falhas esperadas de dados/I/O;
falhas globais de alocação do sistema não são convertidas em dados válidos.

## Verificação executada da primeira revisão (160 traduções)

```sh
python3 -B tools/localization/native_catalog.py self-test
clang++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
  -Ivendor/picosha2 src/localization.cpp src/localization_tests.cpp \
  -o research/private/localization/native-validation/localization-tests
research/private/localization/native-validation/localization-tests \
  assets/local/localization-native/catalog.mhtxt \
  96b26dea4d8feefc3b2b2f32a062964ffc1875ceaf97d4c736b4df12684193b3 \
  assets/local/localization-native/lookup-oracle.tsv
```

Apple clang 21/ARM64 compilou sem warnings com `-Werror`. Passaram 12 verificações
sintéticas do produtor e 3.446 asserções C++, incluindo os 439 fragmentos reais
comparados **uma vez por identidade/índice** com o oracle Python, em ambos os
idiomas. Traduções=160, fallbacks=279. Outra compilação e execução passaram com
AddressSanitizer e UndefinedBehaviorSanitizer, sem diagnóstico. Um programa
separado compilou o header gerado, carregou o pacote e verificou Common 60/61 e
o fallback 62; nenhum hash precisou ser editado nesse consumidor.

As verificações incluem UTF-8 inválido, formato/perfil/locale desconhecidos,
comprimentos e decimais inválidos, truncamento em cada prefixo do fixture,
trailing data, duplicatas, limites de memória, hash divergente, leitura de
arquivo ausente/diretório, seleção de idioma inválida e preservação de texto
vazio, quebras e caracteres privados. Os fixtures sintéticos são texto público
criado para teste; o oracle real fica somente na pasta local.

Medição de execuções completas após aquecimento, sem limpar cache: produtor
idempotente 0,09 s wall / 0,08 s CPU / 19.480.576 bytes RSS máximo; suíte C++ 0,32 s
wall / 0,01 s CPU / 3.096.576 bytes RSS máximo. São medições de processos únicos,
não benchmarks isolados de lookup. Não houve medição fria, comparação com
implementação anterior ou contagem de tokens.

Relatório e hashes:
`research/private/localization/native-validation/validation.json`, junto de
stdout/stderr, métricas, fixture de integração e executáveis locais. Esses testes
isolados não estabelecem fidelidade visual ou execução do jogo.

## Integração no laboratório 0.6

O viewer foi ligado à biblioteca, compilado em macOS ARM64 e compilado para
Windows x64. Os dez testes CTest passaram no Mac. O pacote inclui o catálogo
e sua proveniência em `assets/texts/`; os launchers de ambos os sistemas o
selecionam explicitamente. As nove opções de recurso carregaram o catálogo
com o binário macOS empacotado. Windows ainda não foi executado.

`--text-catalog`, `--text-language pt-BR|en`, `--text-document` e
`--text-fragment` selecionam a inspeção. O painel mostra documento, índice,
idioma e fallback. A biblioteca conserva os controles como dados e o painel
usa texto sem formatação interpretada. Não executa opcodes LMD.

Capturas do binário empacotado verificaram Common 60 como “Voltar” e “Back”:
`research/private/texture-pack-validation/original-ptbr.png` e
`torso-pack-en.png`. A janela persistente foi observada via CUA; a alternância
por clique automatizado não foi confirmada devido à divergência de posição
entre os eventos down/up, registrada em `ui-trace.log` da mesma pasta.
Os modos foram selecionados por CLI para a verificação visual. Não houve
auditoria visual de todos os 439 fragmentos, nem implementação do layout,
comandos ou fluxo dos menus originais.

## Revisão nativa v2 (marco 0.9)

Os seeds `title-expanded.seed.json` e `common-expanded.seed.json` ampliam o
catálogo para Title=58, Common=74 e CharMake=150 traduções diferentes da fonte.
São 122 traduções novas. Os seeds anteriores e os overrides de referência
continuam congelados em 160; nenhuma validação do emulador é transferida às
novas entradas. Em particular, o consumidor original de Common:000062 continua
não identificado, mesmo que sua tradução já esteja disponível no catálogo nativo.

O produtor valida roundtrip inalterado byte a byte e leitura de todas as
traduções após reconstrução pelo dono LMD. Mantém controles, tokens, quebras,
linhas vazias e espaços de junção inicial/final. O CTest
`real-localization-catalog-v2` compara os 439 fragmentos nos dois idiomas, com
pin independente e contagem esperada 282. O argumento opcional de contagem no
teste permite escolher a revisão; não deriva a cobertura do pacote testado.

A nomenclatura das 14 armas adapta os termos em português de fontes oficiais
atuais: [Monster Hunter Now](https://monsterhunternow.com/pt_br/weapons/banbaro_lightbowgun)
e [Monster Hunter Wilds na PlayStation](https://www.playstation.com/pt-br/games/monster-hunter-wilds/).
Isso é uma escolha editorial deste projeto, não uma tradução oficial de MH4U.
NPCs com nomes/títulos ainda sem glossário aprovado, autônimos de idiomas,
símbolos e DUMMY permanecem na fonte. Palico permanece como na revisão anterior.
O trocadilho de Title:000000 foi adaptado para uma recomendação de descanso.

Referências a Nintendo 3DS, cartão SD, Circle Pad Pro e manutenção de servidores
são textos arquivados da fonte. Exibi-las no inspetor não implementa esses
recursos no motor nativo. Os textos são rascunhos; revisão linguística contextual,
layout das telas originais e cobertura completa do jogo continuam pendentes.

O pacote 0.9 passou 20 testes CTest, incluindo 3.446 asserções de catálogo
com os 439 fragmentos em ambos os idiomas. As capturas do binário empacotado
mostram Common:56 em PT-BR/inglês e Title:22 com acentos e parágrafo vazio.
O launcher aberto mostra “Lâmina Dínamo”. Isto verifica três casos de inspetor,
não todos os glifos nem as telas originais. Relatório:
`research/private/localization/native-v2-validation.json`.
