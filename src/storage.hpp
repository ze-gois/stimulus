#pragma once
#include "model.hpp"
#include <json/json.h>

namespace stimulus {
struct Library { std::vector<Config> stimuli, sequence; };
Json::Value config_json(const Config& config);
Config read_config(const Json::Value& root);
std::string encode_library(const Library& library);
Library decode_library(const std::string& text);
std::string encode_session(const Session& session);
std::string encode_responses_csv(const Session& session);
}
