#include "uapki_component.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cwctype>
#include <sstream>

#include "onec/base64.hpp"
#include "onec/variant_utils.hpp"
#include "third_party/nlohmann_json.hpp"

using nlohmann::json;

namespace onec {
namespace {

const WCHAR_T kExtensionName[] = u"UapkiNative";
const WCHAR_T kClassNames[] = u"UapkiNative";
const WCHAR_T kPropLastError[] = u"LastError";
const WCHAR_T kMethodInitialize[] = u"Initialize";
const WCHAR_T kMethodSignFile[] = u"SignFile";
const WCHAR_T kMethodVerifyFile[] = u"VerifyFileSignature";
const WCHAR_T kMethodCertInfo[] = u"GetCertificateInfo";
const WCHAR_T kMethodSignData[] = u"SignData";
const WCHAR_T kMethodVerifyData[] = u"VerifyDataSignature";

const WCHAR_T* kMethodNames[] = {
    kMethodInitialize,
    kMethodSignFile,
    kMethodVerifyFile,
    kMethodCertInfo,
    kMethodSignData,
    kMethodVerifyData
};

inline size_t wcharLength(const WCHAR_T* value) {
    if (!value) {
        return 0;
    }
    size_t len = 0;
    while (value[len] != 0) {
        ++len;
    }
    return len;
}

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline bool equalsIgnoreCase(const WCHAR_T* lhs, const WCHAR_T* rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    const auto lhsLen = static_cast<uint32_t>(wcharLength(lhs));
    const auto rhsLen = static_cast<uint32_t>(wcharLength(rhs));
    if (lhsLen != rhsLen) {
        return false;
    }
    const std::string lhsUtf8 = utf16ToUtf8(lhs, lhsLen);
    const std::string rhsUtf8 = utf16ToUtf8(rhs, rhsLen);
    return toLower(lhsUtf8) == toLower(rhsUtf8);
}

json tryParseJson(const std::string& text) {
    if (text.empty()) {
        return json::object();
    }
    try {
        return json::parse(text);
    }
    catch (const std::exception&) {
        return json::object();
    }
}

}  // namespace

UapkiComponent::UapkiComponent()
    : memoryManager_(nullptr), connection_(nullptr) {}

UapkiComponent::~UapkiComponent() = default;

bool UapkiComponent::Init(void* connection) {
    connection_ = connection;
    return true;
}

bool UapkiComponent::setMemManager(void* mem) {
    memoryManager_ = static_cast<IMemoryManager*>(mem);
    return memoryManager_ != nullptr;
}

long UapkiComponent::GetInfo() {
    return ADDIN_E_INFO;
}

void UapkiComponent::Done() {
    connection_ = nullptr;
    memoryManager_ = nullptr;
}

bool UapkiComponent::RegisterExtensionAs(WCHAR_T** name) {
    if (!memoryManager_ || !name) {
        return false;
    }
    const size_t length = wcharLength(kExtensionName);
    auto* buffer = static_cast<WCHAR_T*>(memoryManager_->AllocMemory(static_cast<uint32_t>((length + 1) * sizeof(WCHAR_T))));
    if (!buffer) {
        return false;
    }
    std::copy(kExtensionName, kExtensionName + length + 1, buffer);
    *name = buffer;
    return true;
}

long UapkiComponent::GetNProps() {
    return static_cast<long>(PropertyId::Count);
}

long UapkiComponent::FindProp(const WCHAR_T* name) {
    if (equalsIgnoreCase(name, kPropLastError)) {
        return static_cast<long>(PropertyId::LastError);
    }
    return -1;
}

const WCHAR_T* UapkiComponent::GetPropName(long num, long) {
    switch (static_cast<PropertyId>(num)) {
        case PropertyId::LastError:
            return kPropLastError;
        default:
            return nullptr;
    }
}

bool UapkiComponent::GetPropVal(long num, tVariant* val) {
    if (!val) {
        return false;
    }
    switch (static_cast<PropertyId>(num)) {
        case PropertyId::LastError:
            try {
                variantSetUtf8String(*val, memoryManager_, lastError_);
                return true;
            }
            catch (const std::exception&) {
                return false;
            }
        default:
            return false;
    }
}

bool UapkiComponent::SetPropVal(long, const tVariant*) {
    return false;
}

bool UapkiComponent::IsPropReadable(long num) {
    return (static_cast<PropertyId>(num) == PropertyId::LastError);
}

bool UapkiComponent::IsPropWritable(long) {
    return false;
}

long UapkiComponent::GetNMethods() {
    return static_cast<long>(MethodId::Count);
}

long UapkiComponent::FindMethod(const WCHAR_T* name) {
    for (long i = 0; i < static_cast<long>(MethodId::Count); ++i) {
        if (equalsIgnoreCase(name, kMethodNames[i])) {
            return i;
        }
    }
    return -1;
}

const WCHAR_T* UapkiComponent::GetMethodName(long num, long) {
    if (num >= 0 && num < static_cast<long>(MethodId::Count)) {
        return kMethodNames[num];
    }
    return nullptr;
}

long UapkiComponent::GetNParams(long num) {
    switch (static_cast<MethodId>(num)) {
        case MethodId::Initialize:
            return 1;
        case MethodId::SignFile:
            return 3;
        case MethodId::VerifyFileSignature:
            return 3;
        case MethodId::GetCertificateInfo:
            return 1;
        case MethodId::SignData:
            return 2;
        case MethodId::VerifyDataSignature:
            return 3;
        default:
            return 0;
    }
}

bool UapkiComponent::GetParamDefValue(long, long, tVariant* defVal) {
    if (defVal) {
        variantSetEmpty(*defVal);
    }
    return false;
}

bool UapkiComponent::HasRetVal(long) {
    return true;
}

bool UapkiComponent::CallAsProc(long, tVariant*, long) {
    return false;
}

bool UapkiComponent::CallAsFunc(long num, tVariant* retValue, tVariant* params, long sizeArray) {
    if (!retValue) {
        return false;
    }
    try {
        lastError_.clear();
        switch (static_cast<MethodId>(num)) {
            case MethodId::Initialize:
                return handleInitialize(retValue, params, sizeArray);
            case MethodId::SignFile:
                return handleSignFile(retValue, params, sizeArray);
            case MethodId::VerifyFileSignature:
                return handleVerifyFile(retValue, params, sizeArray);
            case MethodId::GetCertificateInfo:
                return handleCertInfo(retValue, params, sizeArray);
            case MethodId::SignData:
                return handleSignData(retValue, params, sizeArray);
            case MethodId::VerifyDataSignature:
                return handleVerifyData(retValue, params, sizeArray);
            default:
                return false;
        }
    }
    catch (const std::exception& ex) {
        rememberError(ex.what());
        return false;
    }
}

void UapkiComponent::SetLocale(const WCHAR_T* loc) {
    if (!loc) {
        locale_.clear();
        return;
    }
    const auto length = static_cast<uint32_t>(wcharLength(loc));
    locale_ = utf16ToUtf8(loc, length);
}

bool UapkiComponent::handleInitialize(tVariant* retValue, tVariant* params, long count) {
    if (!ensureParamCount(1, count, params)) {
        return false;
    }
    std::string jsonText = variantToUtf8String(params[0]);
    auto parameters = jsonText.empty() ? json::object() : json::parse(jsonText);
    (void)uapki_.call("INIT", parameters);
    variantSetBool(*retValue, true);
    return true;
}

std::string UapkiComponent::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open file: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void UapkiComponent::writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to write file: " + path);
    }
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

