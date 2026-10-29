#pragma once

// Detecta o fim de um clipe a partir do OSC "/foreground/file/time" (decorrido, total), em segundos.
//
// Dispara uma única vez quando:
//   - o decorrido chega a menos de 'tolerance' do total; ou
//   - o decorrido fica parado por 'StallUpdates' atualizações seguidas a menos de 1 s do fim
//     (o OSC chega em lotes; o último valor pode parar alguns quadros antes do total).
// Volta a armar quando o decorrido recua (novo clipe ou loop) ou quando reset() é chamado.
// Clipes sem duração conhecida (total <= 0, ex.: streams) nunca disparam.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

namespace engine
{
    class ClipEndDetector
    {
    public:
        static constexpr int StallUpdates = 3;
        static constexpr double DefaultTolerance = 0.08;

        explicit ClipEndDetector(double toleranceSeconds = DefaultTolerance) : tolerance(toleranceSeconds) {}

        // Ajusta a tolerância a ~2,5 quadros da taxa do canal (nunca menos que a padrão).
        void setFps(double fps)
        {
            tolerance = fps > 0.0 ? std::max(DefaultTolerance, 2.5 / fps) : DefaultTolerance;
        }

        double toleranceSeconds() const { return tolerance; }

        // Retorna true no instante em que o fim é detectado.
        bool update(double elapsed, double total)
        {
            if (elapsed + 1e-6 < lastElapsed)
            {
                fired = false;
                stalled = 0;
            }
            stalled = (std::fabs(elapsed - lastElapsed) < 1e-6) ? stalled + 1 : 0;
            lastElapsed = elapsed;

            if (fired || total <= 0.0)
                return false;

            const bool nearEnd = elapsed + tolerance >= total;
            const bool stuckNearEnd = stalled >= StallUpdates && elapsed > 0.0 && elapsed + 1.0 >= total;
            if (nearEnd || stuckNearEnd)
            {
                fired = true;
                return true;
            }
            return false;
        }

        void reset()
        {
            fired = false;
            stalled = 0;
            lastElapsed = 0.0;
        }

    private:
        double tolerance;
        double lastElapsed = 0.0;
        int stalled = 0;
        bool fired = false;
    };

    // Confere se o nome do clipe informado pelo OSC ("file/name") corresponde à mídia do item.
    // Ignora maiúsculas, extensão e barra invertida; aceita caminho completo ou só o nome final.
    // Nome vazio (OSC ainda não informou) é aceito, para não travar o avanço.
    inline bool clipMatches(std::string_view oscName, std::string_view media)
    {
        auto normalize = [](std::string_view text) {
            std::string out;
            out.reserve(text.size());
            for (char c : text)
                out += c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const std::size_t slash = out.rfind('/');
            const std::size_t dot = out.rfind('.');
            if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
                out.erase(dot);
            return out;
        };

        if (oscName.empty())
            return true;

        const std::string a = normalize(oscName);
        const std::string b = normalize(media);
        if (a == b)
            return true;
        auto endsWithPath = [](const std::string& full, const std::string& tail) {
            return full.size() > tail.size() && full.compare(full.size() - tail.size(), tail.size(), tail) == 0
                && full[full.size() - tail.size() - 1] == '/';
        };
        return endsWithPath(a, b) || endsWithPath(b, a);
    }
}
