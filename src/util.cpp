#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace minisql {

std::string trim(const std::string& input) {
    std::size_t begin = 0;
    while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) {
        ++begin;
    }
    std::size_t end = input.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    return input.substr(begin, end - begin);
}

std::string to_lower(std::string input) {
    for (char& ch : input) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return input;
}

bool valid_identifier(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    if (name[0] < 'a' || name[0] > 'z') {
        return false;
    }
    for (std::size_t i = 1; i < name.size(); ++i) {
        char ch = name[i];
        bool lower = ch >= 'a' && ch <= 'z';
        bool digit = ch >= '0' && ch <= '9';
        if (!lower && !digit) {
            return false;
        }
    }
    return true;
}

Array<std::string> split_csv(const std::string& input) {
    Array<std::string> parts;
    std::string current;
    bool quoted = false;
    for (std::size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];
        if (ch == '"') {
            quoted = !quoted;
            current.push_back(ch);
        } else if (ch == ',' && !quoted) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(trim(current));
    return parts;
}

std::string escape_field(const std::string& value) {
    std::string out;
    for (char ch : value) {
        if (ch == '\\' || ch == '|' || ch == '\n') {
            out.push_back('\\');
        }
        if (ch == '\n') {
            out.push_back('n');
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

std::string unescape_field(const std::string& value) {
    std::string out;
    bool escaping = false;
    for (char ch : value) {
        if (escaping) {
            out.push_back(ch == 'n' ? '\n' : ch);
            escaping = false;
        } else if (ch == '\\') {
            escaping = true;
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

} // namespace minisql
