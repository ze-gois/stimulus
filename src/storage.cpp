#include "storage.hpp"
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace stimulus {
namespace {
int integer(const Json::Value& root, const char* key, int fallback) {
    if (!root.isMember(key)) return fallback;
    if (!root[key].isInt()) throw std::invalid_argument(std::string("Expected integer: ") + key);
    return root[key].asInt();
}
std::string stringify(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    return Json::writeString(builder, value);
}
Json::Value parse(const std::string& text) {
    Json::CharReaderBuilder builder;
    builder["rejectDupKeys"] = true;
    builder["failIfExtra"] = true;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errors;
    if (!reader->parse(text.data(), text.data() + text.size(), &root, &errors))
        throw std::invalid_argument("Invalid JSON: " + errors);
    return root;
}
}
Json::Value config_json(const Config& c) {
    c.validate();
    Json::Value r;
    r["type"] = name(c.kind);
    r["FPS"] = c.fps; r["duration"] = c.duration; r["repetitions"] = c.repetitions;
    r["random_seed"] = c.seed; r["font_size"] = c.font_size;
    r["center_x"] = c.center_x; r["center_y"] = c.center_y;
    r["n"] = c.count; r["size"] = c.size;
    r["inner_radius"] = c.inner_radius; r["outer_radius"] = c.outer_radius;
    return r;
}
Config read_config(const Json::Value& r) {
    if (!r.isObject() || !r["type"].isString()) throw std::invalid_argument("Missing stimulus type");
    Config c;
    const auto type = r["type"].asString();
    if (type == "Fixing") c.kind = Kind::Fixing;
    else if (type == "RandomCircles") c.kind = Kind::RandomCircles;
    else if (type == "ColoredWords") c.kind = Kind::ColoredWords;
    else throw std::invalid_argument("Unknown stimulus type: " + type);
    c.fps = integer(r, "FPS", c.fps); c.duration = integer(r, "duration", c.duration);
    // Accept historical misspellings but always write the canonical schema.
    const char* repetitions = r.isMember("repetitions") ? "repetitions" : r.isMember("repetitionw") ? "repetitionw" : "repetition";
    c.repetitions = integer(r, repetitions, c.repetitions);
    c.seed = integer(r, "random_seed", c.seed); c.font_size = integer(r, "font_size", c.font_size);
    c.center_x = integer(r, "center_x", c.center_x);
    c.center_y = integer(r, r.isMember("center_y") ? "center_y" : "center_Y", c.center_y);
    c.count = integer(r, "n", c.count); c.size = integer(r, "size", c.size);
    c.inner_radius = integer(r, "inner_radius", c.inner_radius);
    c.outer_radius = integer(r, r.isMember("outer_radius") ? "outer_radius" : "outter_radius", c.outer_radius);
    c.validate();
    return c;
}
std::string encode_library(const Library& library) {
    Json::Value root;
    root["schema_version"] = 1;
    root["stimuli"] = Json::Value(Json::arrayValue);
    root["sequence"] = Json::Value(Json::arrayValue);
    for (const auto& c : library.stimuli) root["stimuli"].append(config_json(c));
    for (const auto& c : library.sequence) root["sequence"].append(config_json(c));
    return stringify(root);
}
Library decode_library(const std::string& text) {
    const auto root = parse(text);
    Library result;
    if (root.isObject() && root.isMember("type")) { result.stimuli.push_back(read_config(root)); return result; }
    if (!root.isObject() || !root["schema_version"].isInt() || root["schema_version"].asInt() != 1 ||
        !root["stimuli"].isArray() || !root["sequence"].isArray())
        throw std::invalid_argument("Expected a version 1 library or a legacy stimulus");
    if (root["stimuli"].size() > 1000 || root["sequence"].size() > 1000)
        throw std::invalid_argument("Library exceeds 1000 items");
    for (const auto& c : root["stimuli"]) result.stimuli.push_back(read_config(c));
    for (const auto& c : root["sequence"]) result.sequence.push_back(read_config(c));
    return result;
}
std::string encode_session(const Session& session) {
    Json::Value root;
    root["schema_version"] = 1; root["status"] = session.status();
    root["time_unit"] = "seconds";
    root["timing_reference"] = "monotonic software frame boundary; not measured photon onset";
    root["trials"] = Json::Value(Json::arrayValue);
    for (const auto& t : session.trials()) {
        Json::Value trial;
        trial["config"] = config_json(t.config);
        trial["sequence_index"] = Json::UInt64(t.sequence_index);
        trial["repetition"] = t.repetition;
        trial["onset_seconds"] = t.onset_seconds; trial["end_seconds"] = t.end_seconds;
        trial["frames"] = Json::UInt64(t.frames);
        trial["responses"] = Json::Value(Json::arrayValue);
        for (const auto& r : t.responses) {
            Json::Value response;
            response["key"] = r.key; response["session_seconds"] = r.session_seconds;
            response["trial_seconds"] = r.trial_seconds; response["frame"] = Json::UInt64(r.frame);
            trial["responses"].append(response);
        }
        root["trials"].append(trial);
    }
    return stringify(root);
}
std::string encode_responses_csv(const Session& session) {
    std::ostringstream out;
    out << "trial,sequence_index,repetition,key,session_seconds,trial_seconds,frame\n" << std::setprecision(17);
    std::size_t i = 0;
    for (const auto& t : session.trials()) {
        for (const auto& r : t.responses)
            out << i << ',' << t.sequence_index << ',' << t.repetition << ',' << r.key << ','
                << r.session_seconds << ',' << r.trial_seconds << ',' << r.frame << '\n';
        ++i;
    }
    return out.str();
}
}
