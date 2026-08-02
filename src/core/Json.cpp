#include "core/Json.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace navalha
{
Json::Json() noexcept : data(nullptr) {}
Json::Json(std::nullptr_t) noexcept : data(nullptr) {}
Json::Json(bool value) noexcept : data(value) {}
Json::Json(double value) noexcept : data(value) {}
Json::Json(int value) noexcept : data(static_cast<double>(value)) {}
Json::Json(std::string value) : data(std::move(value)) {}
Json::Json(const char* value) : data(std::string(value)) {}
Json::Json(Array value) : data(std::move(value)) {}
Json::Json(Object value) : data(std::move(value)) {}
bool Json::isObject() const noexcept { return std::holds_alternative<Object>(data); }
bool Json::isArray() const noexcept { return std::holds_alternative<Array>(data); }
const Json::Object& Json::object() const { return std::get<Object>(data); }
const Json::Array& Json::array() const { return std::get<Array>(data); }
const Json::Value& Json::value() const noexcept { return data; }

std::string_view Json::string(std::string_view fallback) const noexcept
{
    if (const auto* result = std::get_if<std::string>(&data))
        return *result;
    return fallback;
}

double Json::number(double fallback) const noexcept
{
    if (const auto* result = std::get_if<double>(&data))
        return *result;
    return fallback;
}

bool Json::boolean(bool fallback) const noexcept
{
    if (const auto* result = std::get_if<bool>(&data))
        return *result;
    return fallback;
}

const Json* Json::find(std::string_view key) const noexcept
{
    const auto* values = std::get_if<Object>(&data);
    if (values == nullptr)
        return nullptr;
    const auto found = values->find(key);
    return found == values->end() ? nullptr : &found->second;
}

namespace
{
class Parser
{
public:
    Parser(std::string_view input, std::size_t depthLimit)
        : text(input), maximumDepth(depthLimit) {}

    Json parse()
    {
        auto result = parseValue(0);
        whitespace();
        if (position != text.size())
            fail("Trailing JSON data");
        return result;
    }

private:
    Json parseValue(std::size_t depth)
    {
        if (depth > maximumDepth)
            fail("JSON nesting limit exceeded");
        whitespace();
        if (position >= text.size())
            fail("Unexpected end of JSON");
        const auto character = text[position];
        if (character == '{') return parseObject(depth + 1);
        if (character == '[') return parseArray(depth + 1);
        if (character == '"') return Json(parseString());
        if (character == 't') return literal("true", Json(true));
        if (character == 'f') return literal("false", Json(false));
        if (character == 'n') return literal("null", Json(nullptr));
        return parseNumber();
    }

    Json parseObject(std::size_t depth)
    {
        ++position;
        Json::Object values;
        whitespace();
        if (consume('}')) return Json(std::move(values));
        for (;;)
        {
            whitespace();
            if (position >= text.size() || text[position] != '"')
                fail("JSON object key must be a string");
            auto key = parseString();
            whitespace();
            if (!consume(':')) fail("Missing JSON object colon");
            values.insert_or_assign(std::move(key), parseValue(depth));
            whitespace();
            if (consume('}')) break;
            if (!consume(',')) fail("Missing JSON object comma");
        }
        return Json(std::move(values));
    }

    Json parseArray(std::size_t depth)
    {
        ++position;
        Json::Array values;
        whitespace();
        if (consume(']')) return Json(std::move(values));
        for (;;)
        {
            values.push_back(parseValue(depth));
            whitespace();
            if (consume(']')) break;
            if (!consume(',')) fail("Missing JSON array comma");
        }
        return Json(std::move(values));
    }

    std::string parseString()
    {
        ++position;
        std::string output;
        while (position < text.size())
        {
            const auto character = text[position++];
            if (character == '"')
                return output;
            if (static_cast<unsigned char>(character) < 0x20U)
                fail("Control character in JSON string");
            if (character != '\\')
            {
                output.push_back(character);
                continue;
            }
            if (position >= text.size()) fail("Truncated JSON escape");
            const auto escaped = text[position++];
            switch (escaped)
            {
                case '"': case '\\': case '/': output.push_back(escaped); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u': appendUnicode(output); break;
                default: fail("Invalid JSON escape");
            }
        }
        fail("Unterminated JSON string");
    }

    void appendUnicode(std::string& output)
    {
        if (position + 4 > text.size()) fail("Truncated Unicode escape");
        unsigned int code = 0;
        const auto conversion = std::from_chars(
            text.data() + position, text.data() + position + 4, code, 16);
        if (conversion.ec != std::errc {}) fail("Invalid Unicode escape");
        position += 4;
        if (code <= 0x7fU) output.push_back(static_cast<char>(code));
        else if (code <= 0x7ffU)
        {
            output.push_back(static_cast<char>(0xc0U | (code >> 6U)));
            output.push_back(static_cast<char>(0x80U | (code & 0x3fU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xe0U | (code >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (code & 0x3fU)));
        }
    }

    Json parseNumber()
    {
        const auto start = position;
        while (position < text.size()
               && std::string_view("-+0123456789.eE").find(text[position])
                    != std::string_view::npos)
            ++position;
        double value = 0.0;
        const auto conversion = std::from_chars(
            text.data() + start, text.data() + position, value);
        if (conversion.ec != std::errc {} || conversion.ptr != text.data() + position
            || !std::isfinite(value))
            fail("Invalid JSON number");
        return Json(value);
    }

    Json literal(std::string_view expected, Json value)
    {
        if (text.substr(position, expected.size()) != expected)
            fail("Invalid JSON literal");
        position += expected.size();
        return value;
    }

    bool consume(char character)
    {
        if (position < text.size() && text[position] == character)
        {
            ++position;
            return true;
        }
        return false;
    }

    void whitespace()
    {
        while (position < text.size()
               && (text[position] == ' ' || text[position] == '\n'
                   || text[position] == '\r' || text[position] == '\t'))
            ++position;
    }

    [[noreturn]] void fail(const char* message) const
    {
        throw std::invalid_argument(
            std::string(message) + " at byte " + std::to_string(position));
    }

    std::string_view text;
    std::size_t maximumDepth;
    std::size_t position = 0;
};

void serialize(const Json& json, std::string& output)
{
    if (std::holds_alternative<std::nullptr_t>(json.value())) output += "null";
    else if (const auto* value = std::get_if<bool>(&json.value()))
        output += *value ? "true" : "false";
    else if (const auto* value = std::get_if<double>(&json.value()))
    {
        char number[32] {};
        const auto result = std::to_chars(number, number + sizeof(number), *value);
        output.append(number, result.ptr);
    }
    else if (const auto* value = std::get_if<std::string>(&json.value()))
    {
        output.push_back('"');
        for (const auto character : *value)
        {
            switch (character)
            {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(character) < 0x20U)
                    {
                        char escaped[7] {};
                        std::snprintf(escaped, sizeof(escaped), "\\u%04X",
                                      static_cast<unsigned char>(character));
                        output += escaped;
                    }
                    else output.push_back(character);
            }
        }
        output.push_back('"');
    }
    else if (const auto* values = std::get_if<Json::Array>(&json.value()))
    {
        output.push_back('[');
        for (std::size_t index = 0; index < values->size(); ++index)
        {
            if (index != 0) output.push_back(',');
            serialize((*values)[index], output);
        }
        output.push_back(']');
    }
    else
    {
        output.push_back('{');
        bool first = true;
        for (const auto& [key, value] : std::get<Json::Object>(json.value()))
        {
            if (!first) output.push_back(',');
            first = false;
            serialize(Json(key), output);
            output.push_back(':');
            serialize(value, output);
        }
        output.push_back('}');
    }
}
}

Json parseJson(std::string_view text, std::size_t maximumBytes, std::size_t maximumDepth)
{
    if (text.size() > maximumBytes)
        throw std::length_error("JSON exceeds size limit");
    return Parser(text, maximumDepth).parse();
}

std::string serializeJson(const Json& value)
{
    std::string output;
    serialize(value, output);
    return output;
}
}
