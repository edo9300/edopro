#include "crypto.h"

#include <array>

#ifdef EPRO_USE_OPENSSL_CRYPTO

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#else

#include "MD5/md5.h"

#endif

#include "file_stream.h"

namespace epro {

#ifndef EPRO_USE_OPENSSL_CRYPTO

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

#else

MD5Context::MD5Context() : CryptoContext(EVP_md5()) {
	static_assert(sizeof(digest) == MD5_DIGEST_LENGTH);
}

SHA256Context::SHA256Context() : CryptoContext(EVP_sha256()) {
	static_assert(sizeof(digest) == SHA256_DIGEST_LENGTH);
}

CryptoContext::CryptoContext(const void* evp) {
	auto* evp_md_ctx = EVP_MD_CTX_new();
	if(!EVP_DigestInit_ex(evp_md_ctx, static_cast<const EVP_MD*>(evp), nullptr)) {
		EVP_MD_CTX_free(evp_md_ctx);
		return;
	}
	ctx = evp_md_ctx;
}

CryptoContext::~CryptoContext() {
	EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(ctx));
}

void CryptoContext::update(void* data, size_t len) {
	EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(ctx), data, len);
}

void CryptoContext::final(void* data) {
	EVP_DigestFinal_ex(static_cast<EVP_MD_CTX*>(ctx), static_cast<unsigned char*>(data), nullptr);
}

SHA256Context::digest calculateSHA256(epro::path_stringview file) {
	FileStream instream{ file.data(), FileStream::in | FileStream::binary };
	if(instream.fail())
		return {};

	epro::SHA256Context context{};
	std::array<char, 512> buff;
	while(!instream.eof()) {
		instream.read(buff.data(), buff.size());
		context.update(buff.data(), static_cast<size_t>(instream.gcount()));
	}
	return context.final();
}

#endif

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
