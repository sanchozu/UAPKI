#pragma once

#include <map>
#include <string>
#include <vector>

#include "onec/addin_api.hpp"
#include "onec/uapki_json_client.hpp"

namespace onec {

class UapkiComponent : public IComponentBase {
public:
    UapkiComponent();
    ~UapkiComponent() override;

    bool ADDIN_API Init(void* connection) override;
    bool ADDIN_API setMemManager(void* mem) override;
    long ADDIN_API GetInfo() override;
    void ADDIN_API Done() override;
    bool ADDIN_API RegisterExtensionAs(WCHAR_T** name) override;
    long ADDIN_API GetNProps() override;
    long ADDIN_API FindProp(const WCHAR_T* name) override;
    const WCHAR_T* ADDIN_API GetPropName(long num, long alias) override;
    bool ADDIN_API GetPropVal(long num, tVariant* val) override;
    bool ADDIN_API SetPropVal(long num, const tVariant* val) override;
    bool ADDIN_API IsPropReadable(long num) override;
    bool ADDIN_API IsPropWritable(long num) override;
    long ADDIN_API GetNMethods() override;
    long ADDIN_API FindMethod(const WCHAR_T* name) override;
    const WCHAR_T* ADDIN_API GetMethodName(long num, long alias) override;
    long ADDIN_API GetNParams(long num) override;
    bool ADDIN_API GetParamDefValue(long num, long paramNum, tVariant* defVal) override;
    bool ADDIN_API HasRetVal(long num) override;
    bool ADDIN_API CallAsProc(long num, tVariant* params, long sizeArray) override;
    bool ADDIN_API CallAsFunc(long num, tVariant* retValue, tVariant* params, long sizeArray) override;
    void ADDIN_API SetLocale(const WCHAR_T* loc) override;

private:
    enum class MethodId : long {
        Initialize = 0,
        SignFile,
        VerifyFileSignature,
        GetCertificateInfo,
        SignData,
        VerifyDataSignature,
        Count
    };

    enum class PropertyId : long {
        LastError = 0,
        Count
    };

    bool handleInitialize(tVariant* retValue, tVariant* params, long count);
    bool handleSignFile(tVariant* retValue, tVariant* params, long count);
    bool handleVerifyFile(tVariant* retValue, tVariant* params, long count);
    bool handleCertInfo(tVariant* retValue, tVariant* params, long count);
    bool handleSignData(tVariant* retValue, tVariant* params, long count);
    bool handleVerifyData(tVariant* retValue, tVariant* params, long count);

    std::string readFile(const std::string& path);
    void writeFile(const std::string& path, const std::vector<uint8_t>& data);
    nlohmann::json buildSignSummary(const nlohmann::json& signResult, const nlohmann::json& verifyResult);
    nlohmann::json buildVerifySummary(const nlohmann::json& verifyResult);
    nlohmann::json buildCertSummary(const nlohmann::json& certInfo);

    bool ensureParamCount(long expected, long actual, tVariant* params);
    std::string variantToUtf8String(const tVariant& var) const;
    void setResultString(tVariant* retValue, const std::string& utf8) const;
    void rememberError(const std::string& message);

    IMemoryManager* memoryManager_;
    void* connection_;
    std::string locale_;
    std::string lastError_;
    UapkiJsonClient uapki_;
};

}  // namespace onec

