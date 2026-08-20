#ifndef CRYPTO_H
#define CRYPTO_H

#include <array>
#include <cstdint>
#include <optional>

#include "text_types.h"

namespace epro {

#ifndef EPRO_BINARY_SIGNING

class MD5Context {
public:
	using digest = std::array<uint8_t, 16>;
	MD5Context();
	~MD5Context();

	void update(void* data, size_t len);
	digest final();
private:
	void* ctx;
};

#else

class SignContext {
public:
	using digest = std::array<uint8_t, 512>;
	SignContext(const std::array<uint8_t, 512>& signature);
	~SignContext();

	SignContext(const SignContext&) = delete; // non construction-copyable
	SignContext& operator=(const SignContext&) = delete; // non copyable
	SignContext(SignContext&&) = delete; // non construction-movable
	SignContext& operator=(SignContext&&) = delete; // non movable

	bool update(void* data, size_t len);
	bool verify();
private:
	void* ctx;
	digest signature;
};

class CryptoContext {
public:
	~CryptoContext();

	void update(void* data, size_t len);

protected:
	CryptoContext(const void* evp);
	void final(void* buff);

private:
	void* ctx;

};

class MD5Context final : public CryptoContext {
public:
	using digest = std::array<uint8_t, 16>;
	MD5Context();

	digest final() {
		digest ret;
		CryptoContext::final(ret.data());
		return ret;
	}
};

class SHA256Context final : public CryptoContext {
public:
	using digest = std::array<uint8_t, 32>;
	SHA256Context();

	digest final() {
		digest ret;
		CryptoContext::final(ret.data());
		return ret;
	}
};

std::optional<SignContext::digest> loadFileSignature(epro::path_stringview file);
bool verifyFileSignature(epro::path_stringview file, const SignContext::digest& signature);
SHA256Context::digest calculateSHA256(epro::path_stringview file);
#endif

MD5Context::digest calculateMD5(epro::path_stringview file);

}

#endif //CRYPTO_H
