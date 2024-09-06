#ifndef TEXT_TYPES_H_
#define TEXT_TYPES_H_
#include <string>
#include <string_view>

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
using std::basic_string_view;
using path_stringview = basic_string_view<path_char>;
using stringview = basic_string_view<char>;
using wstringview = basic_string_view<wchar_t>;

namespace Detail {
template<typename Char>
static constexpr inline epro::basic_string_view<Char> CHAR_T_STRINGVIEW(epro::stringview stringview, epro::wstringview wstringview) {
	if constexpr(std::is_same_v<Char, wchar_t>) {
		return wstringview;
	} else {
		return stringview;
	}
}
}

#define CHAR_T_STRINGVIEW(Char, text) epro::Detail::CHAR_T_STRINGVIEW<Char>(text ""sv, L"" text ""sv)

}
template<typename T1, typename T2>
bool starts_with(const T1& stringview, const T2& token) {
	if constexpr(std::is_same_v<std::remove_cv_t<T2>, typename T1::value_type>) {
		return stringview.size() >= 1 && *stringview.begin() == token;
	} else {
		epro::basic_string_view token_sv{token};
		return stringview.size() >= token_sv.size() && memcmp(stringview.data(), token_sv.data(), token_sv.size()) == 0;
	}
}
using namespace std::string_view_literals;
#endif /* TEXT_TYPES_H_ */
