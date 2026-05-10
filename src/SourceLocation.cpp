export module SourceLocation;
import std;

export struct SourceLocation {
  std::string_view file;
  size_t line;
  size_t column;

  auto to_string() const -> std::string {
    return std::format("{}:{}:{}", file, line, column);
  }
};
