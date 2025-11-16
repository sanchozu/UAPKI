#include "onec/base64.hpp"

#include <stdexcept>

namespace onec {
namespace {

const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline bool isBase64Char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}

}  // namespace

namespace base64 {

std::string encode(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t octet_a = data[i];
        uint32_t octet_b = (i + 1 < data.size()) ? data[i + 1] : 0;
        uint32_t octet_c = (i + 2 < data.size()) ? data[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        out.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < data.size()) ? kAlphabet[(triple >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < data.size()) ? kAlphabet[triple & 0x3F] : '=');
    }
    return out;
}

std::vector<uint8_t> decode(const std::string& data) {
    std::vector<uint8_t> out;
    if (data.empty()) {
        return out;
    }
    if (data.size() % 4 != 0) {
        throw std::runtime_error("Invalid base64 length");
    }
    auto decodeChar = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        if (c == '=') return -1;
        throw std::runtime_error("Invalid base64 character");
    };
    out.reserve((data.size() / 4) * 3);
    for (size_t i = 0; i < data.size(); i += 4) {
        int vals[4];
        for (int j = 0; j < 4; ++j) {
            char c = data[i + j];
            if (!isBase64Char(c)) {
                throw std::runtime_error("Invalid base64 input");
            }
            vals[j] = decodeChar(c);
        }
        uint32_t triple = 0;
        for (int j = 0; j < 4; ++j) {
            if (vals[j] >= 0) {
                triple |= (vals[j] & 0x3F) << ((3 - j) * 6);
            }
        }
        out.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));
        if (vals[2] >= 0) {
            out.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
        }
        if (vals[3] >= 0) {
            out.push_back(static_cast<uint8_t>(triple & 0xFF));
        }
        if (vals[2] < 0) {
            out.pop_back();
        }
        if (vals[3] < 0) {
            out.pop_back();
        }
    }
    return out;
}

}  // namespace base64
}  // namespace onec

