#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace navalha
{
class Json
{
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json, std::less<>>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Json() noexcept;
    Json(std::nullptr_t) noexcept;
    Json(bool value) noexcept;
    Json(double value) noexcept;
    Json(int value) noexcept;
    Json(std::string value);
    Json(const char* value);
    Json(Array value);
    Json(Object value);

    [[nodiscard]] bool isObject() const noexcept;
    [[nodiscard]] bool isArray() const noexcept;
    [[nodiscard]] const Object& object() const;
    [[nodiscard]] const Array& array() const;
    [[nodiscard]] std::string_view string(std::string_view fallback = {}) const noexcept;
    [[nodiscard]] double number(double fallback = 0.0) const noexcept;
    [[nodiscard]] bool boolean(bool fallback = false) const noexcept;
    [[nodiscard]] const Json* find(std::string_view key) const noexcept;
    [[nodiscard]] const Value& value() const noexcept;

private:
    Value data;
};

[[nodiscard]] Json parseJson(std::string_view text,
                             std::size_t maximumBytes = 16 * 1024 * 1024,
                             std::size_t maximumDepth = 64);
[[nodiscard]] std::string serializeJson(const Json& value);
}