bool UapkiComponent::handleSignFile(tVariant* retValue, tVariant* params, long count) {
    if (!ensureParamCount(3, count, params)) {
        return false;
    }
    const std::string dataPath = variantToUtf8String(params[0]);
    const std::string signaturePath = variantToUtf8String(params[1]);
    const std::string options = variantToUtf8String(params[2]);

    const auto dataBytes = readFile(dataPath);
    std::vector<uint8_t> dataVec(dataBytes.begin(), dataBytes.end());
    json parameters = tryParseJson(options);
    if (!parameters.is_object()) {
        parameters = json::object();
    }
    auto& dataArray = parameters["dataTbs"];
    if (!dataArray.is_array()) {
        dataArray = json::array();
    }
    dataArray.clear();
    dataArray.push_back({{"id", "doc-0"}, {"bytes", base64::encode(dataVec)}});

    auto signResult = uapki_.call("SIGN", parameters);
    const auto& signatures = signResult["signatures"];
    if (!signatures.is_array() || signatures.empty()) {
        throw std::runtime_error("UAPKI returned empty signatures list");
    }
    const std::string signatureBase64 = signatures[0].value("bytes", std::string());
    auto signatureBytes = base64::decode(signatureBase64);
    writeFile(signaturePath, signatureBytes);

    json verifyParams;
    verifyParams["signature"] = {{"bytes", signatureBase64}, {"content", base64::encode(dataVec)}, {"isDigest", false}};
    verifyParams["reportTime"] = true;
    auto verifyResult = uapki_.call("VERIFY", verifyParams);
    auto summary = buildSignSummary(signResult, verifyResult);
    summary["signaturePath"] = signaturePath;
    setResultString(retValue, summary.dump());
    return true;
}

