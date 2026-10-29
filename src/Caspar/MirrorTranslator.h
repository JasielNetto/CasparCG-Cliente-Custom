#pragma once

// Mirror (redundancia cruzada): decide se uma mensagem AMCP deve ser replicada em outro
// servidor/canal e, em caso positivo, produz a mensagem traduzida.
//
// Este arquivo NAO depende do Qt, de proposito: assim a logica de traducao pode ser compilada e
// testada isoladamente (tools/tests/mirror_translator_test.cpp).
//
// Regras:
//  - So mensagens cujo verbo esteja habilitado na regra sao replicadas (padrao: PLAY, LOAD, LOADBG, STOP).
//  - So mensagens enderecadas ao canal de ORIGEM sao replicadas; o numero do canal e trocado pelo canal de DESTINO
//    e a camada (layer) e mantida:  "PLAY 1-10 "clip" MIX 12"  ->  "PLAY 2-10 "clip" MIX 12".
//  - Apenas o primeiro endereco "canal[-camada]" e traduzido. Enderecos dentro de argumentos
//    (por exemplo route://1-10) nao sao alterados.
//  - Verbos sao comparados sem diferenciar maiusculas de minusculas.
//
// Custo: uma unica passada sobre o inicio da mensagem, sem alocacao quando a mensagem NAO e replicada.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mirror
{
    enum Verb : std::uint32_t
    {
        VerbNone   = 0,
        VerbPlay   = 1u << 0,  // PLAY
        VerbLoad   = 1u << 1,  // LOAD e LOADBG
        VerbStop   = 1u << 2,  // STOP
        VerbPause  = 1u << 3,  // PAUSE
        VerbResume = 1u << 4,  // RESUME
        VerbClear  = 1u << 5,  // CLEAR
        VerbMixer  = 1u << 6,  // MIXER
        VerbCall   = 1u << 7,  // CALL (por exemplo CALL 1-10 SEEK 100)
        VerbCg     = 1u << 8   // CG (templates)
    };

    constexpr std::uint32_t DefaultVerbs = VerbPlay | VerbLoad | VerbStop;
    constexpr std::uint32_t AllVerbs = VerbPlay | VerbLoad | VerbStop | VerbPause | VerbResume | VerbClear | VerbMixer | VerbCall | VerbCg;

    struct Rule
    {
        bool enabled = false;
        int sourceChannel = 1;
        int targetChannel = 2;
        std::uint32_t verbs = DefaultVerbs;
    };

    namespace detail
    {
        inline bool isSpace(char c) { return c == ' ' || c == '\t'; }
        inline bool isAlpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
        inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
        inline char upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

        // Compara 'word' (sem diferenciar maiusculas) com um literal MAIUSCULO.
        inline bool equalsNoCase(std::string_view word, const char* upperLiteral)
        {
            std::size_t i = 0;
            for (; i < word.size(); ++i)
            {
                if (upperLiteral[i] == '\0' || upper(word[i]) != upperLiteral[i])
                    return false;
            }
            return upperLiteral[i] == '\0';
        }

        inline std::uint32_t classify(std::string_view verb)
        {
            if (verb.size() < 2 || verb.size() > 6)
                return VerbNone;

            struct Entry { const char* name; std::uint32_t bit; };
            static const Entry table[] = {
                { "PLAY", VerbPlay },   { "LOAD", VerbLoad },     { "LOADBG", VerbLoad },
                { "STOP", VerbStop },   { "PAUSE", VerbPause },   { "RESUME", VerbResume },
                { "CLEAR", VerbClear }, { "MIXER", VerbMixer },   { "CALL", VerbCall },
                { "CG", VerbCg }
            };

            for (const Entry& e : table)
            {
                if (equalsNoCase(verb, e.name))
                    return e.bit;
            }

            return VerbNone;
        }
    }

    // Retorna true e preenche 'out' com a mensagem traduzida quando 'message' deve ser replicada.
    // Retorna false (e nao toca em 'out') caso contrario.
    inline bool translate(std::string_view message, const Rule& rule, std::string& out)
    {
        using namespace detail;

        if (!rule.enabled || rule.verbs == VerbNone)
            return false;

        const std::size_t n = message.size();
        std::size_t i = 0;

        while (i < n && isSpace(message[i]))
            ++i;

        // Verbo.
        const std::size_t verbStart = i;
        while (i < n && isAlpha(message[i]))
            ++i;

        const std::string_view verb = message.substr(verbStart, i - verbStart);

        // "REQ <id> <comando>": o comando interno e replicado SEM o prefixo REQ, para que a resposta
        // do destino nao seja confundida com a resposta da origem (o id pertence a origem).
        if (equalsNoCase(verb, "REQ"))
        {
            if (i >= n || !isSpace(message[i]))
                return false;
            while (i < n && isSpace(message[i]))
                ++i;
            const std::size_t idStart = i;
            while (i < n && !isSpace(message[i]))
                ++i;
            if (i == idStart)
                return false;
            while (i < n && isSpace(message[i]))
                ++i;
            const std::string_view inner = message.substr(i);
            if (inner.size() >= 3 && equalsNoCase(inner.substr(0, 3), "REQ"))
                return false;  // REQ aninhado nao e valido
            return translate(inner, rule, out);
        }

        const std::uint32_t bit = classify(verb);
        if (bit == VerbNone || (rule.verbs & bit) == 0)
            return false;

        // Separador entre o verbo e o endereco.
        if (i >= n || !isSpace(message[i]))
            return false;

        while (i < n && isSpace(message[i]))
            ++i;

        // Canal (somente digitos).
        const std::size_t channelStart = i;
        long channel = 0;
        std::size_t digits = 0;
        while (i < n && isDigit(message[i]))
        {
            channel = channel * 10 + (message[i] - '0');
            if (++digits > 6)
                return false;
            ++i;
        }

        if (digits == 0)
            return false;

        const std::size_t channelEnd = i;

        // Depois do canal so pode vir: fim, espaco ou "-camada".
        if (i < n && !isSpace(message[i]) && message[i] != '-')
            return false;

        if (channel != rule.sourceChannel)
            return false;

        out.assign(message.data(), channelStart);
        out += std::to_string(rule.targetChannel);
        out.append(message.data() + channelEnd, n - channelEnd);

        return true;
    }
}
