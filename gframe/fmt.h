#ifndef FMT_H
#define FMT_H
#ifdef _MSC_VER
#define FMT_UNICODE 0
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
		return format_to(ctx.out(), fmt::basic_string_view{s.data(), s.size()});
	}
};

namespace epro {
using fmt::format;
using fmt::sprintf;
using fmt::print;
}

#endif
