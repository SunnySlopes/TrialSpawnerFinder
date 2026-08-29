#include "pool/JsonParser.hpp"

#include <cctype>
#include <stdexcept>
#include <string_view>

namespace trialfinder {

// ---------------- 对象访问 ----------------

bool Json::has(const std::string& key) const {
    if (type != Type::OBJECT) {
        return false;
    }
    for (const auto& member : object) {
        if (member.first == key) {
            return true;
        }
    }
    return false;
}

const Json* Json::get(const std::string& key) const {
    if (type != Type::OBJECT) {
        return nullptr;
    }
    for (const auto& member : object) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

std::string Json::getString(const std::string& key) const {
    const Json* v = get(key);
    return v != nullptr && v->type == Type::STRING ? v->str : "";
}

int Json::getInt(const std::string& key) const {
    const Json* v = get(key);
    return v != nullptr && v->type == Type::NUMBER ? static_cast<int>(v->number) : 0;
}

double Json::getDouble(const std::string& key) const {
    const Json* v = get(key);
    return v != nullptr && v->type == Type::NUMBER ? v->number : 0.0;
}

const Json* Json::getArray(const std::string& key) const {
    const Json* v = get(key);
    return v != nullptr && v->type == Type::ARRAY ? v : nullptr;
}

// ---------------- 解析器 ----------------

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text), pos_(0) {}

    Json parseValue() {
        skipWhitespace();
        if (atEnd()) {
            throw std::runtime_error("JSON 意外结束");
        }
        char c = peek();
        switch (c) {
            case '{': return parseObject();
            case '[': return parseArray();
            case '"': {
                Json j;
                j.type = Json::Type::STRING;
                j.str = parseString();
                return j;
            }
            case 't':
                expect("true");
                { Json j; j.type = Json::Type::BOOL; j.boolean = true; return j; }
            case 'f':
                expect("false");
                { Json j; j.type = Json::Type::BOOL; j.boolean = false; return j; }
            case 'n':
                expect("null");
                return Json();
            default:
                return parseNumber();
        }
    }

    static Json parse(const std::string& text) {
        Parser parser(text);
        Json value = parser.parseValue();
        parser.skipWhitespace();
        if (parser.pos_ < text.size()) {
            throw std::runtime_error("JSON 尾随内容 @" + std::to_string(parser.pos_));
        }
        return value;
    }

private:
    const std::string& text_;
    std::size_t pos_;

    bool atEnd() const { return pos_ >= text_.size(); }

    char peek() const { return atEnd() ? '\0' : text_[pos_]; }

    void skipWhitespace() {
        while (!atEnd()) {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                pos_++;
            } else {
                break;
            }
        }
    }

    void expect(const char* literal) {
        std::string_view lit(literal);
        if (text_.compare(pos_, lit.size(), lit) != 0) {
            throw std::runtime_error(std::string("期望 ") + literal + " @" + std::to_string(pos_));
        }
        pos_ += lit.size();
    }

    Json parseObject() {
        pos_++;  // '{'
        Json obj;
        obj.type = Json::Type::OBJECT;
        skipWhitespace();
        if (peek() == '}') {
            pos_++;
            return obj;
        }
        while (true) {
            skipWhitespace();
            if (peek() != '"') {
                throw std::runtime_error("期望字符串键 @" + std::to_string(pos_));
            }
            std::string key = parseString();
            skipWhitespace();
            expect(":");
            Json value = parseValue();
            obj.object.emplace_back(std::move(key), std::move(value));
            skipWhitespace();
            if (atEnd()) {
                throw std::runtime_error("对象未结束");
            }
            char c = text_[pos_++];
            if (c == '}') {
                break;
            }
            if (c != ',') {
                throw std::runtime_error("期望 , 或 } @" + std::to_string(pos_ - 1));
            }
        }
        return obj;
    }

    Json parseArray() {
        pos_++;  // '['
        Json arr;
        arr.type = Json::Type::ARRAY;
        skipWhitespace();
        if (peek() == ']') {
            pos_++;
            return arr;
        }
        while (true) {
            arr.array.push_back(parseValue());
            skipWhitespace();
            if (atEnd()) {
                throw std::runtime_error("数组未结束");
            }
            char c = text_[pos_++];
            if (c == ']') {
                break;
            }
            if (c != ',') {
                throw std::runtime_error("期望 , 或 ] @" + std::to_string(pos_ - 1));
            }
        }
        return arr;
    }

    std::string parseString() {
        pos_++;  // '"'
        std::string sb;
        while (true) {
            if (atEnd()) {
                throw std::runtime_error("字符串未结束");
            }
            char c = text_[pos_++];
            if (c == '"') {
                break;
            }
            if (c == '\\') {
                if (atEnd()) {
                    throw std::runtime_error("转义未结束");
                }
                char e = text_[pos_++];
                switch (e) {
                    case '"': sb += '"'; break;
                    case '\\': sb += '\\'; break;
                    case '/': sb += '/'; break;
                    case 'b': sb += '\b'; break;
                    case 'f': sb += '\f'; break;
                    case 'n': sb += '\n'; break;
                    case 'r': sb += '\r'; break;
                    case 't': sb += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) {
                            throw std::runtime_error("\\u 转义不完整");
                        }
                        int hex = 0;
                        for (int i = 0; i < 4; i++) {
                            char h = text_[pos_++];
                            hex <<= 4;
                            if (h >= '0' && h <= '9') hex |= h - '0';
                            else if (h >= 'a' && h <= 'f') hex |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') hex |= h - 'A' + 10;
                            else throw std::runtime_error("无效 \\u 十六进制");
                        }
                        sb += static_cast<char>(hex);
                        break;
                    }
                    default: throw std::runtime_error(std::string("未知转义 \\") + e);
                }
            } else {
                sb += c;
            }
        }
        return sb;
    }

    Json parseNumber() {
        std::size_t start = pos_;
        if (peek() == '-') {
            pos_++;
        }
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            pos_++;
        }
        bool floating = false;
        if (peek() == '.') {
            floating = true;
            pos_++;
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                pos_++;
            }
        }
        if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
            floating = true;
            pos_++;
            if (!atEnd() && (peek() == '+' || peek() == '-')) {
                pos_++;
            }
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                pos_++;
            }
        }
        std::string num = text_.substr(start, pos_ - start);
        if (num.empty() || num == "-") {
            throw std::runtime_error("无效数字 @" + std::to_string(start));
        }
        (void)floating;
        Json j;
        j.type = Json::Type::NUMBER;
        j.number = std::stod(num);
        return j;
    }
};

} // namespace

Json Json::parse(const std::string& text) {
    return Parser::parse(text);
}

Json Json::parseBytes(const uint8_t* data, std::size_t len) {
    return Parser::parse(std::string(reinterpret_cast<const char*>(data), len));
}

} // namespace trialfinder
