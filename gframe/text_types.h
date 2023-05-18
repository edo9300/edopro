#ifndef TEXT_TYPES_H_
#define TEXT_TYPES_H_
#include <string>
#include "fmt.h"
#include "nonstd/string_view.hpp"

// Double macro to convert the macro-defined int to a character literal
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
namespace epro {
#ifdef UNICODE
#define EPRO_TEXT(x) L##x
using path_char = wchar_t;
#else
#define EPRO_TEXT(x) x
using path_char = char;
#endif // UNICODE
using path_string = std::basic_string<path_char>;
using nonstd::basic_string_view;
using path_stringview = basic_string_view<path_char>;
using stringview = basic_string_view<char>;
using wstringview = basic_string_view<wchar_t>;

namespace Detail {
template<typename Char>
constexpr epro::basic_string_view<Char> CHAR_T_STRINGVIEW(epro::stringview, epro::wstringview);
template<>
constexpr epro::stringview CHAR_T_STRINGVIEW(epro::stringview string, epro::wstringview) { return string; }
template<>
constexpr epro::wstringview CHAR_T_STRINGVIEW(epro::stringview, epro::wstringview string) { return string; }
}

#define CHAR_T_STRINGVIEW(Char, text) epro::Detail::CHAR_T_STRINGVIEW<Char>(text ""_sv, L"" text ""_sv)

}
using namespace nonstd::literals;
#endif /* TEXT_TYPES_H_ */
