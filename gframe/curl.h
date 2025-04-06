#ifndef CURL_H
#define CURL_H
#include <curl/curl.h>
#include <type_traits>
#include "compiler_features.h"
#include "fmt.h"

#ifndef CURL_VERSION_BITS
#define CURL_VERSION_BITS(x,y,z) ((x) << 16 | (y) << 8 | (z))
#endif

//curl_global_* functions were introduced in curl 7.8.0
static_assert(LIBCURL_VERSION_NUM >= CURL_VERSION_BITS(7,7,0), "Curl 7.7.0 or greater is required");

#if (LIBCURL_VERSION_NUM < CURL_VERSION_BITS(7,19,4))
// CURLOPT_NOPROXY was added in 7.19.4
#define CURLOPT_NOPROXY (static_cast<CURLoption>(10177))

// CURLOPT_WRITEDATA was added in 7.9.7 as alias to CURLOPT_FILE
#ifdef CURLOPT_WRITEDATA
#undef CURLOPT_WRITEDATA
#endif
#define CURLOPT_WRITEDATA CURLOPT_FILE
#endif

template<typename CharT>
struct fmt::formatter<CURLcode, CharT> : formatter<std::underlying_type_t<CURLcode>, CharT> {
	template <typename FormatContext>
	constexpr auto format(CURLcode value, FormatContext& ctx) const
		-> decltype(ctx.out()) {
		return formatter<std::underlying_type_t<CURLcode>, CharT>::format(value, ctx);
	}
	template <typename FormatContext>
	constexpr auto format(CURLcode value, FormatContext& ctx)
		-> decltype(ctx.out()) {
		return formatter<std::underlying_type_t<CURLcode>, CharT>::format(value, ctx);
	}
};

// curl prior to 7.17 didn't copy the strings passed to curl_easy_setopt
#if (LIBCURL_VERSION_NUM < CURL_VERSION_BITS(7,17,0))
// curl_easy_strerror was added in 7.12.0
#if (LIBCURL_VERSION_NUM < CURL_VERSION_BITS(7,12,0))
#define curl_easy_strerror(...) "???"
#endif
#include <array>
#include <string>
#include <utility>

class enrichedCurl {
public:
	enrichedCurl(CURL* curl) : m_curl{ curl } {}
	char* setStringOpt(epro::stringview str, int opt) {
		auto& cache_entry = cache[opt];
		cache_entry = str;
		return cache_entry.data();
	}
	operator CURL* () const {
		return m_curl;
	}
	static constexpr auto map_option(CURLoption option) {
		if(option == CURLOPT_CAINFO)
			return 0;
		else if(option == CURLOPT_NOPROXY)
			return 1;
		else if(option == CURLOPT_USERAGENT)
			return 2;
		else if(option == CURLOPT_URL)
			return 3;
		unreachable();
	}
private:
	CURL* m_curl;
	std::array<std::string, 4> cache;
};

template<CURLoption option, typename ...Args>
static inline auto curl_easy_setopt_int(enrichedCurl& curl, Args&&... args) {
	if constexpr(option == CURLOPT_CAINFO || option == CURLOPT_NOPROXY || option == CURLOPT_USERAGENT || option == CURLOPT_URL) {
		return curl_easy_setopt(curl, option, curl.setStringOpt(std::forward<Args>(args)..., enrichedCurl::map_option(option)));
	} else {
		return curl_easy_setopt(curl, option, std::forward<Args>(args)...);
	}
}
static inline auto curl_easy_init_int() {
	return curl_easy_init();
}
#ifdef curl_easy_setopt
#undef curl_easy_setopt
#endif
#ifdef curl_easy_init
#undef curl_easy_init
#endif
#if (LIBCURL_VERSION_NUM < CURL_VERSION_BITS(7,12,0))
// force library initialization by creating a CURL object and freeing it immediately after
#define curl_global_init(...) []{ auto* curl = (curl_easy_init)(); if(curl == nullptr) return CURLE_FAILED_INIT; curl_easy_cleanup(curl); return CURLE_OK; }()
#define curl_global_cleanup(...) (void)0
#endif
#define curl_easy_setopt(curl, option, ...) curl_easy_setopt_int<option>(curl, __VA_ARGS__)
#define curl_easy_init() enrichedCurl{curl_easy_init_int()}
#endif

#endif
