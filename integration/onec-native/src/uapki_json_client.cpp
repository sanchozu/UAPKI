#include "onec/uapki_json_client.hpp"

#include <stdexcept>
#include <string>

#include "onec/addin_api.hpp"
#include "third_party/nlohmann_json.hpp"
#include "uapki-export.h"

namespace onec {

UapkiJsonClient::UapkiJsonClient() = default;

nlohmann::json UapkiJsonClient::call(const std::string& method, const nlohmann::json& parameters) {
    nlohmann::json request;
    request["method"] = method;
    request["parameters"] = parameters;
    auto response = nlohmann::json::parse(invokeProcess(request.dump()));
    const auto errorCode = response.value("errorCode", 0);
    if (errorCode != 0) {
        throw std::runtime_error(response.value("error", std::string("UAPKI error")) + " (#" + std::to_string(errorCode) + ")");
    }
    return response.value("result", nlohmann::json::object());
}

nlohmann::json UapkiJsonClient::call(const std::string& method) {
    return call(method, nlohmann::json::object());
}

std::string UapkiJsonClient::invokeProcess(const std::string& request) {
    char* response = process(request.c_str());
    if (!response) {
        throw std::runtime_error("uapki::process returned null");
    }
    std::string result(response);
    json_free(response);
    return result;
}

}  // namespace onec

