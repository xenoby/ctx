#pragma once

#include "serializer.hpp"

class JsonSerializer : public ISerializer {
public:
    bool write(const ContextData& data, const std::string& path) override;
};
