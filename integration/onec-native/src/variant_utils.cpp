#include "onec/variant_utils.hpp"

#include <codecvt>
#include <locale>
#include <stdexcept>

namespace onec {

static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> g_converter;

std::u16string utf8ToUtf16(const std::string& utf8) {
    if (utf8.empty()) {
        return std::u16string();
    }
    return g_converter.from_bytes(utf8);
}

std::string utf16ToUtf8(const WCHAR_T* str, uint32_t length) {
    if (!str || length == 0) {
        return std::string();
    }
    const char16_t* ptr = reinterpret_cast<const char16_t*>(str);
    return g_converter.to_bytes(ptr, ptr + length);
}

void variantSetUtf8String(tVariant& var, IMemoryManager* mem, const std::string& utf8) {
    variantSetStringUtf16(var, mem, utf8ToUtf16(utf8));
}

std::string variantGetUtf8String(const tVariant& var) {
    switch (var.vt) {
        case VTYPE_EMPTY:
        case VTYPE_NULL:
            return std::string();
        case VTYPE_PWSTR:
        case VTYPE_STRING:
            return utf16ToUtf8(var.value.pwstrVal, var.strLen);
        case VTYPE_PSTR:
            if (var.value.pstrVal && var.strLen > 0) {
                return std::string(var.value.pstrVal, var.strLen);
            }
            return std::string();
        default:
            throw std::runtime_error("Unsupported variant type for string conversion");
    }
}

}  // namespace onec

