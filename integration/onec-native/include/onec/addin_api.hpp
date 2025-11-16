#pragma once

#include <cstdint>

typedef uint16_t WCHAR_T;

#ifndef ADDIN_API
#  if defined(_WIN32)
#    define ADDIN_API __declspec(dllexport)
#  else
#    define ADDIN_API __attribute__((visibility("default")))
#  endif
#endif

namespace onec {

enum AddInInfo {
    ADDIN_E_INFO = 1
};

enum VarType : uint16_t {
    VTYPE_EMPTY = 0,
    VTYPE_NULL = 1,
    VTYPE_I2 = 2,
    VTYPE_I4 = 3,
    VTYPE_R4 = 4,
    VTYPE_R8 = 5,
    VTYPE_DATE = 7,
    VTYPE_STRING = 8,
    VTYPE_BOOL = 11,
    VTYPE_VARIANT = 12,
    VTYPE_I1 = 16,
    VTYPE_UI1 = 17,
    VTYPE_UI2 = 18,
    VTYPE_UI4 = 19,
    VTYPE_I8 = 20,
    VTYPE_UI8 = 21,
    VTYPE_INT = 22,
    VTYPE_UINT = 23,
    VTYPE_FLOAT = 24,
    VTYPE_DOUBLE = 25,
    VTYPE_PSTR = 31,
    VTYPE_PWSTR = 32,
    VTYPE_BINARY = 33
};

struct tVariant {
    VarType vt;
    uint16_t reserved1;
    uint16_t reserved2;
    uint16_t reserved3;
    union {
        int32_t intVal;
        int64_t int64Val;
        double doubleVal;
        bool boolVal;
        char* pstrVal;
        WCHAR_T* pwstrVal;
        void* ptrVal;
    } value;
    uint32_t strLen;
    uint32_t reserved4;
};

class IAddInDefBase;

class IMemoryManager {
public:
    virtual ~IMemoryManager() = default;
    virtual void ADDIN_API AddRef() = 0;
    virtual void ADDIN_API Release() = 0;
    virtual void* ADDIN_API AllocMemory(uint32_t size) = 0;
    virtual void ADDIN_API FreeMemory(void* ptr) = 0;
};

class IComponentBase {
public:
    virtual ~IComponentBase() = default;
    virtual bool ADDIN_API Init(void* connection) = 0;
    virtual bool ADDIN_API setMemManager(void* mem) = 0;
    virtual long ADDIN_API GetInfo() = 0;
    virtual void ADDIN_API Done() = 0;
    virtual bool ADDIN_API RegisterExtensionAs(WCHAR_T** name) = 0;
    virtual long ADDIN_API GetNProps() = 0;
    virtual long ADDIN_API FindProp(const WCHAR_T* name) = 0;
    virtual const WCHAR_T* ADDIN_API GetPropName(long num, long alias) = 0;
    virtual bool ADDIN_API GetPropVal(long num, tVariant* val) = 0;
    virtual bool ADDIN_API SetPropVal(long num, const tVariant* val) = 0;
    virtual bool ADDIN_API IsPropReadable(long num) = 0;
    virtual bool ADDIN_API IsPropWritable(long num) = 0;
    virtual long ADDIN_API GetNMethods() = 0;
    virtual long ADDIN_API FindMethod(const WCHAR_T* name) = 0;
    virtual const WCHAR_T* ADDIN_API GetMethodName(long num, long alias) = 0;
    virtual long ADDIN_API GetNParams(long num) = 0;
    virtual bool ADDIN_API GetParamDefValue(long num, long paramNum, tVariant* defVal) = 0;
    virtual bool ADDIN_API HasRetVal(long num) = 0;
    virtual bool ADDIN_API CallAsProc(long num, tVariant* params, long sizeArray) = 0;
    virtual bool ADDIN_API CallAsFunc(long num, tVariant* retValue, tVariant* params, long sizeArray) = 0;
    virtual void ADDIN_API SetLocale(const WCHAR_T* loc) = 0;
};

} // namespace onec

