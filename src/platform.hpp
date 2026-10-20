#pragma once
#include <optional>
#include <string>
namespace stimulus {
std::optional<std::string> load_library_text();
void save_library_text(const std::string& text);
void export_text(const std::string& filename, const std::string& text);
std::string read_import_text();
bool presentation_focused();
}
