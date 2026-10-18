#pragma once

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

// Minimal JSON helpers for the control protocol. Only what the protocol emits
// is supported: flat string values, no general parser.
namespace json {

inline void appendString(std::string& out, std::string_view s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

inline std::string quote(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    appendString(out, s);
    return out;
}

// Reads the string value of the first top-level occurrence of "key":"...".
// Only \uXXXX escapes below 0x80 are decoded, which is all appendString emits.
inline std::optional<std::string> findString(std::string_view doc, std::string_view key) {
    std::string needle = quote(key) + ":\"";
    auto pos = doc.find(needle);
    if (pos == std::string_view::npos) return std::nullopt;
    pos += needle.size();

    std::string out;
    while (pos < doc.size()) {
        char c = doc[pos++];
        if (c == '"') return out;
        if (c != '\\' || pos >= doc.size()) {
            out.push_back(c);
            continue;
        }
        char e = doc[pos++];
        switch (e) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
                if (pos + 4 <= doc.size()) {
                    out.push_back(static_cast<char>(std::stoi(std::string(doc.substr(pos, 4)), nullptr, 16)));
                    pos += 4;
                }
                break;
            default: out.push_back(e);
        }
    }
    return std::nullopt;
}

} // namespace json
