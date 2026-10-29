#pragma once

// Detecta o fim de um clipe a partir do OSC "/foreground/file/time" (decorrido, total), em segundos.
//
// Dispara uma única vez quando o decorrido chega a menos de 'tolerance' do total. Volta a armar
// quando o decorrido recua (novo clipe ou loop) ou quando reset() é chamado.
// Clipes sem duração conhecida (total <= 0, ex.: streams) nunca disparam.

namespace engine
{
    class ClipEndDetector
    {
    public:
        explicit ClipEndDetector(double toleranceSeconds = 0.08) : tolerance(toleranceSeconds) {}

        // Retorna true no instante em que o fim é detectado.
        bool update(double elapsed, double total)
        {
            if (elapsed + 1e-6 < lastElapsed)
                fired = false;
            lastElapsed = elapsed;

            if (fired || total <= 0.0)
                return false;
            if (elapsed + tolerance >= total)
            {
                fired = true;
                return true;
            }
            return false;
        }

        void reset()
        {
            fired = false;
            lastElapsed = 0.0;
        }

    private:
        double tolerance;
        double lastElapsed = 0.0;
        bool fired = false;
    };
}
