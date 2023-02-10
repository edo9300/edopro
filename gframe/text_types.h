#ifndef TEXT_TYPES_H_
#define TEXT_TYPES_H_
#include <string>
#include <fmt/core.h>
static_assert(FMT_VERSION >= 50300, "Fmt 5.3.0 or greater is required");
#include <fmt/printf.h>
#if FMT_VERSION >= 80000
#include <fmt/xchar.h>
#elif FMT_VERSION < 60000
#include <fmt/time.h>
#endif
#include "nonstd/string_view.hpp"
namespace nonstd {
namespace sv_lite {
template<typename T>
inline fmt::basic_string_view<T> to_string_view(const nonstd::basic_string_view<T>& s) {
	return { s.data(), s.size() };
}
}
}
namespace irr {
namespace core {
template<typename T>
class irrAllocator;
template<typename T, typename TAlloc>
class string;
template<typename T, typename TAlloc>
inline fmt::basic_string_view<T> to_string_view(const irr::core::string<T, TAlloc>& s) {
	return { s.data(), s.size() };
}
}
}

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

#if FMT_VERSION >= 80000
template <typename T, typename... Args, typename Char = typename T::value_type>
inline std::basic_string<Char> format(const T& format_str, Args&&... args) {
	return fmt::vformat(fmt::basic_string_view<Char>{ format_str.data(), format_str.size() },
				   fmt::make_format_args<fmt::buffer_context<Char>>(args...));
}
template <typename T, typename... Args, typename Char = typename T::value_type>
inline std::basic_string<Char> sprintf(const T& format_str, const Args&... args) {
	using context = fmt::basic_printf_context_t<Char>;
	return fmt::vsprintf(fmt::basic_string_view<Char>{ format_str.data(), format_str.size() },
					fmt::make_format_args<context>(args...));
}
template <std::size_t N, typename Char, typename... Args>
inline std::basic_string<Char> format(Char const (&format_str)[N], Args&&... args) {
	return fmt::vformat(fmt::basic_string_view<Char>{ format_str, N - 1 },
				   fmt::make_format_args<fmt::buffer_context<Char>>(args...));
}
#else
using fmt::format;
using fmt::sprintf;
#endif

}
using namespace nonstd::literals;
#endif /* TEXT_TYPES_H_ */