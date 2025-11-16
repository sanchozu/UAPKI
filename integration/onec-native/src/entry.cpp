#include "onec/addin_api.hpp"
#include "onec/variant_utils.hpp"
#include "uapki_component.hpp"

#include <string>

namespace {
const WCHAR_T kClassName[] = u"UapkiNative";

inline uint32_t wcharLength(const WCHAR_T* value) {
    if (!value) {
        return 0;
    }
    uint32_t len = 0;
    while (value[len] != 0) {
        ++len;
    }
    return len;
}
}

extern "C" {

ADDIN_API long GetClassObject(const WCHAR_T* name, onec::IComponentBase** pInterface) {
    if (!pInterface || !name) {
        return 0;
    }
    const auto len = wcharLength(name);
    const std::string requested = onec::utf16ToUtf8(name, len);
    const std::string expected = onec::utf16ToUtf8(kClassName, wcharLength(kClassName));
    if (requested != expected) {
        return 0;
    }
    *pInterface = new onec::UapkiComponent();
    return 1;
}

ADDIN_API long DestroyObject(onec::IComponentBase* pInterface) {
    delete pInterface;
    return 1;
}

ADDIN_API const WCHAR_T* GetClassNames() {
    return kClassName;
}

}

