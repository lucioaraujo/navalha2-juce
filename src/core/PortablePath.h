#pragma once

#include <string>
#include <string_view>

namespace navalha
{
[[nodiscard]] bool isSafePortableRelativePath(std::string_view path) noexcept;
[[nodiscard]] std::string normalizePortableRelativePath(std::string_view path);
}
