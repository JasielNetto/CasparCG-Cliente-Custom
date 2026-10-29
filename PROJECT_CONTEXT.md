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

### D-07 — Sequência mínima no rundown (2026-10-29)
Primeira ligação visível do núcleo, para validar contra o servidor 2.4.3. No menu de contexto do rundown:
**Run Selection as Sequence**, **Run Selection as Sequence (Loop)** e **Stop Sequence**. Usa os itens de
vídeo selecionados, na ordem visual (grupo selecionado inclui seus itens). Regras da versão 1:
- todos no mesmo servidor/canal/layer (vira o recurso VT);
- item com Loop ligado é recusado (nunca terminaria);
- transição, seek/length e atrasos dos itens ainda são ignorados;
- uma sequência por aba de rundown; iniciar outra substitui a anterior;
- o andamento aparece na barra de status; o estado visual dos itens no rundown não é atualizado.

Implementação: `src/Core/SequenceController` (núcleo + `AmcpPlayoutPort` + `OscClipEndWatcher`).
As ações novas ficam no fim do menu porque o restante dele é habilitado por posição.

### D-08 — CasparClient: cliente novo no mesmo repositório (2026-10-29)
Executável novo `casparclient` (`src/CasparClient`, Qt Widgets), ao lado do cliente antigo, que continua
sendo compilado e entregue. Reaproveita `engine`, `playout`, `caspar` e `osc`; **não** usa o banco de dados
nem os widgets do cliente antigo.
- Configuração própria em `station.json` (pasta de configuração do usuário: `QStandardPaths::AppConfigLocation`).
- Rundown salvo em `.ccrundown` (JSON); estados operacionais não são salvos.
- Interface em português; tema escuro (Fusion).
- Tela: biblioteca de mídia | rundown | painéis VT, Selo 1 e Selo 2 (só acompanhamento; Play/Pause/Stop/Clear no botão direito).
- Atalhos: F7 prepara, F8 dispara, F9 para; Ctrl+1/2/3 Bloco/Sequência/Conjunto; Alt+1/2/3 recurso; Ctrl+L loop.
- Porta OSC configurável (padrão 6250). Cliente antigo e novo no mesmo PC disputariam a mesma porta UDP.

Regras de execução (`ExecutionController`):
- por recurso, no máximo um nó **no ar** e um **preparado**; um LOADBG novo substitui o preparado anterior;
- disparar um Item preparado faz só o take; sem preparo, faz LOADBG + PLAY (Conjunto de um item);
- disparar algo num recurso usado por Sequência em execução a substitui (`interrupt`);
- fim de clipe (OSC) avança a Sequência dona do recurso ou marca o item como concluído;
- Bloco não executa.

### D-09 — Regras de segurança da execução, após revisão independente (2026-10-29)
- **Recurso de Sequência em execução é dela.** O preparo manual (F7) nesse recurso é recusado com aviso,
  porque a pré-carga da Sequência usa o mesmo background; disparar direto (F8) continua possível.
  Iniciar uma Sequência descarta o que estava preparado nos recursos dela.
- **Interrupção só depois do sucesso da preparação.** `engine::fire` chama `beforeTake` quando todas as
  preparações deram certo; é ali que a Sequência do recurso é interrompida. Disparo bloqueado ou abortado
  não derruba a Sequência.
- **Play do painel nunca manda PLAY às cegas.** Retoma a pausa ou faz o take do que este cliente preparou;
  sem isso, só avisa "Nada preparado".
- **Fim de clipe confere o nome** (`engine::clipMatches`) com a mídia esperada; tolerância de ~2,5 quadros e
  detecção de tempo parado perto do fim. Pausado não conta como fim.
- **Época.** Trocar de rundown ou de estação incrementa a época; respostas atrasadas da época anterior são
  ignoradas (ids de nó se repetem entre rundowns).
- Item removido do rundown deixa de ser acompanhado (continua no ar no servidor); Sequência com nó removido
  é interrompida. Queda de servidor descarta o que estava preparado nele.
- Stop/Clear só liberam o recurso na tela quando o servidor confirma.
- Tempo limite padrão do AMCP: 5 s. Porta OSC que não abre é avisada na barra de status e nos painéis.
- F7/F8/F9 sem repetição por tecla segurada.

## Estrutura do núcleo (`src/Engine`)
- `Resource.h` — recursos lógicos (VT, Selo 1, Selo 2) e `ResourceMap` (recurso → servidor/canal/layer).
- `Item.h` — item executável.
- `PlayoutPort.h` — porta de saída.
- `Conjunto.h` — validação (vazio, recurso duplicado, recurso sem destino) e disparo coordenado.
- `Sequence.h` — `SequenceRunner` (uma vez / loop).
- `AmcpFormat.h` — comandos LOADBG/PLAY/STOP, `REQ` e leitura de `RES`.
- `ClipEndDetector.h` — detecção de fim de clipe a partir do tempo OSC.
- `Rundown.h` — árvore Item/Bloco/Sequência/Conjunto com regras de composição e estados.
- `StationConfig.h` — servidores, recursos e porta OSC, com validação.
- `TimeFormat.h` — tempo decorrido/restante (segundos reais + quadro).

## Biblioteca Playout (`src/Playout`, Qt, sem banco de dados)
- `AmcpPlayoutPort` — `engine::PlayoutPort` sobre `CasparDevice`; quem cria informa um `DeviceResolver`.
- `OscLayerMonitor` — nome, tempo, pausa, loop, fps e fim de clipe de um destino, a partir de qualquer fonte OSC.

## Integração no cliente antigo
- `src/Caspar/AmcpDevice` — `sendRequest()` e sinal `requestCompleted()`.
- `src/Core/SequenceController` — Sequência mínima a partir de clipes do rundown (D-07).

## Testes
Sem Qt e sem servidor real, com playout simulado (`tools/tests/engine/FakePlayoutPort.h`):

    cmake -S tools/tests/engine -B build-engine-tests
    cmake --build build-engine-tests
    ctest --test-dir build-engine-tests --output-on-failure

Em GCC/Clang compila com `-Werror` e ASan/UBSan (desligue com `-DENGINE_SANITIZERS=OFF`).

## Pendências e hipóteses
- No cliente antigo só a Sequência mínima (D-07) usa o núcleo; o CasparClient (D-08) usa tudo.
- CasparClient: InPoint/OutPoint, seek, transições, preview, dois playouts, MOS e trigger externo ainda não.
- Revisão (pendentes de baixa prioridade): tempo limite reportado como falha embora o comando possa
  executar depois; Conjunto fica "No ar" depois que os filhos saem; arrastar entre duas instâncias do
  cliente; `removeRows` sempre recusado no modelo do rundown.
- Rollback em falha parcial de Conjunto: não implementado; decidir com a operação.
- Precedência entre execuções: hipótese adotada é que a execução nova assume o recurso e a anterior
  recebe `interrupt()`. O árbitro de recursos ainda não existe.
- Sincronismo entre recursos em canais diferentes não é garantido pelo take em rajada.
- O código Qt novo é validado pelo CI (Windows/Linux/macOS); ainda não foi testado contra servidor real.
- Lote atômico (`BEGIN`/`COMMIT`) para o Conjunto: não confirmado para a 2.4.3; validar antes de usar.
- Itens com `loop = true` dentro de Sequência não terminam e travam o avanço; validar regra.
