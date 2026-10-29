// Testes do núcleo de execução (src/Engine), sem Qt e sem servidor real.
// Build: ver tools/tests/engine/CMakeLists.txt ou o comando em PROJECT_CONTEXT.md.

#include "AmcpFormat.h"
#include "ClipEndDetector.h"
#include "Conjunto.h"
#include "Sequence.h"
#include "FakePlayoutPort.h"

#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace engine;

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::printf("  FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static ResourceMap stationMap()
{
    ResourceMap map;
    map.set(LogicalResource::Vt, { "S1", 1, 10 });
    map.set(LogicalResource::Selo1, { "S1", 1, 20 });
    map.set(LogicalResource::Selo2, { "S1", 1, 30 });
    return map;
}

static Item item(const std::string& id, LogicalResource resource, const std::string& media = {})
{
    return { id, resource, media.empty() ? id : media, false };
}

static std::optional<FireReport> fireSync(const Conjunto& c, const ResourceMap& map, FakePlayoutPort& port)
{
    std::optional<FireReport> report;
    int calls = 0;
    fire(c, map, port, [&](const FireReport& r) { report = r; ++calls; });
    CHECK(calls <= 1);
    return report;
}

// ---------- Conjunto ----------

static void conjuntoCombinacoesValidas()
{
    const std::vector<std::vector<LogicalResource>> combos = {
        { LogicalResource::Vt }, { LogicalResource::Selo1 }, { LogicalResource::Selo2 },
        { LogicalResource::Vt, LogicalResource::Selo1 }, { LogicalResource::Vt, LogicalResource::Selo2 },
        { LogicalResource::Selo1, LogicalResource::Selo2 },
        { LogicalResource::Vt, LogicalResource::Selo1, LogicalResource::Selo2 } };

    for (const auto& combo : combos)
    {
        FakePlayoutPort port;
        Conjunto c{ "C", {} };
        for (std::size_t i = 0; i < combo.size(); ++i)
            c.items.push_back(item("i" + std::to_string(i), combo[i]));

        const auto report = fireSync(c, stationMap(), port);
        CHECK(report && report->status == FireStatus::Success);
        CHECK(port.calls.size() == combo.size() * 2);
        // Duas fases: todos os prepare antes de qualquer take.
        for (std::size_t i = 0; i < combo.size(); ++i)
        {
            CHECK(port.calls[i].op == "prepare");
            CHECK(port.calls[combo.size() + i].op == "take");
        }
    }
}

static void conjuntoRecursoDuplicadoBloqueado()
{
    FakePlayoutPort port;
    Conjunto c{ "C", { item("a", LogicalResource::Vt), item("b", LogicalResource::Selo1), item("c", LogicalResource::Vt) } };
    const auto report = fireSync(c, stationMap(), port);
    CHECK(report && report->status == FireStatus::Rejected);
    CHECK(report->validation.issues.size() == 1);
    CHECK(report->validation.issues[0].error == ValidationError::DuplicateResource);
    CHECK(report->validation.issues[0].itemId == "c");
    CHECK(port.calls.empty());
}

static void conjuntoVazioBloqueado()
{
    FakePlayoutPort port;
    const auto report = fireSync(Conjunto{ "C", {} }, stationMap(), port);
    CHECK(report && report->status == FireStatus::Rejected);
    CHECK(report->validation.issues[0].error == ValidationError::Empty);
    CHECK(port.calls.empty());
}

static void conjuntoRecursoSemDestinoBloqueado()
{
    FakePlayoutPort port;
    ResourceMap map = stationMap();
    map.remove(LogicalResource::Selo2);
    Conjunto c{ "C", { item("a", LogicalResource::Vt), item("b", LogicalResource::Selo2) } };
    const auto report = fireSync(c, map, port);
    CHECK(report && report->status == FireStatus::Rejected);
    CHECK(report->validation.issues[0].error == ValidationError::UnmappedResource);
    CHECK(report->validation.issues[0].resource == LogicalResource::Selo2);
    CHECK(port.calls.empty());
}

static void conjuntoFalhaNaPreparacaoNaoVaiAoAr()
{
    FakePlayoutPort port;
    port.failOn("prepare", "S1", 1, 20, "arquivo inexistente");
    Conjunto c{ "C", { item("vt", LogicalResource::Vt), item("s1", LogicalResource::Selo1) } };
    const auto report = fireSync(c, stationMap(), port);
    CHECK(report && report->status == FireStatus::Aborted);
    CHECK(report->items[0].outcome == ItemOutcome::NotAttempted);
    CHECK(report->items[1].outcome == ItemOutcome::PrepareFailed);
    CHECK(report->items[1].error == "arquivo inexistente");
    for (const auto& call : port.calls)
        CHECK(call.op != "take");
}

static void conjuntoFalhaParcialNoTake()
{
    FakePlayoutPort port;
    port.failOn("take", "S1", 1, 30);
    Conjunto c{ "C", { item("vt", LogicalResource::Vt), item("s1", LogicalResource::Selo1), item("s2", LogicalResource::Selo2) } };
    const auto report = fireSync(c, stationMap(), port);
    CHECK(report && report->status == FireStatus::Partial);
    CHECK(report->items[0].outcome == ItemOutcome::OnAir);
    CHECK(report->items[1].outcome == ItemOutcome::OnAir);
    CHECK(report->items[2].outcome == ItemOutcome::TakeFailed);
}

static void conjuntoTodosTakesFalham()
{
    FakePlayoutPort port;
    port.failOn("take", "S1", 1, 10);
    port.failOn("take", "S1", 1, 20);
    Conjunto c{ "C", { item("vt", LogicalResource::Vt), item("s1", LogicalResource::Selo1) } };
    const auto report = fireSync(c, stationMap(), port);
    CHECK(report && report->status == FireStatus::Failed);
}

static void conjuntoRespostasAdiadasForaDeOrdem()
{
    FakePlayoutPort port;
    port.deferred = true;
    Conjunto c{ "C", { item("vt", LogicalResource::Vt), item("s1", LogicalResource::Selo1) } };

    std::optional<FireReport> report;
    int calls = 0;
    fire(c, stationMap(), port, [&](const FireReport& r) { report = r; ++calls; });

    CHECK(port.pendingCount() == 2);
    port.release(1);
    CHECK(port.pendingCount() == 1);  // take só depois de todas as preparações
    port.release(0);
    CHECK(port.pendingCount() == 2);  // takes enviados em rajada
    port.release(1);
    CHECK(calls == 0);
    port.release(0);
    CHECK(calls == 1);
    CHECK(report && report->status == FireStatus::Success);
}

static void conjuntoBeforeTakeSoComPreparacaoOk()
{
    FakePlayoutPort port;
    int calls = 0;
    std::size_t callsAtBeforeTake = 0;
    Conjunto c{ "C", { item("vt", LogicalResource::Vt), item("s1", LogicalResource::Selo1) } };
    fire(c, stationMap(), port, [](const FireReport&) {}, [&]() { ++calls; callsAtBeforeTake = port.calls.size(); });
    CHECK(calls == 1);
    CHECK(callsAtBeforeTake == 2);  // depois das 2 preparações, antes de qualquer take

    FakePlayoutPort failing;
    failing.failOn("prepare", "S1", 1, 20);
    int abortedCalls = 0;
    fire(c, stationMap(), failing, [](const FireReport&) {}, [&]() { ++abortedCalls; });
    CHECK(abortedCalls == 0);

    int rejectedCalls = 0;
    Conjunto dup{ "D", { item("a", LogicalResource::Vt), item("b", LogicalResource::Vt) } };
    fire(dup, stationMap(), port, [](const FireReport&) {}, [&]() { ++rejectedCalls; });
    CHECK(rejectedCalls == 0);
}

// ---------- Sequência ----------

struct Recorder
{
    std::vector<SequenceEvent> events;
    SequenceListener listener() { return [this](const SequenceEvent& e) { events.push_back(e); }; }
    SequenceState last() const { return events.empty() ? SequenceState::Idle : events.back().state; }
};

static Sequence threeClips(RepeatMode mode)
{
    return { "SEQ", { item("a", LogicalResource::Vt), item("b", LogicalResource::Vt), item("c", LogicalResource::Vt) }, mode };
}

static void sequenciaUmaVezTermina()
{
    FakePlayoutPort port;
    Recorder rec;
    const ResourceMap map = stationMap();
    SequenceRunner runner(threeClips(RepeatMode::Once), map, port, rec.listener());

    CHECK(runner.start());
    CHECK(runner.state() == SequenceState::Running);
    CHECK(runner.currentIndex() == 0);

    runner.onItemFinished("a");
    CHECK(runner.currentIndex() == 1);
    runner.onItemFinished("b");
    CHECK(runner.currentIndex() == 2);
    runner.onItemFinished("c");
    CHECK(runner.state() == SequenceState::Finished);

    const std::vector<std::string> expected = {
        "prepare:1-10:a", "take:1-10", "prepare:1-10:b",
        "take:1-10", "prepare:1-10:c",
        "take:1-10" };
    CHECK(port.ops() == expected);
}

static void sequenciaLoopVoltaAoInicio()
{
    FakePlayoutPort port;
    const ResourceMap map = stationMap();
    SequenceRunner runner(threeClips(RepeatMode::Loop), map, port);

    CHECK(runner.start());
    runner.onItemFinished("a");
    runner.onItemFinished("b");
    runner.onItemFinished("c");
    CHECK(runner.state() == SequenceState::Running);
    CHECK(runner.currentIndex() == 0);
    runner.onItemFinished("a");
    CHECK(runner.currentIndex() == 1);
    // No último item do loop, o primeiro já foi pré-carregado.
    CHECK(port.ops()[6] == "prepare:1-10:a");
}

static void sequenciaLoopDeUmItem()
{
    FakePlayoutPort port;
    const ResourceMap map = stationMap();
    SequenceRunner runner({ "SEQ", { item("a", LogicalResource::Vt) }, RepeatMode::Loop }, map, port);
    CHECK(runner.start());
    runner.onItemFinished("a");
    runner.onItemFinished("a");
    CHECK(runner.state() == SequenceState::Running);
    CHECK(runner.currentIndex() == 0);
}

static void sequenciaIgnoraFimDeOutroItem()
{
    FakePlayoutPort port;
    const ResourceMap map = stationMap();
    SequenceRunner runner(threeClips(RepeatMode::Once), map, port);
    CHECK(runner.start());
    runner.onItemFinished("b");
    runner.onItemFinished("x");
    CHECK(runner.currentIndex() == 0);
}

static void sequenciaStopTiraDoAr()
{
    FakePlayoutPort port;
    Recorder rec;
    const ResourceMap map = stationMap();
    SequenceRunner runner(threeClips(RepeatMode::Loop), map, port, rec.listener());
    CHECK(runner.start());
    runner.stop();
    CHECK(runner.state() == SequenceState::Stopped);
    CHECK(port.calls.back().op == "stop");
    const std::size_t before = port.calls.size();
    runner.onItemFinished("a");
    CHECK(port.calls.size() == before);
}

static void sequenciaInterrupcaoNaoEnviaComandos()
{
    FakePlayoutPort port;
    const ResourceMap map = stationMap();
    SequenceRunner runner(threeClips(RepeatMode::Loop), map, port);
    CHECK(runner.start());
    const std::size_t before = port.calls.size();
    runner.interrupt();
    CHECK(runner.state() == SequenceState::Interrupted);
    CHECK(port.calls.size() == before);
    CHECK(runner.start());  // pode ser reiniciada depois
}

static void sequenciaErroNoTake()
{
    FakePlayoutPort port;
    Recorder rec;
    const ResourceMap map = stationMap();
    port.failOn("take", "S1", 1, 10, "servidor recusou");
    SequenceRunner runner(threeClips(RepeatMode::Once), map, port, rec.listener());
    CHECK(runner.start());
    CHECK(runner.state() == SequenceState::Error);
    CHECK(rec.events.back().error == "servidor recusou");
}

static void sequenciaErroApontaItemQueFalhou()
{
    FakePlayoutPort port;
    Recorder rec;
    const ResourceMap map = stationMap();
    SequenceRunner runner({ "SEQ", { item("a", LogicalResource::Vt), item("b", LogicalResource::Selo1) }, RepeatMode::Once }, map, port, rec.listener());
    port.failOn("take", "S1", 1, 20, "selo recusado");
    CHECK(runner.start());
    runner.onItemFinished("a");
    CHECK(runner.state() == SequenceState::Error);
    CHECK(rec.events.back().itemId == "a");        // o que estava no ar
    CHECK(rec.events.back().failedItemId == "b");  // o que falhou
    CHECK(rec.events.back().failedIndex == 1);
}

static void sequenciaFimAntesDaPreCarga()
{
    FakePlayoutPort port;
    port.deferred = true;
    const ResourceMap map = stationMap();
    SequenceRunner runner(threeClips(RepeatMode::Once), map, port);

    CHECK(runner.start());
    port.release();  // prepare a
    port.release();  // take a
    CHECK(runner.state() == SequenceState::Running);
    CHECK(port.pendingCount() == 1);  // prepare b em curso

    runner.onItemFinished("a");      // termina antes da pré-carga responder
    runner.onItemFinished("a");      // evento duplicado é ignorado
    CHECK(runner.currentIndex() == 0);
    port.release();                  // prepare b responde -> take b
    CHECK(port.calls.back().op == "take");
    port.release();                  // take b
    CHECK(runner.currentIndex() == 1);
}

static void sequenciaDestruidaComRespostasPendentes()
{
    FakePlayoutPort port;
    port.deferred = true;
    const ResourceMap map = stationMap();
    {
        SequenceRunner runner(threeClips(RepeatMode::Once), map, port);
        CHECK(runner.start());
    }
    port.releaseAll();  // não pode acessar o runner destruído (verificado pelo ASan)
    CHECK(port.pendingCount() == 0);
}

static void sequenciaRecursoSemDestinoNaoInicia()
{
    FakePlayoutPort port;
    ResourceMap map = stationMap();
    map.remove(LogicalResource::Selo1);
    SequenceRunner runner({ "SEQ", { item("a", LogicalResource::Vt), item("b", LogicalResource::Selo1) }, RepeatMode::Once }, map, port);
    CHECK(!runner.start());
    CHECK(port.calls.empty());
}

static void sequenciaTrocaDeDestinoTiraAnterior()
{
    FakePlayoutPort port;
    const ResourceMap map = stationMap();
    SequenceRunner runner({ "SEQ", { item("a", LogicalResource::Vt), item("b", LogicalResource::Selo1) }, RepeatMode::Once }, map, port);
    CHECK(runner.start());
    runner.onItemFinished("a");
    const std::vector<std::string> expected = { "prepare:1-10:a", "take:1-10", "prepare:1-20:b", "take:1-20", "stop:1-10" };
    CHECK(port.ops() == expected);
}

// ---------- AMCP ----------

static void amcpFormataComandos()
{
    const Endpoint vt{ "S1", 1, 10 };
    CHECK(amcp::loadBg(vt, item("a", LogicalResource::Vt, "jornal/abertura")) == "LOADBG 1-10 \"jornal/abertura\"");
    Item looped = item("b", LogicalResource::Vt, "vinheta");
    looped.loop = true;
    CHECK(amcp::loadBg(vt, looped) == "LOADBG 1-10 \"vinheta\" LOOP");
    CHECK(amcp::play(vt) == "PLAY 1-10");
    CHECK(amcp::stop({ "S2", 2, 30 }) == "STOP 2-30");
    CHECK(amcp::pause(vt) == "PAUSE 1-10");
    CHECK(amcp::resume(vt) == "RESUME 1-10");
    CHECK(amcp::clear(vt) == "CLEAR 1-10");
    CHECK(amcp::request("e1", "PLAY 1-10") == "REQ e1 PLAY 1-10");
    CHECK(amcp::quote("a \"b\" c\\d") == "\"a \\\"b\\\" c\\\\d\"");
    CHECK(amcp::quote("not\xC3\xAD" "cias") == "\"not\xC3\xAD" "cias\"");
}

static void amcpLeRespostas()
{
    auto r = amcp::parseResponse("RES e12 202 LOADBG OK\r\n");
    CHECK(r && r->id == "e12" && r->code == 202 && r->text == "LOADBG OK" && r->ok());

    r = amcp::parseResponse("RES e13 404 LOADBG ERROR");
    CHECK(r && r->code == 404 && !r->ok());

    r = amcp::parseResponse("RES e14 200 INFO OK");
    CHECK(r && r->ok());

    CHECK(!amcp::parseResponse("202 PLAY OK"));
    CHECK(!amcp::parseResponse("RES"));
    CHECK(!amcp::parseResponse("RES e15"));
    CHECK(!amcp::parseResponse("RES e16 2x2 PLAY OK"));
    CHECK(!amcp::parseResponse("RESULT e17 202 PLAY OK"));
}

// ---------- Fim de clipe ----------

static void fimDeClipeDisparaUmaVez()
{
    ClipEndDetector d(0.08);
    CHECK(!d.update(0.0, 10.0));
    CHECK(!d.update(5.0, 10.0));
    CHECK(d.update(9.96, 10.0));
    CHECK(!d.update(10.0, 10.0));
    CHECK(!d.update(10.0, 10.0));   // clipe parado no último quadro
}

static void fimDeClipeRearmaComNovoClipe()
{
    ClipEndDetector d(0.08);
    CHECK(d.update(10.0, 10.0));
    CHECK(!d.update(10.0, 10.0));   // pacote atrasado do clipe anterior após o take
    CHECK(!d.update(0.04, 30.0));   // novo clipe começou: rearma
    CHECK(d.update(29.95, 30.0));
}

static void fimDeClipeIgnoraDuracaoDesconhecida()
{
    ClipEndDetector d;
    CHECK(!d.update(100.0, 0.0));
    CHECK(!d.update(200.0, -1.0));
}

static void fimDeClipeQuandoTempoPara()
{
    ClipEndDetector d;
    CHECK(!d.update(9.80, 10.0));   // OSC parou 0,2 s antes do fim
    CHECK(!d.update(9.80, 10.0));
    CHECK(!d.update(9.80, 10.0));
    CHECK(d.update(9.80, 10.0));    // 3 atualizações paradas perto do fim
    CHECK(!d.update(9.80, 10.0));

    ClipEndDetector pausado;        // pausa longe do fim não é fim
    for (int i = 0; i < 10; ++i)
        CHECK(!pausado.update(4.0, 10.0));
}

static void fimDeClipeToleranciaPorQuadro()
{
    ClipEndDetector d;
    d.setFps(25.0);
    CHECK(d.toleranceSeconds() == 0.1);
    CHECK(d.update(9.9, 10.0));     // 2,5 quadros a 25 fps
    ClipEndDetector e;
    e.setFps(59.94);
    CHECK(e.toleranceSeconds() == ClipEndDetector::DefaultTolerance);
    e.setFps(0.0);
    CHECK(e.toleranceSeconds() == ClipEndDetector::DefaultTolerance);
}

static void nomeDoClipeConfere()
{
    CHECK(clipMatches("ABERTURA.mp4", "JORNAL/ABERTURA"));
    CHECK(clipMatches("jornal/abertura.MOV", "JORNAL/ABERTURA"));
    CHECK(clipMatches("JORNAL\\ABERTURA.mxf", "jornal/abertura"));
    CHECK(clipMatches("", "JORNAL/ABERTURA"));
    CHECK(!clipMatches("VINHETA.mp4", "JORNAL/ABERTURA"));
    CHECK(!clipMatches("NOVA_ABERTURA.mp4", "JORNAL/ABERTURA"));  // sufixo sem barra não vale
    CHECK(clipMatches("v1.2/clip.mp4", "V1.2/CLIP"));
}

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        { "conjunto: combinações válidas", conjuntoCombinacoesValidas },
        { "conjunto: recurso duplicado bloqueado", conjuntoRecursoDuplicadoBloqueado },
        { "conjunto: vazio bloqueado", conjuntoVazioBloqueado },
        { "conjunto: recurso sem destino bloqueado", conjuntoRecursoSemDestinoBloqueado },
        { "conjunto: falha na preparação não vai ao ar", conjuntoFalhaNaPreparacaoNaoVaiAoAr },
        { "conjunto: falha parcial no take", conjuntoFalhaParcialNoTake },
        { "conjunto: todos os takes falham", conjuntoTodosTakesFalham },
        { "conjunto: respostas adiadas fora de ordem", conjuntoRespostasAdiadasForaDeOrdem },
        { "conjunto: beforeTake só com preparação ok", conjuntoBeforeTakeSoComPreparacaoOk },
        { "sequência: uma vez termina", sequenciaUmaVezTermina },
        { "sequência: loop volta ao início", sequenciaLoopVoltaAoInicio },
        { "sequência: loop de um item", sequenciaLoopDeUmItem },
        { "sequência: ignora fim de outro item", sequenciaIgnoraFimDeOutroItem },
        { "sequência: stop tira do ar", sequenciaStopTiraDoAr },
        { "sequência: interrupção não envia comandos", sequenciaInterrupcaoNaoEnviaComandos },
        { "sequência: erro no take", sequenciaErroNoTake },
        { "sequência: fim antes da pré-carga", sequenciaFimAntesDaPreCarga },
        { "sequência: erro aponta o item que falhou", sequenciaErroApontaItemQueFalhou },
        { "sequência: destruída com respostas pendentes", sequenciaDestruidaComRespostasPendentes },
        { "sequência: recurso sem destino não inicia", sequenciaRecursoSemDestinoNaoInicia },
        { "sequência: troca de destino tira o anterior", sequenciaTrocaDeDestinoTiraAnterior },
        { "amcp: formata comandos", amcpFormataComandos },
        { "amcp: lê respostas RES", amcpLeRespostas },
        { "fim de clipe: dispara uma vez", fimDeClipeDisparaUmaVez },
        { "fim de clipe: rearma com novo clipe", fimDeClipeRearmaComNovoClipe },
        { "fim de clipe: ignora duração desconhecida", fimDeClipeIgnoraDuracaoDesconhecida },
        { "fim de clipe: tempo parado perto do fim", fimDeClipeQuandoTempoPara },
        { "fim de clipe: tolerância por quadro", fimDeClipeToleranciaPorQuadro },
        { "fim de clipe: nome do clipe confere", nomeDoClipeConfere } };

    for (const auto& [name, test] : tests)
    {
        const int before = failures;
        test();
        std::printf("%s %s\n", failures == before ? "[ok]   " : "[FALHA]", name);
    }
    std::printf("\n%d verificações, %d falhas\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
