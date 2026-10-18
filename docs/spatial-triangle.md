# Consulta vertical original tipo21

`spatial_triangle` implementa a folha numérica `cc94f0 → 1eb400 → bdcf50` e expõe separadamente o produto escalar de `193c18..193c44`. Usa os vértices fornecidos pelo dono do recurso. Não interpreta SBC, percorre árvores, escolhe objetos, executa callbacks ou substitui a busca original por um raycast genérico.

A fonte está descrita em [player-spatial-context-v2.md](player-spatial-context-v2.md). O novo componente usa os readbacks e resultados ARM já retidos; não houve nova sessão REA ou execução ARM.

## API e propriedade

`Triangle` contém três posições XYZ como palavras raw float32. `Vertical` contém origem XYZ e direção Y opcionais: ausência significa desconhecido. `intersect(triangle,vertical,write_hit=true)` retorna `variant<Output,Error>`. `Output.intersects` distingue hit de miss; `Output.hit` contém um resultado somente quando houve hit e a escrita foi solicitada. `write_hit=false` mantém todos os cálculos e a decisão. Não há estado persistente nem mutação das entradas.

Os offsets abaixo são hexadecimais. O registro `Hit` contém posição XYZW, normal XYZW, a palavra em `+20`, o parâmetro t em `+24` e o byte em `+28`. Os dois W e `+20` são +0; o byte é 1. O struct C++ pode ter padding: consumidores projetam os campos, nunca presumem que `sizeof(Hit)==41` ou copiam sua representação de memória como o registro ARM.

`normal_gate(source_normal,test_vector)` retorna **true para aceitar**, pois o produto escalar em ordem float32 é <=0. Não normaliza a normal fonte. Essa operação representa apenas o gate de `193884`, sem alegar seus ramos de transformação, seleção de normal ou callbacks.

A normal do hit e a normal fonte são propriedades diferentes. O hit usa o produto vetorial dos vértices, normalizado condicionalmente, e sua orientação depende do sinal da distância ao plano. A normal fonte continua com o dono do SBC/atributo; não é sobrescrita pela normal do hit.

## Operações recuperadas

Os dois vetores de aresta e o produto vetorial seguem a ordem de `cc94f0`. O comprimento é a raiz quadrada de X²+Y²+Z², com multiplicações e somas separadas. A comparação da representação assinada com `0x34000000` decide se aplica o recíproco do comprimento. Um triângulo pequeno não é automaticamente erro: o original pode conservar sua normal sem normalizar e prosseguir.

A interseção vertical rejeita quando `abs(normal.y*directionY)` tem bits inferiores a `0x3727c5ac`. A origem só é consumida depois desse teste; portanto direção paralela pode produzir miss com origem desconhecida. Uma direção desconhecida é erro, mesmo para normal zero, pois o original a lê e multiplica.

A distância ao plano preserva a associação original: o termo X é somado ao termo constante; os termos Y/Z são combinados separadamente; somente depois os dois resultados são somados. O parâmetro t é a distância negada dividida pelo denominador. t negativo rejeita; não foi acrescentado um limite superior. A posição copia X/Z da origem e calcula Y por multiplicação e soma separadas.

Os três testes orientados em XZ são todos calculados antes das comparações. Igualdade aceita bordas e vértices. Inverter a ordem dos vértices pode mudar a aceitação; a rotina não foi substituída por uma interseção baricêntrica de duas faces. Um miss não devolve o resultado temporário calculado para o plano.

## Domínio e falhas

O perfil admite operandos consumidos e intermediários finitos em binary32 IEEE754, arredondamento ao mais próximo e subnormais preservados. `motion_transform_math::multiply/add` conserva o dono existente dessas operações e da verificação do ambiente; subtração usa ADD com inversão do sinal do segundo operando, equivalente nesse domínio inclusive para zero com sinal. Divisão e raiz locais preservam a ordem da folha e só ocorrem após a checagem do ambiente. Fast-math é recusado; os testes compilam com `-ffp-contract=off`.

Os erros distinguem operando desconhecido, operando não finito, overflow e ambiente incompatível, mantendo a causa do dono aritmético quando aplicável. Dados não consumidos não são validados antecipadamente: uma origem com NaN pode permanecer ignorada no ramo paralelo. Esses erros são o limite explícito da API nativa, não retornos ou exceções atribuídos ao jogo original.

## Evidência

`research/private/spatial-triangle-native-v1/geometry-owner/source-readback.json` verifica cinco readbacks, total 2200 bytes, contra o ELF SHA-256`d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435`. Eles cobrem os 968 bytes solicitados das três funções/literais e do gate. O readback maior de `193884` contém ramos adicionais que este módulo não implementa.

O comparador independente, em `oracle-adapter/`, reusa os registros ARM originais: 1464 consultas, 102 hits e 1362 misses. Os 4182 bytes dos 102 registros de hit coincidiram byte a byte. Os inputs incluem os vértices do SBC fixado e distinguem a normal fonte do resultado. Essa contagem corresponde a consultas individuais dentro dos 274 casos retidos, não a 1464 novas execuções ARM. A enumeração explícita dos pares no experimento não comprova a busca original completa do manager.

Os testes públicos verificam 88 condições adicionais, incluindo: direção ascendente/descendente, t negativo e sem máximo artificial, igualdade nas bordas, winding, sinal de zero, ambos limiares, normal pequena não normalizada, output ausente, dados desconhecidos ignorados/consumidos, overflow, não finitos e ambiente float. São testes nativos derivados do contrato; não ampliam a cobertura de execução ARM. As compilações normal e ASan/UBSan/float-cast-overflow estritas exigem exit0, stderr vazio, contagem exata e fontes imutáveis antes/depois.

Continuam fora do componente: montagem do carrier, pares com duas faces, transformações, formas alternativas, busca de objetos/grupos/árvores, filtro dinâmico, callback de atributos, lista/seleção entre múltiplas superfícies, unidades físicas, integração no Player e scheduler.
