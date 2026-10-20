#include "platform.hpp"
#include "storage.hpp"
#include "raylib.h"
#include <stdexcept>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>
EM_JS(char*, browser_load, (), {
    try { const s = localStorage.getItem('stimulus.library.v1'); return s === null ? 0 : stringToNewUTF8(s); }
    catch (e) { return stringToNewUTF8('!Storage unavailable: ' + e.message); }
});
EM_JS(int, browser_save, (const char* text), {
    try { localStorage.setItem('stimulus.library.v1', UTF8ToString(text)); return 1; }
    catch (e) { return 0; }
});
EM_JS(void, browser_download, (const char* filename, const char* text), {
    const url = URL.createObjectURL(new Blob([UTF8ToString(text)], {type:'text/plain;charset=utf-8'}));
    const a = document.createElement('a'); a.href = url; a.download = UTF8ToString(filename);
    document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 1000);
});
#else
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
namespace {
std::string read_file(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 4 * 1024 * 1024) throw std::runtime_error("JSON exceeds 4 MiB");
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void write_file(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    const auto tmp = path.string() + ".tmp";
    std::ofstream file(tmp, std::ios::binary);
    file << text;
    file.close();
    if (!file) throw std::runtime_error("Cannot write " + path.string());
    std::filesystem::rename(tmp, path);
}
}
#endif
namespace stimulus {
std::optional<std::string> load_library_text() {
#ifdef __EMSCRIPTEN__
    char* data = browser_load();
    if (!data) return std::nullopt;
    std::string text(data); std::free(data);
    if (!text.empty() && text.front() == '!') throw std::runtime_error(text.substr(1));
    return text;
#else
    if (std::filesystem::exists("files/library.json")) return read_file("files/library.json");
    // Migrate legacy one-stimulus-per-file saves without deleting the originals.
    if (!std::filesystem::exists("files/stimuli")) return std::nullopt;
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator("files/stimuli"))
        if (entry.is_regular_file() && entry.path().extension() == ".json") paths.push_back(entry.path());
    if (paths.empty()) return std::nullopt;
    std::sort(paths.begin(), paths.end());
    Library result;
    for (const auto& path : paths) {
        const auto part = decode_library(read_file(path));
        result.stimuli.insert(result.stimuli.end(), part.stimuli.begin(), part.stimuli.end());
    }
    return encode_library(result);
#endif
}
void save_library_text(const std::string& text) {
#ifdef __EMSCRIPTEN__
    if (!browser_save(text.c_str())) throw std::runtime_error("Browser storage unavailable or full; export JSON instead");
#else
    write_file("files/library.json", text);
#endif
}
void export_text(const std::string& filename, const std::string& text) {
#ifdef __EMSCRIPTEN__
    browser_download(filename.c_str(), text.c_str());
#else
    write_file(std::filesystem::path("files/exports") / filename, text);
#endif
}
std::string read_import_text() {
#ifdef __EMSCRIPTEN__
    throw std::runtime_error("Use Import JSON above the canvas");
#else
    return read_file("files/import.json");
#endif
}
bool presentation_focused() {
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({return !document.hidden && document.hasFocus();}) != 0;
#else
    return IsWindowFocused();
#endif
}
}
