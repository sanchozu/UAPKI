#pragma once

#include <string>
#include <vector>

#include "onec/addin_api.hpp"
#include "third_party/nlohmann_json.hpp"

namespace onec {

class UapkiJsonClient {
public:
    UapkiJsonClient();

    nlohmann::json call(const std::string& method, const nlohmann::json& parameters);
    nlohmann::json call(const std::string& method);

private:
    std::string invokeProcess(const std::string& request);
};

}  // namespace onec

