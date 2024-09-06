#ifndef ADDRESS_H
#define ADDRESS_H

#include <cstdint>
#include <string>
#include "bufferio.h"
#include "text_types.h"

extern "C" {
	struct in_addr;
	struct in6_addr;
}

namespace epro {

struct Address {
	friend struct Host;
	friend std::string format_address(const Address& address);
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

std::string format_address(const Address&);
std::wstring wformat_address(const Address&);

}

#endif //ADDRESS_H
