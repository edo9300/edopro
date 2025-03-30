#include <event2/event.h>
#include "address.h"
#include "bufferio.h"
#include "config.h"
#include "fmt.h"
#if EDOPRO_ANDROID
#include <netinet/in.h>
#endif

namespace epro {

Address::Address(const char* s) :Address{} {
	if(evutil_inet_pton(AF_INET, s, buffer) == 1) {
		family = INET;
		return;
	}
	if(evutil_inet_pton(AF_INET6, s, buffer) == 1) {
		family = INET6;
		return;
	}
}

Address::Address(const void* address, AF address_family) :Address{} {
	family = address_family;
	memcpy(buffer, address, family == INET ? sizeof(in_addr::s_addr) : sizeof(in6_addr::s6_addr));
}

void Address::setIP4(const uint32_t* ip) {
	family = INET;
	memcpy(buffer, ip, sizeof(in_addr::s_addr));
}

void Address::setIP6(const void* ip) {
	family = INET6;
	memcpy(buffer, ip, sizeof(in6_addr::s6_addr));
}

void Address::toInAddr(in_addr& sin_addr) const {
	memcpy(&sin_addr.s_addr, buffer, sizeof(in_addr::s_addr));
}

void Address::toIn6Addr(in6_addr& sin6_addr) const {
	memcpy(sin6_addr.s6_addr, buffer, sizeof(in6_addr::s6_addr));
}

std::string format_address(const Address& address) {
	if(address.family == address.UNK)
		return "";
	char ret[50]{};
	if(evutil_inet_ntop(address.family == address.INET ? AF_INET : AF_INET6, address.buffer, ret, sizeof(ret)) == nullptr)
		return "";
	return ret;
}

std::wstring wformat_address(const Address& address) {
	return BufferIO::DecodeUTF8(format_address(address));
}

bool Host::operator==(const Host& other) const {
	if(address.family != other.address.family)
		return false;
	if(port != other.port)
		return false;
	if(address.family == address.INET) {
		return memcmp(address.buffer, other.address.buffer, sizeof(in_addr::s_addr)) == 0;
	}
	return memcmp(address.buffer, other.address.buffer, sizeof(in6_addr::s6_addr)) == 0;
}

Host Host::resolve(epro::stringview address, uint16_t port) {
	Address resolved_address{ address.data() };
	if(resolved_address.family == Address::UNK) {
		evutil_addrinfo hints{};
		evutil_addrinfo* answer = nullptr;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = EVUTIL_AI_ADDRCONFIG;
		auto getAddrInfo = [&](auto ai_family) {
			hints.ai_family = ai_family;
			return evutil_getaddrinfo(address.data(), epro::to_string(port).data(), &hints, &answer) == 0;
		};
		//Give priority to ipv4 addresses
		if(getAddrInfo(AF_INET)) {
			auto* in_answer = reinterpret_cast<sockaddr_in*>(answer->ai_addr);
			resolved_address = Address{ &in_answer->sin_addr.s_addr, Address::INET };
		} else if(getAddrInfo(AF_INET6)) {
			auto* addr_ipv6 = reinterpret_cast<sockaddr_in6*>(answer->ai_addr);
			resolved_address = Address{ &addr_ipv6->sin6_addr.s6_addr, Address::INET6 };
		} else
			throw std::runtime_error("Host not resolved");
		evutil_freeaddrinfo(answer);
	}
	return { resolved_address, port };
}

}
