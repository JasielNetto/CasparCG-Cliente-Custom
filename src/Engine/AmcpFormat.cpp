#include "AmcpFormat.h"

#include <cctype>

namespace engine::amcp
{
    std::string quote(std::string_view text)
    {
        std::string out;
        out.reserve(text.size() + 2);
        out += '"';
        for (char c : text)
        {
            if (c == '\\' || c == '"')
                out += '\\';
            out += c;
        }
        out += '"';
        return out;
    }

    std::string address(const Endpoint& endpoint)
    {
        return std::to_string(endpoint.channel) + "-" + std::to_string(endpoint.layer);
    }

    std::string loadBg(const Endpoint& endpoint, const Item& item)
    {
        std::string command = "LOADBG " + address(endpoint) + " " + quote(item.media);
        if (item.loop)
            command += " LOOP";
        return command;
    }

    std::string play(const Endpoint& endpoint)
    {
        return "PLAY " + address(endpoint);
    }

    std::string stop(const Endpoint& endpoint)
    {
        return "STOP " + address(endpoint);
    }

    std::string request(std::string_view id, std::string_view command)
    {
        std::string out = "REQ ";
        out.append(id);
        out += ' ';
        out.append(command);
        return out;
    }

    namespace
    {
        bool isSpace(char c) { return c == ' ' || c == '\t'; }

        std::string_view nextToken(std::string_view& rest)
        {
            std::size_t i = 0;
            while (i < rest.size() && isSpace(rest[i]))
                ++i;
            const std::size_t start = i;
            while (i < rest.size() && !isSpace(rest[i]))
                ++i;
            const std::string_view token = rest.substr(start, i - start);
            rest.remove_prefix(i);
            return token;
        }
    }

    std::optional<Response> parseResponse(std::string_view line)
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.remove_suffix(1);

        std::string_view rest = line;
        if (nextToken(rest) != "RES")
            return std::nullopt;

        const std::string_view id = nextToken(rest);
        const std::string_view codeText = nextToken(rest);
        if (id.empty() || codeText.size() != 3)
            return std::nullopt;

        int code = 0;
        for (char c : codeText)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return std::nullopt;
            code = code * 10 + (c - '0');
        }

        while (!rest.empty() && isSpace(rest.front()))
            rest.remove_prefix(1);

        return Response{ std::string(id), code, std::string(rest) };
    }
}
