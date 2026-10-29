# Contexto do projeto

Memória técnica do CasparClient (fork do CasparCG Client). Decisões, convenções e limitações
necessárias à continuidade. O escopo funcional está no documento "CasparClient — Projeto de Implementação"
(RF-01 a RF-09).

## Decisões

### D-01 — Evoluir o fork, com núcleo de execução separado (2026-10-29)
O CasparClient é construído sobre este fork (C++17 / Qt 6). As regras de execução (Conjunto, Sequência,
roteamento, redundância) ficam em um núcleo novo, `src/Engine`, **sem dependência de Qt**, seguindo o
mesmo padrão do `src/Caspar/MirrorTranslator.h`.

Motivo: no cliente original cada widget do rundown chama o `CasparDevice` diretamente
(ex.: `RundownMovieWidget::executePlay`), o que impede testar a execução sem interface gráfica
(princípio 7 e critério de aceite). O núcleo isola essa lógica; os widgets passam a delegar a ele aos poucos.

### D-02 — Porta de playout assíncrona (2026-10-29)
O núcleo só conversa com o playout pela interface `engine::PlayoutPort` (`prepare`, `take`, `stop`),
com conclusão por callback. Adaptador AMCP, Playout Router (RF-06) e servidor simulado implementam essa porta.
O núcleo não conhece AMCP, versão do CasparCG nem redundância (princípios 2 e 5).

### D-03 — Conjunto em duas fases (2026-10-29)
1. `prepare` em todos os itens (LOADBG). Qualquer falha aborta: nada vai ao ar.
2. `take` em todos, em rajada (PLAY). Falha aqui é parcial e reportada por item.

Sem rollback automático em falha parcial — ver pendências.

### D-04 — Sequência com avanço pelo cliente e pré-carga (2026-10-29)
O próximo item é preparado enquanto o atual está no ar; o avanço acontece quando o fim do item é
informado (`onItemFinished`, a ser alimentado pelo OSC). `stop()` tira do ar; `interrupt()` marca a
sequência como substituída sem enviar comandos. Avanço pelo servidor (`LOADBG ... AUTO`) fica para quando
o adaptador expuser essa capacidade.

## Estrutura do núcleo (`src/Engine`)
- `Resource.h` — recursos lógicos (VT, Selo 1, Selo 2) e `ResourceMap` (recurso → servidor/canal/layer).
- `Item.h` — item executável.
- `PlayoutPort.h` — porta de saída.
- `Conjunto.h` — validação (vazio, recurso duplicado, recurso sem destino) e disparo coordenado.
- `Sequence.h` — `SequenceRunner` (uma vez / loop).

## Testes
Sem Qt e sem servidor real, com playout simulado (`tools/tests/engine/FakePlayoutPort.h`):

    cmake -S tools/tests/engine -B build-engine-tests
    cmake --build build-engine-tests
    ctest --test-dir build-engine-tests --output-on-failure

Em GCC/Clang compila com `-Werror` e ASan/UBSan (desligue com `-DENGINE_SANITIZERS=OFF`).

## Pendências e hipóteses
- Ainda não ligado ao build principal (`src/CMakeLists.txt`) nem aos widgets.
- Rollback em falha parcial de Conjunto: não implementado; decidir com a operação.
- Precedência entre execuções: hipótese adotada é que a execução nova assume o recurso e a anterior
  recebe `interrupt()`. O árbitro de recursos ainda não existe.
- Sincronismo entre recursos em canais diferentes não é garantido pelo take em rajada.
- Fim de item deve vir do OSC; o mapeamento OSC → `onItemFinished` ainda não existe.
- Itens com `loop = true` dentro de Sequência não terminam e travam o avanço; validar regra.