bool UapkiComponent::handleVerifyFile(tVariant* retValue, tVariant* params, long count) {
    if (!ensureParamCount(3, count, params)) {
        return false;
    }
    const std::string dataPath = variantToUtf8String(params[0]);
    const std::string signaturePath = variantToUtf8String(params[1]);
    const std::string options = variantToUtf8String(params[2]);

    json verifyParams = tryParseJson(options);
    if (!verifyParams.is_object()) {
        verifyParams = json::object();
    }

    const auto signatureData = readFile(signaturePath);
    std::vector<uint8_t> signatureBytes(signatureData.begin(), signatureData.end());
    auto& signatureObject = verifyParams["signature"];
    if (!signatureObject.is_object()) {
        signatureObject = json::object();
    }
    signatureObject["bytes"] = base64::encode(signatureBytes);
    signatureObject["isDigest"] = false;

    if (!dataPath.empty()) {
        const auto fileData = readFile(dataPath);
        std::vector<uint8_t> fileBytes(fileData.begin(), fileData.end());
        signatureObject["content"] = base64::encode(fileBytes);
    }

    if (!verifyParams.contains("reportTime")) {
        verifyParams["reportTime"] = true;
    }

    auto verifyResult = uapki_.call("VERIFY", verifyParams);
    setResultString(retValue, buildVerifySummary(verifyResult).dump());
    return true;
}

bool UapkiComponent::handleCertInfo(tVariant* retValue, tVariant* params, long count) {
    if (!ensureParamCount(1, count, params)) {
        return false;
    }
    const std::string input = variantToUtf8String(params[0]);
    json certParams = tryParseJson(input);
    if (!certParams.is_object()) {
        const auto certData = readFile(input);
        std::vector<uint8_t> bytes(certData.begin(), certData.end());
        certParams = json::object();
        certParams["bytes"] = base64::encode(bytes);
    }
    auto certInfo = uapki_.call("CERT_INFO", certParams);
    setResultString(retValue, buildCertSummary(certInfo).dump());
    return true;
}

bool UapkiComponent::handleSignData(tVariant* retValue, tVariant* params, long count) {
    if (!ensureParamCount(2, count, params)) {
        return false;
    }
    const std::string data = variantToUtf8String(params[0]);
    const std::string options = variantToUtf8String(params[1]);

    std::vector<uint8_t> dataBytes(data.begin(), data.end());
    json parameters = tryParseJson(options);
    if (!parameters.is_object()) {
        parameters = json::object();
    }
    auto& dataArray = parameters["dataTbs"];
    if (!dataArray.is_array()) {
        dataArray = json::array();
    }
    dataArray.clear();
    dataArray.push_back({{"id", "buffer"}, {"bytes", base64::encode(dataBytes)}});
    auto signResult = uapki_.call("SIGN", parameters);
    const auto& signatures = signResult["signatures"];
    if (!signatures.is_array() || signatures.empty()) {
        throw std::runtime_error("UAPKI returned empty signatures list");
    }
    const std::string signatureBase64 = signatures[0].value("bytes", std::string());
    setResultString(retValue, signatureBase64);
    return true;
}

