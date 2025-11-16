#pragma once

#include "onec/addin_api.hpp"

#include <string>
#include <vector>
#include <stdexcept>

namespace onec {

inline void variantSetEmpty(tVariant& var) {
    var.vt = VTYPE_EMPTY;
    var.strLen = 0;
    var.value.ptrVal = nullptr;
}

inline void variantSetBool(tVariant& var, bool value) {
    var.vt = VTYPE_BOOL;
    var.value.boolVal = value;
}

inline void variantSetStringUtf16(tVariant& var, IMemoryManager* mem, const std::u16string& value) {
    if (!mem) {
        throw std::runtime_error("Memory manager is not set");
    }
    const size_t bytes = (value.size() + 1) * sizeof(WCHAR_T);
    auto* buffer = static_cast<WCHAR_T*>(mem->AllocMemory(static_cast<uint32_t>(bytes)));
    if (!buffer) {
        throw std::runtime_error("Unable to allocate memory for string");
    }
    std::copy(value.begin(), value.end(), buffer);
    buffer[value.size()] = 0;
    var.vt = VTYPE_PWSTR;
    var.value.pwstrVal = buffer;
    var.strLen = static_cast<uint32_t>(value.size());
}

void variantSetUtf8String(tVariant& var, IMemoryManager* mem, const std::string& utf8);

std::string variantGetUtf8String(const tVariant& var);

std::u16string utf8ToUtf16(const std::string& utf8);

std::string utf16ToUtf8(const WCHAR_T* str, uint32_t length);

}  // namespace onec

