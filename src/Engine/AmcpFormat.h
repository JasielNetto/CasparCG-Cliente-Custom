#pragma once

// Formatação de comandos AMCP e leitura de respostas com identificador (REQ/RES).
// Alvo: CasparCG Server 2.2+ (validado para 2.4.3), que aceita "REQ <id> <comando>" e responde
// "RES <id> <código> <texto>". Sem Qt, para ser testado isoladamente.

#include "Item.h"
#include "Resource.h"

#include <optional>
#include <string>
#include <string_view>

namespace engine::amcp
{
    // Coloca o nome entre aspas, escapando barra invertida e aspas.
    std::string quote(std::string_view text);

    std::string address(const Endpoint& endpoint);  // "1-10"

    std::string loadBg(const Endpoint& endpoint, const Item& item);  // LOADBG 1-10 "clip" [LOOP]
    std::string play(const Endpoint& endpoint);                      // PLAY 1-10
    std::string stop(const Endpoint& endpoint);                      // STOP 1-10
    std::string pause(const Endpoint& endpoint);                     // PAUSE 1-10
    std::string resume(const Endpoint& endpoint);                    // RESUME 1-10
    std::string clear(const Endpoint& endpoint);                     // CLEAR 1-10

    std::string request(std::string_view id, std::string_view command);  // REQ <id> <comando>

    struct Response
    {
        std::string id;
        int code = 0;
        std::string text;  // restante da linha após o código, ex.: "LOADBG OK"

        bool ok() const { return code >= 200 && code < 300; }
    };

    // Lê "RES <id> <código> [texto]". Retorna vazio se a linha não for uma resposta com id.
    std::optional<Response> parseResponse(std::string_view line);
}