bool UapkiComponent::handleVerifyData(tVariant* retValue, tVariant* params, long count) {
    if (!ensureParamCount(3, count, params)) {
        return false;
    }
    const std::string data = variantToUtf8String(params[0]);
    const std::string signatureBase64 = variantToUtf8String(params[1]);
    const std::string options = variantToUtf8String(params[2]);

    json verifyParams = tryParseJson(options);
    if (!verifyParams.is_object()) {
        verifyParams = json::object();
    }
    auto& signatureObject = verifyParams["signature"];
    if (!signatureObject.is_object()) {
        signatureObject = json::object();
    }
    signatureObject["bytes"] = signatureBase64;
    signatureObject["content"] = base64::encode(std::vector<uint8_t>(data.begin(), data.end()));
    signatureObject["isDigest"] = false;
    verifyParams["reportTime"] = true;

    auto verifyResult = uapki_.call("VERIFY", verifyParams);
    setResultString(retValue, buildVerifySummary(verifyResult).dump());
    return true;
}

json UapkiComponent::buildSignSummary(const json& signResult, const json& verifyResult) {
    json summary;
    summary["success"] = true;
    summary["signatures"] = signResult.value("signatures", json::array());
    const auto& signatureInfos = verifyResult.value("signatureInfos", json::array());
    if (!signatureInfos.empty()) {
        const auto& info = signatureInfos.front();
        summary["signAlgo"] = info.value("signAlgo", "");
        summary["digestAlgo"] = info.value("digestAlgo", "");
        summary["signingTime"] = info.value("signingTime", info.value("bestSignatureTime", ""));
        summary["status"] = info.value("status", "");
        summary["validSignatures"] = info.value("validSignatures", false);
        summary["validDigests"] = info.value("validDigests", false);
        summary["signerCertId"] = info.value("signerCertId", "");
    }
    summary["reportTime"] = verifyResult.value("reportTime", "");
    summary["certIds"] = verifyResult.value("certIds", json::array());
    return summary;
}

json UapkiComponent::buildVerifySummary(const json& verifyResult) {
    json summary;
    summary["signatureInfos"] = verifyResult.value("signatureInfos", json::array());
    summary["reportTime"] = verifyResult.value("reportTime", "");
    summary["content"] = verifyResult.value("content", json::object());
    summary["certIds"] = verifyResult.value("certIds", json::array());
    return summary;
}

json UapkiComponent::buildCertSummary(const json& certInfo) {
    json summary;
    summary["serialNumber"] = certInfo.value("serialNumber", "");
    summary["issuer"] = certInfo.value("issuer", json::object());
    summary["subject"] = certInfo.value("subject", json::object());
    summary["validity"] = certInfo.value("validity", json::object());
    summary["selfSigned"] = certInfo.value("selfSigned", false);
    summary["signatureInfo"] = certInfo.value("signatureInfo", json::object());
    return summary;
}

bool UapkiComponent::ensureParamCount(long expected, long actual, tVariant* params) {
    if (actual < expected) {
        throw std::runtime_error("Invalid parameters count");
    }
    if (expected > 0 && !params) {
        throw std::runtime_error("Parameters pointer is null");
    }
    return true;
}

std::string UapkiComponent::variantToUtf8String(const tVariant& var) const {
    return variantGetUtf8String(var);
}

void UapkiComponent::setResultString(tVariant* retValue, const std::string& utf8) const {
    if (!retValue) {
        return;
    }
    variantSetUtf8String(*retValue, memoryManager_, utf8);
}

void UapkiComponent::rememberError(const std::string& message) {
    lastError_ = message;
}

}  // namespace onec

