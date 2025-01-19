#ifndef FMT_H
#define FMT_H
#ifdef _MSC_VER
#define FMT_UNICODE 0
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4541) // 'dynamic_cast' used on polymorphic type
#pragma warning(disable : 4127) // conditional expression is constant
#endif
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtautological-compare"
#endif

#include <fmt/core.h>
static_assert(FMT_VERSION >= 50000, "Fmt 5.0.0 or greater is required");
#include <fmt/printf.h>
#if FMT_VERSION >= 80000
#include <fmt/xchar.h>
#endif
#if FMT_VERSION >= 60000
#include <fmt/chrono.h>
#else
#include <fmt/time.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if ((FMT_VERSION / 100) == 502)
#include <string_view>
// fmt 5.2.X are the only versions of fmt not supporting string views as format strings
// in earlier versions they worked, probably out of luck, in 5.3.0 they  were explicitly
// handled and allowed in the api, borrow the internal api in case of fmt 5.2.X and add
// manually support for string views
template <typename Char>
struct fmt::internal::format_string_traits<std::basic_string_view<Char>> :
	format_string_traits_base<Char> {};
#endif

#ifdef FMT_UNICODE
#undef FMT_UNICODE
#endif
#include "compiler_features.h"
#if EDOPRO_PSVITA
#include "porting.h"
#endif

#include "text_types.h"

namespace irr::core {
template<typename CharT, typename TAlloc>
class string;
}

template<typename CharT, typename TAlloc>
struct fmt::formatter<irr::core::string<CharT, TAlloc>, CharT> {
	template<typename ParseContext>
	constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
	template <typename ParseContext>
	constexpr auto format(const irr::core::string<CharT, TAlloc>& s, ParseContext& ctx) const {
		return format_to(ctx.out(), basic_string_view{s.data(), s.size()});
	}
};

namespace epro {
struct Address;
std::string format_address(const Address&);
std::wstring wformat_address(const Address&);
}

template<typename T>
struct fmt::formatter<epro::Address, T> {
	template<typename ParseContext>
	constexpr auto parse(ParseContext& ctx) const { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const epro::Address& address, FormatContext& ctx) const {
		static constexpr auto format_str = CHAR_T_STRINGVIEW(T, "{}");
		const auto formatted = [&] {
			if constexpr(std::is_same_v<T, wchar_t>) {
				return wformat_address(address);
			} else {
				return format_address(address);
			}
		}();
		return format_to(ctx.out(), format_str, formatted);
	}
};

namespace epro {
using fmt::format;
using fmt::sprintf;
#if !EDOPRO_PSVITA
using fmt::print;
#else
template<typename... Args>
void print(Args&&... args) {
	porting::print(epro::format(std::forward<Args>(args)...));
}
#endif
using fmt::format_to_n;
#if FMT_VERSION >= 60000
using fmt::to_string;
using fmt::to_wstring;
#else
template <typename T> inline std::string to_string(const T& value) {
	return format("{}", value);
}
template <typename T> inline std::wstring to_wstring(const T& value) {
	return format(L"{}", value);
}
#endif
}

#endif
