#include "crypto.h"

#include <array>

#include "MD5/md5.h"
#include "file_stream.h"

namespace epro {

MD5Context::MD5Context() {
	static_assert(sizeof(digest) == MD5_DIGEST_LENGTH);
	auto* md5_ctx = new MD5_CTX();
	MD5_Init(md5_ctx);
	ctx = md5_ctx;
}

MD5Context::~MD5Context() {
	delete static_cast<MD5_CTX*>(ctx);
}

void MD5Context::update(void* data, size_t len) {
	MD5_Update(static_cast<MD5_CTX*>(ctx), data, len);
}

auto MD5Context::final() -> digest {
	digest result;
	MD5_Final(result.data(), static_cast<MD5_CTX*>(ctx));
	return result;
}

MD5Context::digest calculateMD5(epro::path_stringview file) {
	FileStream instream{ file.data(), FileStream::in | FileStream::binary };
	if(instream.fail())
		return {};

	epro::MD5Context context{};
	std::array<char, 512> buff;
	while(!instream.eof()) {
		instream.read(buff.data(), buff.size());
		context.update(buff.data(), static_cast<size_t>(instream.gcount()));
	}
	return context.final();
}

}
