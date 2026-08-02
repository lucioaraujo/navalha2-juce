#include "core/PortablePath.h"

#include <stdexcept>

namespace navalha
{
bool isSafePortableRelativePath(std::string_view path) noexcept
{
    if (path.empty() || path.front() == '/' || path.front() == '\\'
        || path.find('\0') != std::string_view::npos
        || path.find(':') != std::string_view::npos)
        return false;

    std::size_t start = 0;
    while (start <= path.size())
    {
        const auto end = path.find_first_of("/\\", start);
        const auto length = (end == std::string_view::npos ? path.size() : end) - start;
        const auto part = path.substr(start, length);
        if (part.empty() || part == "." || part == "..")
            return false;
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    return true;
}

std::string normalizePortableRelativePath(std::string_view path)
{
    if (!isSafePortableRelativePath(path))
        throw std::invalid_argument("Unsafe portable project path");

    std::string normalized(path);
    for (auto& character : normalized)
        if (character == '\\')
            character = '/';
    return normalized;
}
}
