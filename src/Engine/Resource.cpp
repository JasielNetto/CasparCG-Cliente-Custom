#include "Resource.h"

namespace engine
{
    const char* toString(LogicalResource resource)
    {
        switch (resource)
        {
            case LogicalResource::Vt: return "VT";
            case LogicalResource::Selo1: return "Selo 1";
            case LogicalResource::Selo2: return "Selo 2";
        }
        return "?";
    }

    void ResourceMap::set(LogicalResource resource, const Endpoint& endpoint)
    {
        endpoints[resource] = endpoint;
    }

    void ResourceMap::remove(LogicalResource resource)
    {
        endpoints.erase(resource);
    }

    std::optional<Endpoint> ResourceMap::find(LogicalResource resource) const
    {
        const auto it = endpoints.find(resource);
        if (it == endpoints.end())
            return std::nullopt;
        return it->second;
    }
}
