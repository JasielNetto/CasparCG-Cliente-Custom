#pragma once

// Formatação de tempo para os painéis de recurso (decorrido / restante).

#include <cmath>
#include <cstdio>
#include <string>

namespace engine
{
    // "HH:MM:SS:FF" com fps conhecido (segundos reais + quadro dentro do segundo); "HH:MM:SS.d" (décimos) sem fps. Negativos viram zero.
    inline std::string formatTime(double seconds, double fps = 0.0)
    {
        constexpr double MaxSeconds = 99.0 * 3600.0 + 59.0 * 60.0 + 59.0;
        if (!(seconds > 0.0))  // também trata NaN
            seconds = 0.0;
        if (seconds > MaxSeconds)
            seconds = MaxSeconds;

        char buffer[96];
        if (fps > 0.0)
        {
            // Segundos de relógio real; os quadros são só a fração do segundo (não é timecode drop-frame).
            const long long wholeSeconds = static_cast<long long>(std::floor(seconds + 1e-9));
            const long long maxFrame = static_cast<long long>(std::ceil(fps)) - 1;
            long long frames = static_cast<long long>(std::floor((seconds - static_cast<double>(wholeSeconds)) * fps + 1e-6));
            if (frames > maxFrame)
                frames = maxFrame;
            std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld:%02lld",
                          wholeSeconds / 3600, (wholeSeconds / 60) % 60, wholeSeconds % 60, frames);
        }
        else
        {
            const long long tenths = static_cast<long long>(std::floor(seconds * 10.0 + 1e-6));
            const long long wholeSeconds = tenths / 10;
            std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld.%lld",
                          wholeSeconds / 3600, (wholeSeconds / 60) % 60, wholeSeconds % 60, tenths % 10);
        }
        return buffer;
    }

    inline double remainingTime(double elapsed, double total)
    {
        const double remaining = total - elapsed;
        return remaining > 0.0 ? remaining : 0.0;
    }
}
