#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace onec {
namespace base64 {

std::string encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> decode(const std::string& data);

}  // namespace base64
}  // namespace onec

