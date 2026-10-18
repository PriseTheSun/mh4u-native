# Preparação das ligações de animação aos ossos

`motion_binding::prepare_tracks` implementa o prefixo de `0x95aa90`, da
entrada até **antes de `0x95acd8`**, e o helper real `0xae923c`. Esse prefixo
seleciona canais de rotação/translação/escala dos ossos e os dois canais do
modelo. Retorna um `Prepared`; ainda é necessário executar a preparação dos
transforms base/anterior no restante de `95aa90` antes de usar essas ligações
no playback. O módulo não consome pending bits em `9577ec`, não amostra
curvas, não mistura camadas e não produz uma pose ou movimento de gameplay.

## Donos dos dados e efeitos

O chamador entrega um `motion_request::Layer`, os canais anteriores, o mapa
de bone ID para índice de joint e uma ação de um recurso imutável verificado.
`Action::header` referencia os fatos já interpretados por `motion_request::Action`;
não há outro parser de cabeçalhos LMT. Frames, atributos, máscaras e event
bindings continuam com `motion_request::Layer`. `Tracks` possui apenas os
descritores novos e a tabela de joints. Track/key references retêm a identidade
do recurso, banco/slot/ordinal ou offset; não seguem a substituição de um banco.
Offset zero presente e ponteiro explicitamente nulo são valores distintos.

Uma ação explicitamente ausente devolve o estado sem mudanças, sem ler mapa
ou converter floats. Para uma ação presente, o prefixo:

1. Zera os dois descritores de canais do modelo, preservando os joints prévios.
2. Converte para binary32 o resultado **unsigned** de `frame_count-1` e o loop
   signed não negativo; loop negativo vira `+0`. Frame count zero dá `2^32`
   após o arredondamento: não aplicamos aqui o limite do sampler posterior.
3. Substitui as quatro event references e acrescenta atributos `0x4000` e
   `0x100000` conforme bits0/1 do cabeçalho. Não recalcula event masks.
4. Percorre todos os tracks na ordem da tabela. O byte alto dos metadados
   escolhe o bone ID; o byte no bit8 escolhe kind. Map255 não tem joint.
5. Kinds0/1/2 ligam rotação/translação/escala do joint, respectivamente; kind2
   acrescenta atributo`0x40`. Kinds3/4 ligam rotação/translação do modelo mesmo
   sem joint. Outros kinds não ligam canal.
6. Sempre que há joint, inclusive com kinds de modelo ou desconhecidos, revisita
   os três pesos e acrescenta bits`1/2/4` para peso diferente de zero e
   `0x10/0x20/0x40` para os bits exatos de`+1`. Duplicatas sobrescrevem o canal,
   mas **não limpam** flags acumulados por tracks anteriores.

O helper copia referência do track, buffer e peso e zera o tempo do descritor.
Os campos de tempo/peso são transportados como bits. A comparação com zero
usa a representação IEEE: ambos os zeros são iguais; subnormais, infinitos e
NaNs são diferentes de zero. Não modelamos FPSCR exception flags ou traps.
Não há aritmética de pesos, normalização ou dedução de ação de caminhada.

O perfil exige storage de joints e bindings possuído, não nulo e disjunto;
pointer wrapping, aliasing com outros buffers e os casos de endereços ARM
nulos após aritmética ficam fora. O mapa só precisa cobrir IDs realmente
acessados. Um índice mapeado fora do storage ou um fato ausente gera erro
transacional. Um event base nulo no original produz `0,72,144,216`, não quatro
grupos ausentes; esse caso é rejeitado por não representar referências válidas.
Os limites configuráveis de4096tracks/256joints são políticas de recursos do
chamador, não uma afirmação dos limites originais. A contagem de tracks deve
caber no u32 original. Conversões de inteiro exigem arredondamento nearest-even;
não geram subnormais. Entradas preservadas podem conter quaisquer bits.

## Evidência e validação

ELF SHA-256 `d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`.
REA/Ghidra sessão1792287542293: leituras057(prefixo),058(helper) e060(literal
zero em95ae38), total624bytes. Todas foram comparadas ao ELF e retidas em
`research/private/motion-binding-v1/evidence/`. A tabela de branches em
`95abd8..95abec` é dado, nunca região executável permitida no harness.

O oracle executou **714 casos** com os bytes originais, incluindo o helper
sem stub. Os470casos sintéticos exercitam os três layers0/1/3 em casos básicos,
layers0..3 nos randomizados, storage de0/1/8/32/255joints, todos os kinds relevantes,
map255, duplicatas, bits preservados, extremos u32/s32, zeros com sinal,
subnormais, infinitos e NaNs. Os244casos adicionais reutilizam os descritores
de122ações reais dos LMT54913/92608, interpretados pelo dono
`tools/resources/animation_probe.py`; os mapas/storage continuam sintéticos,
com uma variante completa e outra esparsa. Isso não comprova o mapa runtime
do caçador nem a associação desses recursos a bancos em gameplay.
O endereço sintético de `model+224` participa apenas da aritmética de
identidade não nula neste prefixo; seu storage não é desreferenciado nem
mapeado pelo ensaio. A tabela de ligações dos joints é alocada e verificada.
Uma extensão ao sufixo deve mapear quaisquer novos dados realmente lidos.

Nos casos presentes o oracle para antes de95acd8 e verifica o stack frame
salvo de56bytes; **não afirma retorno da função completa**. A ação ausente
retorna normalmente. Leituras precisam estar inicializadas; somente stack,
layer selecionado e joints possuídos podem receber writes. Sete controles
negativos cobrem ausência de mapa/joints/tracks/event base, write guard,
orçamento de instruções e tentativa de executar a tabela de branches.
O máximo foi3797instruções por caso, abaixo do limite100000.

O C++ passou **575811 verificações em cada build**, normal`-O2` e
AddressSanitizer+UndefinedBehaviorSanitizer. Frames e todos os campos
representados são comparados por bits/identidade com o oracle, incluindo
referências antigas preservadas e todas as flags dos joints. Arquivos de
entrada foram fixados antes/depois, e as fontes compiladas foram copiadas
para o snapshot. O oracle também verificou que bytes do layer fora dos
campos cobertos não mudaram. Reproduções:

```sh
research/private/root-motion-runtime/venv/bin/python research/private/motion-binding-v1/verify_arm.py
python3 research/private/motion-binding-v1/validate_native.py
```

O gerador ARM recusa sobrescrever o relatório final; use uma pasta de saída
nova para uma nova versão. O teste nativo aceita o `oracle.bin` como argumento;
sem argumento executa apenas os limites públicos. O runtime C++ não executa
ARM e não depende de Unicorn. Os dados privados e a ROM não são vinculados
ao executável. Integração de build e Windows ficam registradas separadamente.

A continuação a partir de95acd8 está implementada separadamente em
[motion_initialization](motion-initialization-runtime.md), que reutiliza
este prefixo e o sampler nativo. O próximo vínculo é `9577ec` sobre pending
layers e a ligação dos transforms resultantes ao clock. A inicialização
diagnóstica existente em `action_motion` permanece com seu escopo anterior.
