// Teste do tradutor do Mirror (sem Qt).
//
//   g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src/Caspar tools/tests/mirror_translator_test.cpp -o /tmp/mirror_test && /tmp/mirror_test
//
// No Windows (Developer Command Prompt):
//   cl /std:c++17 /EHsc /W4 /I src\Caspar tools\tests\mirror_translator_test.cpp

#include "MirrorTranslator.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static int failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::printf("  FALHA linha %d: %s\n", __LINE__, #cond);                    \
            ++failures;                                                               \
        }                                                                             \
    } while (0)

static mirror::Rule rule(int from = 1, int to = 2, std::uint32_t verbs = mirror::DefaultVerbs, bool enabled = true)
{
    mirror::Rule r;
    r.enabled = enabled;
    r.sourceChannel = from;
    r.targetChannel = to;
    r.verbs = verbs;
    return r;
}

// Devolve a mensagem traduzida, ou "<nao>" se nao deve ser replicada.
static std::string tr(const std::string& msg, const mirror::Rule& r)
{
    std::string out = "SENTINELA";
    if (!mirror::translate(msg, r, out))
    {
        CHECK(out == "SENTINELA"); // 'out' nao pode ser tocado quando nao replica
        return "<nao>";
    }
    return out;
}

int main()
{
    using namespace mirror;

    // --- verbos padrao: PLAY, LOAD, LOADBG, STOP ---
    CHECK(tr("PLAY 1-10 \"jornal/abertura\" MIX 12 LINEAR RIGHT", rule()) == "PLAY 2-10 \"jornal/abertura\" MIX 12 LINEAR RIGHT");
    CHECK(tr("PLAY 1-10", rule()) == "PLAY 2-10");
    CHECK(tr("PLAY 1", rule()) == "PLAY 2");
    CHECK(tr("LOADBG 1-10 \"x\" CUT 0 LINEAR RIGHT   AUTO", rule()) == "LOADBG 2-10 \"x\" CUT 0 LINEAR RIGHT   AUTO");
    CHECK(tr("LOAD 1-10 \"x\"", rule()) == "LOAD 2-10 \"x\"");
    CHECK(tr("STOP 1-10", rule()) == "STOP 2-10");
    CHECK(tr("PLAY 1-10 [HTML] \"http://a/b\"", rule()) == "PLAY 2-10 [HTML] \"http://a/b\"");

    // --- minusculas e espacos nas pontas ---
    CHECK(tr("play 1-10 \"x\"", rule()) == "play 2-10 \"x\"");
    CHECK(tr("  Stop   1-5  ", rule()) == "  Stop   2-5  ");
    CHECK(tr("\tPLAY\t1-10", rule()) == "\tPLAY\t2-10");

    // --- camada preservada, canal de varios digitos ---
    CHECK(tr("PLAY 1-999 \"x\"", rule()) == "PLAY 2-999 \"x\"");
    CHECK(tr("PLAY 12-10 \"x\"", rule(12, 3)) == "PLAY 3-10 \"x\"");
    CHECK(tr("PLAY 1-10 \"x\"", rule(1, 10)) == "PLAY 10-10 \"x\"");

    // --- outros canais nao sao replicados ---
    CHECK(tr("PLAY 2-10 \"x\"", rule()) == "<nao>");
    CHECK(tr("PLAY 11-10 \"x\"", rule()) == "<nao>"); // 11 != 1
    CHECK(tr("PLAY 10-1 \"x\"", rule()) == "<nao>");  // canal 10, nao 1

    // --- verbos fora da regra padrao ---
    CHECK(tr("PAUSE 1-10", rule()) == "<nao>");
    CHECK(tr("RESUME 1-10", rule()) == "<nao>");
    CHECK(tr("CALL 1-10 SEEK 100", rule()) == "<nao>");
    CHECK(tr("MIXER 1-10 VOLUME 0.5", rule()) == "<nao>");
    CHECK(tr("CLEAR 1", rule()) == "<nao>");
    CHECK(tr("CG 1-10 ADD 1 \"t\" 1", rule()) == "<nao>");
    CHECK(tr("CLS", rule()) == "<nao>");
    CHECK(tr("VERSION SERVER", rule()) == "<nao>");
    CHECK(tr("INFO 1", rule()) == "<nao>");
    CHECK(tr("THUMBNAIL RETRIEVE \"x\"", rule()) == "<nao>");
    CHECK(tr("DATA LIST", rule()) == "<nao>");
    CHECK(tr("SWAP 1-10 2-10", rule()) == "<nao>");

    // --- verbos opcionais quando habilitados ---
    CHECK(tr("PAUSE 1-10", rule(1, 2, DefaultVerbs | VerbPause)) == "PAUSE 2-10");
    CHECK(tr("RESUME 1-10", rule(1, 2, DefaultVerbs | VerbResume)) == "RESUME 2-10");
    CHECK(tr("CALL 1-10 SEEK 100", rule(1, 2, DefaultVerbs | VerbCall)) == "CALL 2-10 SEEK 100");
    CHECK(tr("MIXER 1-10 VOLUME 0.5", rule(1, 2, DefaultVerbs | VerbMixer)) == "MIXER 2-10 VOLUME 0.5");
    CHECK(tr("CLEAR 1", rule(1, 2, DefaultVerbs | VerbClear)) == "CLEAR 2");
    CHECK(tr("CG 1-10 ADD 1 \"t\" 1", rule(1, 2, DefaultVerbs | VerbCg)) == "CG 2-10 ADD 1 \"t\" 1");
    CHECK(tr("PLAY 1-10", rule(1, 2, VerbStop)) == "<nao>"); // PLAY desligado
    CHECK(tr("STOP 1-10", rule(1, 2, VerbStop)) == "STOP 2-10");
    CHECK(tr("PLAY 1-10", rule(1, 2, AllVerbs)) == "PLAY 2-10");

    // --- regra desligada / sem verbos ---
    CHECK(tr("PLAY 1-10", rule(1, 2, DefaultVerbs, false)) == "<nao>");
    CHECK(tr("PLAY 1-10", rule(1, 2, VerbNone)) == "<nao>");

    // --- somente o PRIMEIRO endereco e traduzido; route:// fica intacto ---
    CHECK(tr("PLAY 1-10 route://1-5", rule()) == "PLAY 2-10 route://1-5");
    CHECK(tr("LOADBG 1-10 route://1 FRAMES_DELAY 3", rule()) == "LOADBG 2-10 route://1 FRAMES_DELAY 3");

    // --- entradas malformadas (nunca devem estourar nem replicar) ---
    CHECK(tr("", rule()) == "<nao>");
    CHECK(tr("   ", rule()) == "<nao>");
    CHECK(tr("PLAY", rule()) == "<nao>");
    CHECK(tr("PLAY ", rule()) == "<nao>");
    CHECK(tr("PLAY x-10", rule()) == "<nao>");
    CHECK(tr("PLAY -10", rule()) == "<nao>");
    CHECK(tr("PLAY 1x-10", rule()) == "<nao>");
    CHECK(tr("PLAY1-10", rule()) == "<nao>");       // sem separador
    CHECK(tr("PLAYX 1-10", rule()) == "<nao>");     // verbo desconhecido
    CHECK(tr("PLA 1-10", rule()) == "<nao>");
    CHECK(tr("LOADBGX 1-10", rule()) == "<nao>");
    CHECK(tr("PLAY 1234567 \"x\"", rule()) == "<nao>"); // canal absurdo
    CHECK(tr("PLAY 99999999999999999999 \"x\"", rule()) == "<nao>");
    CHECK(tr("\xC3\xA9 1-10", rule()) == "<nao>");  // UTF-8 no lugar do verbo

    // --- aliasing: 'out' pode ser reutilizado entre chamadas ---
    {
        std::string buf;
        CHECK(translate("PLAY 1-10 \"a\"", rule(), buf) && buf == "PLAY 2-10 \"a\"");
        CHECK(translate("STOP 1-3", rule(), buf) && buf == "STOP 2-3");
    }

    // --- nome com acento e espaco no clipe passa intacto ---
    CHECK(tr("PLAY 1-10 \"not\xC3\xAD" "cias/aber tura\" MIX 25", rule()) == "PLAY 2-10 \"not\xC3\xAD" "cias/aber tura\" MIX 25");

    // --- REQ <id> <comando>: replica o comando interno sem o prefixo REQ ---
    CHECK(tr("REQ e1 LOADBG 1-10 \"clip\" LOOP", rule()) == "LOADBG 2-10 \"clip\" LOOP");
    CHECK(tr("req e2 PLAY 1-10", rule()) == "PLAY 2-10");
    CHECK(tr("REQ   e3   STOP 1-20", rule()) == "STOP 2-20");
    CHECK(tr("REQ e4 PLAY 3-10", rule()) == "<nao>");              // outro canal
    CHECK(tr("REQ e5 INFO", rule()) == "<nao>");                   // verbo nao habilitado
    CHECK(tr("REQ e6 PAUSE 1-10", rule()) == "<nao>");             // opcional desligado
    CHECK(tr("REQ e6 PAUSE 1-10", rule(1, 2, mirror::AllVerbs)) == "PAUSE 2-10");
    CHECK(tr("REQ", rule()) == "<nao>");
    CHECK(tr("REQ e7", rule()) == "<nao>");
    CHECK(tr("REQ e8 REQ e9 PLAY 1-10", rule()) == "<nao>");      // aninhado
    CHECK(tr("REQUEST 1-10", rule()) == "<nao>");

    if (failures)
    {
        std::printf("%d verificacao(oes) falharam\n", failures);
        return EXIT_FAILURE;
    }

    std::printf("todos os testes do tradutor passaram\n");
    return EXIT_SUCCESS;
}
