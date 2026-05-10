module;
#include <cstdio>

export module Fatal;
import std;

template <typename... Args> struct fatal_rformat {
  template <std::convertible_to<std::string_view> StrLike>
  consteval fatal_rformat(
      const StrLike &str,
      std::source_location location = std::source_location::current())
      : str(str), location(location) {}

  std::format_string<Args...> str;
  std::source_location location;
};

template <typename... Args>
using fatal_format = fatal_rformat<std::type_identity_t<Args>...>;

export template <typename... Args>
[[noreturn]] void fatal(fatal_format<Args...> fmt, Args &&...args) {
  try {
    const auto message = std::format(fmt.str, std::forward<Args>(args)...);

    std::println(stderr, "fatal: {}\n  at {}:{}:{}\n  in {}", message,
                 fmt.location.file_name(), fmt.location.line(),
                 fmt.location.column(), fmt.location.function_name());
  } catch (const std::exception &e) {
    std::println(
        stderr,
        "fatal: <failed to format fatal message: {}>\n  at {}:{}:{}\n  in {}",
        e.what(), fmt.location.file_name(), fmt.location.line(),
        fmt.location.column(), fmt.location.function_name());
  } catch (...) {
    std::println(
        stderr,
        "fatal: <failed to format fatal message>\n  at {}:{}:{}\n  in {}",
        fmt.location.file_name(), fmt.location.line(), fmt.location.column(),
        fmt.location.function_name());
  }

  std::abort();
}
