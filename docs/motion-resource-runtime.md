# Recurso LMT nativo com identidade e armazenamento imutáveis

`src/motion_resource.hpp/.cpp` lê o LMT67 local, verifica seu SHA-256 e conserva
uma única representação dos fatos usados pelo pedido de animação, preparação
dos canais e amostragem. Não usa código ARM para interpretar o arquivo em
execução. A origem desta implementação são os bytes locais e os contratos
já documentados em [animação](animation-findings.md), [eventos](native-motion-events.md)
e [preparação dos canais](motion-binding-runtime.md).

`Resource::load` e `decode` exigem o hash esperado em hexadecimal minúsculo.
Uma leitura bem-sucedida recebe uma identidade de instância de 64 bits,
processual e nunca reutilizada. Cópias do objeto compartilham armazenamento
e identidade; duas leituras do mesmo conteúdo têm identidades distintas.
O SHA identifica conteúdo, enquanto a identidade de instância impede que um
canal antigo passe a usar silenciosamente o recurso que substituiu seu banco.
Essas identidades não são IDs de rede ou ponteiros do 3DS.

O objeto possui bytes, headers de ações, metadata/pesos dos tracks e registros
de eventos. `request_resource`, `binding_action`, `sampler_track` e
`event_groups` devolvem projeções emprestadas desse armazenamento. O header
referenciado pelo binder é o mesmo do pedido, e as projeções dos eventos
apontam para os mesmos registros. O sampler recebe o buffer completo e seu
offset no arquivo; não reinterpreta headers LMT. Ao menos uma cópia de
`Resource` deve permanecer viva durante o uso das projeções. Como em outros
handles movidos de C++, não se acessa um objeto depois de movê-lo.

Ausência permanece explícita: banco ausente pertence ao `Model` chamador;
tabela LMT de tamanho zero é presente; slot com offset zero é ação ausente;
ponteiro de eventos nulo é diferente de quatro grupos vazios. A tabela pode
preservar mais de 256 slots, mas o seletor recuperado nos consumidores limita
a seleção a oito bits. O recurso não inventa um banco nem um mapa de ossos.

A leitura valida cada região antes do acesso e cobra os limites agregados
antes de alocar objetos. As políticas padrão são 64 MiB, 4.096 slots,
32.768 tracks e 250.000 registros de eventos; não são máximos recuperados do
formato. Mesmo com uma política maior, bytes acima de `UINT32_MAX` são
rejeitados antes da alocação: este perfil usa offsets de 32 bits. A flag
`0x02000000`, que evita a relocação aninhada no carregador original, continua
fora do perfil. O ponteiro de float tracks é preservado e validado como
endereço dentro do arquivo, mas seu payload não é interpretado.

Campos numéricos permanecem como bits ou inteiros originais. Codecs
desconhecidos, contagem de frames zero, loops negativos, pesos não finitos e
durações que excedem a aritmética assinada não são normalizados pelo parser.
O consumidor responsável decide se pode operá-los: o setter, por exemplo,
rejeita overflow da soma assinada dos eventos. Pointers e tamanhos
inconsistentes são erros estruturais, nunca dados vazios substitutos.

## Verificação

`research/private/motion-resource-v1/source-facts.bin` compara todos os fatos
dos dois LMT locais já usados no projeto: 204 slots, 122 ações presentes,
2.565 tracks e 701 registros de eventos. O produtor usa os donos offline
existentes, `animation_probe.parse_lmt` e `lmt_events_probe.event_groups`,
sem outro parser de offsets. SHA do arquivo de comparação:
`33f61c70b8f8e993380e39ef25f98acbea29e9a7914c86efdf3f13acc4d8712f`.
Headers, metadata, bits de floats, presença, limites, prefixos de eventos e
hash de cada buffer de chaves são comparados pelo teste nativo.

O teste também compõe `Resource → motion_request → motion_binding →
motion_sampler`, troca o recurso do banco e verifica a rejeição da identidade
antiga. Inclui cópia/movimento do dono, arquivos truncados, overflow de
offsets, budgets agregados, eventos nulos/vazios, bits não finitos, codecs
desconhecidos e a rejeição semântica pelo consumidor correto.
Foram executadas 130.793 verificações por configuração normal e ASan/UBSan.
O relatório `native-validation.json` registra comandos, hashes antes/depois,
fontes compiladas e tempos. Essa comparação de fatos é offline; não é
execução ARM nem uma sessão do jogo. A [inicialização de animação](motion-initialization-runtime.md)
faz a comparação separada do uso desses recursos com a rotina ARM original.

O recurso ainda não foi ligado ao visualizador entregue. Carregamento global
de bancos, substituição durante gameplay, atualização de joints e blend de
camadas continuam sujeitos aos contratos ainda em reconstrução.
