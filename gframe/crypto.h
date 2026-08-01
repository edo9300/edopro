#ifndef CRYPTO_H
#define CRYPTO_H

#include <array>
#include <cstdint>

#include "text_types.h"

namespace epro {

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

MD5Context::digest calculateMD5(epro::path_stringview file);

}

#endif //CRYPTO_H
