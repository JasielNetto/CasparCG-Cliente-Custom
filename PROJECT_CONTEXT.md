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

### D-05 — Servidor alvo e correlação de respostas (2026-10-29)
CasparCG Server **2.4.3**. Os comandos do núcleo saem como `REQ <id> <comando>` e só são concluídos
quando chega `RES <id> <código>`, quando o tempo limite expira (padrão 2000 ms) ou quando o servidor
desconecta (`AmcpPlayoutPort`). Respostas `RES` não passam pelo tratamento padrão do `CasparDevice`;
chegam só pelo sinal `AmcpDevice::requestCompleted`. O restante do cliente continua sem `REQ`.

O Mirror replica o comando interno sem o prefixo `REQ`, para a resposta do destino não ser confundida
com a da origem. Se o servidor de origem estiver desconectado, o núcleo recebe falha imediata e não
envia nada (nem ao espelho): o failover de comandos do núcleo fica para o Playout Router (Fase 4).

### D-06 — Fim de clipe pelo OSC (2026-10-29)
`OscClipEndWatcher` assina `/channel/N/stage/layer/L/foreground/file/time` (decorrido, total) e usa
`engine::ClipEndDetector`: dispara uma vez quando faltam menos de 0,08 s e rearma quando o decorrido recua.
Clipes sem duração (streams) nunca disparam. Não reiniciar o detector a cada take (pacote atrasado do
clipe anterior pareceria o fim do novo). O avanço pelo cliente gera um pequeno intervalo entre clipes;
para corte no quadro exato, a evolução é `LOADBG ... AUTO`.

## Estrutura do núcleo (`src/Engine`)
- `Resource.h` — recursos lógicos (VT, Selo 1, Selo 2) e `ResourceMap` (recurso → servidor/canal/layer).
- `Item.h` — item executável.
- `PlayoutPort.h` — porta de saída.
- `Conjunto.h` — validação (vazio, recurso duplicado, recurso sem destino) e disparo coordenado.
- `Sequence.h` — `SequenceRunner` (uma vez / loop).
- `AmcpFormat.h` — comandos LOADBG/PLAY/STOP, `REQ` e leitura de `RES`.
- `ClipEndDetector.h` — detecção de fim de clipe a partir do tempo OSC.

## Integração no Core (Qt)
- `src/Core/AmcpPlayoutPort` — implementa `engine::PlayoutPort` sobre os `CasparDevice` do `DeviceManager`.
- `src/Core/OscClipEndWatcher` — emite `clipEnded()` por destino; alimenta `SequenceRunner::onItemFinished`.
- `src/Caspar/AmcpDevice` — `sendRequest()` e sinal `requestCompleted()`.

## Testes
Sem Qt e sem servidor real, com playout simulado (`tools/tests/engine/FakePlayoutPort.h`):

    cmake -S tools/tests/engine -B build-engine-tests
    cmake --build build-engine-tests
    ctest --test-dir build-engine-tests --output-on-failure

Em GCC/Clang compila com `-Werror` e ASan/UBSan (desligue com `-DENGINE_SANITIZERS=OFF`).

## Pendências e hipóteses
- O núcleo está no build principal, mas nenhum widget ou tela o usa ainda.
- Rollback em falha parcial de Conjunto: não implementado; decidir com a operação.
- Precedência entre execuções: hipótese adotada é que a execução nova assume o recurso e a anterior
  recebe `interrupt()`. O árbitro de recursos ainda não existe.
- Sincronismo entre recursos em canais diferentes não é garantido pelo take em rajada.
- Falta o controlador que liga `OscClipEndWatcher` ao `SequenceRunner` (e a tela que cria Sequências).
- O código Qt novo é validado pelo CI (Windows/Linux/macOS); ainda não foi testado contra servidor real.
- Lote atômico (`BEGIN`/`COMMIT`) para o Conjunto: não confirmado para a 2.4.3; validar antes de usar.
- Itens com `loop = true` dentro de Sequência não terminam e travam o avanço; validar regra.
