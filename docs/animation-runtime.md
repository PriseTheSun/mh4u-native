# Interpolação de animação no motor nativo

O consumidor glTF suporta canais de translação, rotação e escala com `STEP`,
`LINEAR` e `CUBICSPLINE`. A seleção pertence ao sampler de cada canal. A extensão
de curvas não altera o comportamento anterior de LINEAR: vetores continuam
lineares e rotações continuam com slerp de menor arco.

CUBICSPLINE lê três linhas por chave — tangente de entrada, valor e tangente
de saída — e requer pelo menos duas chaves. As tangentes são derivadas por
segundo; o cálculo de Hermite multiplica-as pela duração do segmento. Uma
rotação cúbica usa a mesma equação por componente e normaliza somente o
quaternion resultante. Não há troca automática de hemisfério ou normalização
das tangentes. Esses contratos vêm da
[especificação glTF, apêndice C](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#interpolation-cubic).

`src/gltf.cpp` valida a contagem tripla antes de alocar as estruturas do canal.
O orçamento cumulativo de dois milhões de linhas inclui valores e tangentes,
além dos limites já existentes de arquivos, buffers e accessors.
`src/scene.cpp` verifica tempos crescentes, dados finitos e associação a nós
TRS. Valores de rotação cúbica precisam ter norma quadrada a no máximo `1e-4`
de um; são preservados sem reescrita, pois alterar apenas o valor mudaria sua
relação com as tangentes. O resultado é limitado aos valores das extremidades
fora do intervalo de chaves.

Os testes de `src/scene_tests.cpp` passam pelo loader público e pelo sampler:

- Uma curva de translação com chaves em 2 e 4 segundos e tangentes assimétricas
  verifica ordem das linhas, unidades das derivadas e amostras conhecidas.
- Uma curva quaternion com vértices unitários representa o blend de dois
  quaternions brutos de normas diferentes; as amostras verificam que a norma
  dos endpoints originais pode ser preservada pela curva exportada.
- Um segmento com dot negativo verifica que o consumidor cúbico conserva os
  sinais escritos, sem aplicar a política de slerp.
- Contagem de saída incorreta, uma única chave cúbica, tangente não finita e
  vértice quaternion não unitário são rejeitados.

A compilação macOS e as sete suites CTest passaram após essa mudança, incluindo
o recurso real já exportado. Log privado:
`research/private/scene-validation/cubic-build-macos.log`.
O consumidor também compilou para Windows x64, sem execução nesse sistema;
log `research/private/scene-validation/cubic-build-windows.log`.
Esses testes verificam o contrato glTF e o motor local; não demonstram, por si,
equivalência com o sampler ARM ou com transições e estados do jogo.

## Amostras independentes do exportador

O target de diagnóstico `mh4u-animation-samples` usa o mesmo `load_gltf` e
`sample_animation` do renderer. Recebe o caminho da cena como primeiro argumento
e trios `clip node seconds` pela entrada padrão. Cada resposta repete o trio e
acrescenta translação XYZ, rotação XYZW e escala XYZ, totalizando 13 colunas.
Não aplica loop; aceita até 65.536 consultas por processo e rejeita índices ou
tempos inválidos. A impressão conserva precisão suficiente para roundtrip de
float32. É um adaptador de teste, sem matemática paralela de interpolação.

Cinco casos de protocolo com o recurso local constam em
`research/private/scene-validation/animation-samples-adapter.json`. Isso permite
que um verificador compare o executável C++ com equações derivadas do ARM sem
usar a implementação do exportador como única referência.
