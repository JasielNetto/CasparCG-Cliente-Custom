// Testes do modelo de Rundown, da configuração da estação e da formatação de tempo (sem Qt).

#include "Rundown.h"
#include "StationConfig.h"
#include "TimeFormat.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace engine;

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::printf("  FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static Item clip(const std::string& media, LogicalResource resource = LogicalResource::Vt)
{
    Item item;
    item.media = media;
    item.resource = resource;
    return item;
}

static std::vector<std::string> labels(const Rundown& r)
{
    std::vector<std::string> out;
    for (NodeId id : r.flatten())
        out.push_back(r.find(id)->label);
    return out;
}

// ---------- Rundown ----------

static void regrasDeComposicao()
{
    CHECK(Rundown::canContain(std::nullopt, NodeKind::Bloco));
    CHECK(Rundown::canContain(NodeKind::Bloco, NodeKind::Sequencia));
    CHECK(Rundown::canContain(NodeKind::Bloco, NodeKind::Conjunto));
    CHECK(Rundown::canContain(NodeKind::Bloco, NodeKind::Item));
    CHECK(!Rundown::canContain(NodeKind::Bloco, NodeKind::Bloco));
    CHECK(Rundown::canContain(NodeKind::Sequencia, NodeKind::Item));
    CHECK(!Rundown::canContain(NodeKind::Sequencia, NodeKind::Conjunto));
    CHECK(!Rundown::canContain(NodeKind::Conjunto, NodeKind::Sequencia));
    CHECK(!Rundown::canContain(NodeKind::Item, NodeKind::Item));
}

static void inserirRespeitaRegras()
{
    Rundown r;
    const auto bloco = r.addGroup(NodeKind::Bloco, RootId, -1, "Bloco 1");
    CHECK(bloco.has_value());
    CHECK(!r.addGroup(NodeKind::Bloco, *bloco, -1, "aninhado"));

    const auto seq = r.addGroup(NodeKind::Sequencia, *bloco, -1, "Seq");
    CHECK(seq.has_value());
    CHECK(!r.addGroup(NodeKind::Conjunto, *seq, -1, "x"));
    CHECK(r.addItem(*seq, -1, clip("a"), "A").has_value());

    const auto item = r.addItem(RootId, -1, clip("solto"), "Solto");
    CHECK(item.has_value());
    CHECK(!r.addItem(*item, -1, clip("b"), "dentro de item"));
    CHECK(!r.addItem(999, -1, clip("b"), "pai inexistente"));
    CHECK(!r.addGroup(NodeKind::Item, RootId, -1, "item como grupo"));
    CHECK(r.size() == 4);
}

static void ordemDeExibicao()
{
    Rundown r;
    const NodeId b = *r.addGroup(NodeKind::Bloco, RootId, -1, "B");
    r.addItem(b, -1, clip("1"), "B1");
    r.addItem(b, -1, clip("3"), "B3");
    r.addItem(b, 1, clip("2"), "B2");      // insere no meio
    r.addItem(RootId, 0, clip("0"), "R0");  // insere no início da raiz
    const std::vector<std::string> expected = { "R0", "B", "B1", "B2", "B3" };
    CHECK(labels(r) == expected);
    CHECK(r.indexOf(b) == 1);
}

static void removerApagaDescendentes()
{
    Rundown r;
    const NodeId b = *r.addGroup(NodeKind::Bloco, RootId, -1, "B");
    const NodeId s = *r.addGroup(NodeKind::Sequencia, b, -1, "S");
    const NodeId i = *r.addItem(s, -1, clip("x"), "X");
    r.addItem(RootId, -1, clip("y"), "Y");
    CHECK(r.remove(b));
    CHECK(r.find(s) == nullptr);
    CHECK(r.find(i) == nullptr);
    CHECK(r.size() == 1);
    CHECK(!r.remove(b));
    CHECK(!r.remove(RootId));
}

static void moverRespeitaRegrasECiclos()
{
    Rundown r;
    const NodeId b1 = *r.addGroup(NodeKind::Bloco, RootId, -1, "B1");
    const NodeId b2 = *r.addGroup(NodeKind::Bloco, RootId, -1, "B2");
    const NodeId seq = *r.addGroup(NodeKind::Sequencia, b1, -1, "S");
    const NodeId a = *r.addItem(seq, -1, clip("a"), "A");

    CHECK(!r.canMove(b2, b1));
    CHECK(r.canMove(seq, b2));
    CHECK(!r.canMove(RootId, b2));
    CHECK(r.canAdd(b1, NodeKind::Conjunto) && !r.canAdd(seq, NodeKind::Conjunto) && !r.canAdd(777, NodeKind::Item));
    CHECK(!r.move(b2, b1, -1));   // Bloco em Bloco
    CHECK(!r.move(seq, seq, -1)); // em si mesmo
    CHECK(!r.move(seq, a, -1));   // em descendente (Item)
    CHECK(r.move(seq, b2, -1));
    CHECK(r.find(seq)->parent == b2);
    CHECK(r.find(b1)->children.empty());
    CHECK(r.move(a, RootId, 0));
    const std::vector<std::string> expected = { "A", "B1", "B2", "S" };
    CHECK(labels(r) == expected);
}

static void moverDentroDoMesmoPai()
{
    Rundown r;
    const NodeId x = *r.addItem(RootId, -1, clip("x"), "X");
    r.addItem(RootId, -1, clip("y"), "Y");
    r.addItem(RootId, -1, clip("z"), "Z");
    CHECK(r.move(x, RootId, 2));  // posição final 2
    const std::vector<std::string> expected = { "Y", "Z", "X" };
    CHECK(labels(r) == expected);
}

static void converteParaONucleo()
{
    Rundown r;
    const NodeId s = *r.addGroup(NodeKind::Sequencia, RootId, -1, "S");
    const NodeId a = *r.addItem(s, -1, clip("a"), "A");
    const NodeId b = *r.addItem(s, -1, clip("b"), "B");
    CHECK(r.setRepeat(s, RepeatMode::Loop));

    const auto seq = r.toSequence(s);
    CHECK(seq && seq->repeat == RepeatMode::Loop && seq->items.size() == 2);
    CHECK(seq->items[0].id == Rundown::engineId(a) && seq->items[1].media == "b");
    CHECK(Rundown::fromEngineId(seq->items[1].id) == b);
    CHECK(!r.toConjunto(s));

    const NodeId c = *r.addGroup(NodeKind::Conjunto, RootId, -1, "C");
    r.addItem(c, -1, clip("vt", LogicalResource::Vt), "VT");
    r.addItem(c, -1, clip("selo", LogicalResource::Selo1), "Selo");
    const auto conj = r.toConjunto(c);
    CHECK(conj && conj->items.size() == 2 && conj->items[1].resource == LogicalResource::Selo1);
    CHECK(!r.toSequence(c));
}

static void itensDeUmBloco()
{
    Rundown r;
    const NodeId b = *r.addGroup(NodeKind::Bloco, RootId, -1, "B");
    const NodeId i1 = *r.addItem(b, -1, clip("1"), "1");
    const NodeId s = *r.addGroup(NodeKind::Sequencia, b, -1, "S");
    const NodeId i2 = *r.addItem(s, -1, clip("2"), "2");
    const std::vector<NodeId> expected = { i1, i2 };
    CHECK(r.itemsOf(b) == expected);
    CHECK(r.itemsOf(i1) == std::vector<NodeId>{ i1 });
}

static void estadosEErros()
{
    Rundown r;
    const NodeId a = *r.addItem(RootId, -1, clip("a"), "A");
    CHECK(r.setState(a, NodeState::Error, "arquivo inexistente"));
    CHECK(r.find(a)->error == "arquivo inexistente");
    CHECK(r.setState(a, NodeState::OnAir, "ignorado"));
    CHECK(r.find(a)->error.empty());
    r.resetStates();
    CHECK(r.find(a)->state == NodeState::Idle);
    CHECK(!r.setRepeat(a, RepeatMode::Loop));  // só Sequência
    CHECK(r.setExternalId(a, "MOS-123") && r.find(a)->externalId == "MOS-123");
}

static void engineIdInvalido()
{
    CHECK(!Rundown::fromEngineId(""));
    CHECK(!Rundown::fromEngineId("12a"));
    CHECK(!Rundown::fromEngineId("99999999999999999999"));
    CHECK(Rundown::fromEngineId("42") == NodeId(42));
}

// ---------- Configuração da estação ----------

static StationConfig validStation()
{
    StationConfig c;
    c.servers = { { "S1", "10.0.0.1", 5250 }, { "S2", "10.0.0.2", 5250 } };
    c.resources.set(LogicalResource::Vt, { "S1", 1, 10 });
    c.resources.set(LogicalResource::Selo1, { "S1", 1, 20 });
    c.resources.set(LogicalResource::Selo2, { "S1", 1, 30 });
    return c;
}

static bool hasIssue(const std::vector<ConfigIssue>& issues, ConfigError error)
{
    for (const ConfigIssue& issue : issues)
        if (issue.error == error)
            return true;
    return false;
}

static void configValida()
{
    CHECK(validStation().validate().empty());
    CHECK(validStation().findServer("S2") != nullptr);
    CHECK(validStation().findServer("S3") == nullptr);
}

static void configDetectaErros()
{
    StationConfig c = validStation();
    c.servers.push_back({ "S1", "", 0 });
    c.resources.set(LogicalResource::Selo2, { "S1", 1, 20 });  // mesmo destino do Selo 1
    c.oscPort = 70000;
    const auto issues = c.validate();
    CHECK(hasIssue(issues, ConfigError::DuplicateServerName));
    CHECK(hasIssue(issues, ConfigError::EmptyHost));
    CHECK(hasIssue(issues, ConfigError::InvalidPort));
    CHECK(hasIssue(issues, ConfigError::SharedEndpoint));
    CHECK(hasIssue(issues, ConfigError::InvalidOscPort));

    StationConfig d = validStation();
    d.resources.set(LogicalResource::Vt, { "S9", 0, -1 });
    const auto more = d.validate();
    CHECK(hasIssue(more, ConfigError::UnknownServer));
    CHECK(hasIssue(more, ConfigError::InvalidChannel));
    CHECK(hasIssue(more, ConfigError::InvalidLayer));

    StationConfig e;
    e.servers = { { "", "10.0.0.3", 5250 } };
    CHECK(hasIssue(e.validate(), ConfigError::EmptyServerName));
}

static void mesmoLayerEmServidoresDiferentesPode()
{
    StationConfig c = validStation();
    c.resources.set(LogicalResource::Selo2, { "S2", 1, 20 });  // mesmo canal/layer do Selo 1, outro servidor
    CHECK(c.validate().empty());
}

// ---------- Tempo ----------

static void formataTempo()
{
    CHECK(formatTime(0) == "00:00:00.0");
    CHECK(formatTime(83.46) == "00:01:23.4");
    CHECK(formatTime(3725.0) == "01:02:05.0");
    CHECK(formatTime(-3) == "00:00:00.0");
    CHECK(formatTime(83.5, 25) == "00:01:23:12");
    CHECK(formatTime(1.0 / 25 * 24, 25) == "00:00:00:24");
    CHECK(formatTime(10.0, 29.97) == "00:00:10:00");
    CHECK(formatTime(10.999, 29.97) == "00:00:10:29");
    CHECK(formatTime(500000.0) == "99:59:59.0");
    CHECK(remainingTime(8, 10) == 2.0);
    CHECK(remainingTime(12, 10) == 0.0);
}

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        { "rundown: regras de composição", regrasDeComposicao },
        { "rundown: inserir respeita regras", inserirRespeitaRegras },
        { "rundown: ordem de exibição", ordemDeExibicao },
        { "rundown: remover apaga descendentes", removerApagaDescendentes },
        { "rundown: mover respeita regras e ciclos", moverRespeitaRegrasECiclos },
        { "rundown: mover dentro do mesmo pai", moverDentroDoMesmoPai },
        { "rundown: converte para o núcleo", converteParaONucleo },
        { "rundown: itens de um Bloco", itensDeUmBloco },
        { "rundown: estados e erros", estadosEErros },
        { "rundown: id do núcleo inválido", engineIdInvalido },
        { "estação: configuração válida", configValida },
        { "estação: detecta erros", configDetectaErros },
        { "estação: mesmo layer em servidores diferentes", mesmoLayerEmServidoresDiferentesPode },
        { "tempo: formatação", formataTempo } };

    for (const auto& [name, test] : tests)
    {
        const int before = failures;
        test();
        std::printf("%s %s\n", failures == before ? "[ok]   " : "[FALHA]", name);
    }
    std::printf("\n%d verificações, %d falhas\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
