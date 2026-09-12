#pragma once

#include <string>

namespace texnx::textures {

struct TexturePack {
    std::string name;
    std::string rootPath;
    std::string commonPath;
    std::string iconPath;
    std::string description;
};

} // namespace texnx::textures
