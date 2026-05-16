set_languages("c++23")
set_toolchains("llvm")

package("mio")
  set_kind("library", {headeronly = true})
  set_homepage("https://github.com/mandreyel/mio")
  set_description("Cross-platform C++11 header-only library for memory mapped file IO")
  set_license("MIT")

  add_urls("https://github.com/mandreyel/mio.git")
  add_versions("2021.9.21", "3f86a95c0784d73ce6815237ec33ed25f233b643")
  add_versions("2023.3.3", "8b6b7d878c89e81614d05edca7936de41ccdd2da")

  add_deps("cmake")

  on_install("windows", "linux", "macosx", "bsd", "iphoneos", "android", function (package)
    import("package.tools.cmake").install(package, {"-Dmio.tests=OFF"})
  end)

  on_test(function (package)
    assert(package:check_cxxsnippets({
      test = [[
        #include <string>
        #include <vector>
        #include <algorithm>
        #include <mio/mmap.hpp>

        static void test() {
          mio::mmap_source mmap(mio::invalid_handle, 0, mio::map_entire_file);
        }
      ]]
    }, {configs = {languages = "c++14"}}))
  end)
package_end()

package("reflect-cpp")
  set_homepage("https://github.com/getml/reflect-cpp")
  set_description("A C++20 library for fast serialization, deserialization and validation using reflection.")
  set_license("MIT")

  add_urls("https://github.com/getml/reflect-cpp/archive/refs/tags/$(version).tar.gz",
           "https://github.com/getml/reflect-cpp.git", {submodules = false})

  add_configs("yyjson", {description = "Enable yyjson support.", default = true, type = "boolean"})
  add_configs("shared", {description = "Build shared library.", default = false, type = "boolean", readonly = true})

  on_load(function (package)
    package:add("deps", "cmake")
    package:add("deps", "ctre", {configs = {cmake = true}})

    if package:config("yyjson") then
      package:add("deps", "yyjson")
    end
  end)

  on_install(function (package)
    local configs = {
      "-DREFLECTCPP_USE_BUNDLED_DEPENDENCIES=OFF",
      "-DREFLECTCPP_USE_VCPKG=OFF",
      "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"),
      "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"),
      "-DREFLECTCPP_JSON=" .. (package:config("yyjson") and "ON" or "OFF"),
    }
    import("package.tools.cmake").install(package, configs)
  end)
package_end()

package("proxy")
  set_kind("library", {headeronly = true})
  set_homepage("https://github.com/ngcpp/proxy")
  set_description("Proxy: Easy Polymorphism in C++")
  set_license("MIT")

  add_urls("https://github.com/ngcpp/proxy.git")

  on_install(function (package)
    os.vcp("include/*", package:installdir("include"))
  end)

  on_test(function (package)
    assert(package:has_cxxincludes("proxy/proxy.h", {configs = {languages = "c++20"}}))
  end)
package_end()

package("boost-ext-ut")
  set_kind("library", {headeronly = true})
  set_homepage("https://github.com/boost-ext/ut")
  set_description("C++20 single-header/unit testing framework")
  set_license("BSL-1.0")

  add_urls("https://github.com/boost-ext/ut.git")

  on_install(function (package)
    os.vcp("include/*", package:installdir("include"))
  end)

  on_test(function (package)
    assert(package:has_cxxincludes("boost/ut.hpp", {configs = {languages = "c++20"}}))
  end)
package_end()

add_requires("mio 2023.3.3")
add_requires("reflect-cpp main", {system = false})
add_requires("proxy", {system = false})
add_requires("boost-ext-ut", {system = false})
add_requires("unordered_dense", {system = false, configs = {modules = true}})

target("compiler")
  add_files("src/*.cpp")
  set_policy("build.c++.modules", true)
  add_packages("reflect-cpp", "mio", "proxy", "unordered_dense")

target("unit_tests")
  set_kind("binary")
  set_default(false)
  add_files("tests/*.cpp")
  add_files("src/SourceLocation.cpp", "src/Token.cpp", "src/Fatal.cpp", "src/Lexer.cpp", "src/Parser.cpp", "src/Parser-LR.cpp")
  set_policy("build.c++.modules", true)
  add_packages("boost-ext-ut", "reflect-cpp", "proxy", "unordered_dense")
