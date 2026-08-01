#ifndef CRYPTO_H
#define CRYPTO_H

#include <array>
#include <cstdint>

#include "text_types.h"

namespace epro {

#ifndef EPRO_USE_OPENSSL_CRYPTO

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

SHA256Context::digest calculateSHA256(epro::path_stringview file);
#endif

MD5Context::digest calculateMD5(epro::path_stringview file);

}

#endif //CRYPTO_H
