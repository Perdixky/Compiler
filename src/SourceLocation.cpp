export module SourceLocation;
import std;

export struct SourceLocation {
  std::string_view file;
  std::size_t line;
  std::size_t column;

  auto to_string() const -> std::string {
    return std::format("{}:{}:{}", file, line, column);
  }
};
