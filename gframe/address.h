#ifndef ADDRESS_H
#define ADDRESS_H

#include <cstdint>
#include <string>
#include "fmt.h"
#include "bufferio.h"
#include "text_types.h"

extern "C" {
	struct in_addr;
	struct in6_addr;
}

namespace epro {

struct Address {
	friend struct Host;
private:
	uint8_t buffer[32]{}; //buffer big enough to store an ipv6
public:
	enum AF : uint8_t {
		UNK,
		INET,
		INET6,
	};
	Address() : family(UNK) {}
	explicit Address(const char* s);
	Address(const void* address, AF family);
	AF family;
	void setIP4(const uint32_t* ip);
	void setIP6(const void* ip);
	template<typename T>
	std::basic_string<T> format() const;
	void toInAddr(in_addr& sin_addr) const;
	void toIn6Addr(in6_addr& sin6_addr) const;
};

struct Host {
	Address address;
	uint16_t port;
	bool operator==(const Host& other) const;
	static Host resolve(epro::wstringview address, epro::wstringview port) {
		return resolve(BufferIO::EncodeUTF8(address), static_cast<uint16_t>(std::stoi({ port.data(), port.size() })));
	}
	static Host resolve(epro::stringview address, uint16_t port);
};

template<>
std::basic_string<char> Address::format() const;
template<>
std::basic_string<wchar_t> Address::format() const;
}

template<typename T>
struct fmt::formatter<epro::Address, T> {
	template<typename ParseContext>
	constexpr auto parse(ParseContext& ctx) const { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const epro::Address& address, FormatContext& ctx) const {
		static constexpr auto format_str = CHAR_T_STRINGVIEW(T, "{}");
		return fmt::format_to(ctx.out(), format_str.data(), address.format<T>());
	}
};

#endif //ADDRESS_H
