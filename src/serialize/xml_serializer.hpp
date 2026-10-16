#pragma once

#include "serializer.hpp"

class XmlSerializer : public ISerializer {
public:
    bool write(const ContextData& data, const std::string& path) override;
};
