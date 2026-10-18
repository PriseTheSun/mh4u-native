# Conferência do objetivo do porte

Conferência em 18/10/2026. **O porte completo ainda não está concluído.**
O objetivo permanece Monster Hunter 4 Ultimate em motor nativo próprio para
macOS e Windows, PT-BR, seletor de texturas, joystick configurável e coop online.
Compatibilidade com 3DS foi adiada pelo usuário. Os testes isolados abaixo
não substituem a execução do jogo completo.

| Requisito | Evidência disponível | O que impede concluir |
| --- | --- | --- |
| Motor nativo no Mac | Aplicação C++ arm64 aberta e observada pela interface; caçador real com 7 partes/19 ossos, MOD/LMT direto, pose, skinning e cena com movimento de diagnóstico | Bootstrap, atualização completa, controle original, combate, IA, missões, progressão e saves ainda incompletos |
| Windows nativo | Compilação x64 e testes compilados pelo LLVM-MinGW | Não houve execução em Windows; fluxo jogável também incompleto |
| Recursos e apresentação | Onze entradas do laboratório; duas receitas MOD/LMT; cenário, colisão e texturas extraídos da ROM | Variantes, composição completa, materiais, efeitos, áudio e interfaces necessários ao jogo ainda não cobertos |
| PT-BR | Catálogo nativo com 925 traduções experimentais em 1.082 fragmentos de 10 documentos,157 fallbacks; inventário de 42.196 fragmentos em 285 payloads suportados | Tradução integral, revisão editorial, menus funcionais, variáveis e layout durante gameplay pendentes |
| Seletor de texturas | Packs PNG identificados por hash; modos original/pack/sem textura e fallback com verificações anteriores | Persistência e integração em gameplay; nova tentativa de clicar no painel pela automação não demonstrou alternância |
| Joystick configurável | Código de eixos, deadzone, remapeamento e backup | Controle físico/hotplug e uso no gameplay original não verificados em nenhuma das duas plataformas |
| Online funcionando | Dois clientes GUI nativos e simulação autoritativa de movimento/colisão em loopback, protocolo de até 4 participantes | Ações, combate, inventário, missões, desconexão/reconexão e coop em duas redes externas ainda não verificados |
| Acompanhar os testes | Laboratório 0.17 aberto; janela trazida à frente, animação avançando e comando de reenquadramento observado | A nova API de atualização está sob teste automatizado, ainda não ligada ao loop jogável da janela |

A implementação em andamento compõe a atualização persistente do personagem
até antes de `b148f0`, incluindo eventos da animação real. O seu relatório está
em [player-update-events.md](player-update-events.md). Esse recorte aproxima o
controlador do comportamento original, mas não define a inicialização do jogo
nem completa o restante da rotina.

A sequência necessária continua sendo fechar a atualização e seus produtores
de contexto, ligar o controle e a inicialização reais ao mundo e então avançar
pelos sistemas de combate, IA, missões, progressão e saves. A cooperação deve
transportar esses sistemas compartilhados, não apenas posições. A tradução,
packs e controles precisam ser verificados nesses fluxos reais. A conclusão
exige executar e validar o resultado nas duas plataformas.

Fontes da conferência: `project-status.json`, implementação atual em `src/`,
[roadmap](roadmap.md), [bootstrap](player-gameplay-bootstrap.md),
[scheduler](player-input-scheduler-quest.md),
[catálogo PT-BR](localization-v4.md), [rede local](network-viewer.md) e
`research/private/player-update-events-v1/laboratory-readback.json`.
Os relatórios de teste preservam hashes, alcance e limites de cada ensaio.
