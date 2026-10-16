#pragma once

#include <string>
#include <cstdint>

enum class Lang {
    Cpp,
    Python,
    JS,
    TS,
    Rust, Go, CSharp, Java, Kotlin, Swift,
    Unknown
};

inline std::string lang_to_string(Lang l) {
    switch (l) {
    case Lang::Cpp:     return "cpp";
    case Lang::Python:  return "python";
    case Lang::JS:      return "javascript";
    case Lang::TS:      return "typescript";
    case Lang::Rust:    return "rust";
    case Lang::Go:      return "go";
    case Lang::CSharp:  return "csharp";
    case Lang::Java:    return "java";
    case Lang::Kotlin:  return "kotlin";
    case Lang::Swift:   return "swift";
    default:            return "unknown";
    }
}

