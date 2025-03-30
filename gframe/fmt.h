#ifndef FMT_H
#define FMT_H
#ifdef _MSC_VER
#define FMT_UNICODE 0
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4541) // 'dynamic_cast' used on polymorphic type
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

#ifdef FMT_UNICODE
#undef FMT_UNICODE
#endif

namespace irr {namespace core {
template<typename CharT, typename TAlloc>
class string;
}}

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
using fmt::print;
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
