#pragma once

#include "array.hpp"

#include <string>

namespace minisql {

std::string trim(const std::string& input);
std::string to_lower(std::string input);
bool valid_identifier(const std::string& name);
Array<std::string> split_csv(const std::string& input);
std::string escape_field(const std::string& value);
std::string unescape_field(const std::string& value);
std::string json_escape(const std::string& value);

}  // namespace minisql
