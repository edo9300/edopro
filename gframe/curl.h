#ifndef CURL_H
#define CURL_H
#include <curl/curl.h>
#include <type_traits>
#include "fmt.h"

//CURLOPT_NOPROXY requires 7.19.4
//even if that was removed, we won't be able to go below 7.17.0 since before that strings were not
//copied by curl, requiring a restructuring of the code
static_assert(LIBCURL_VERSION_MAJOR > 7 || LIBCURL_VERSION_MINOR >= 19 || (LIBCURL_VERSION_MINOR == 19 && LIBCURL_VERSION_PATCH >= 4), "Curl 7.19.4 or greater is required");

template<typename CharT>
struct fmt::formatter<CURLcode, CharT> : formatter<std::underlying_type_t<CURLcode>, CharT> {
	template <typename FormatContext>
	constexpr auto format(CURLcode value, FormatContext& ctx) const
		-> decltype(ctx.out()) {
		return formatter<std::underlying_type_t<CURLcode>, CharT>::format(value, ctx);
	}
};

#endif
