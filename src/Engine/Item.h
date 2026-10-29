#pragma once

#include "Resource.h"

#include <string>

namespace engine
{
    // Item executável do Rundown, já reduzido ao que o núcleo precisa para disparar.
    // Propriedades específicas (transição, seek, InPoint/OutPoint etc.) entram aqui quando
    // o adaptador AMCP passar a consumi-las.
    struct Item
    {
        std::string id;
        LogicalResource resource = LogicalResource::Vt;
        std::string media;
        bool loop = false;
    };
}
