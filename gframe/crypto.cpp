#include "crypto.h"

#include <array>

#ifdef EPRO_BINARY_SIGNING

#include <memory>

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

#include "fmt.h"

extern const size_t public_key_len;
extern const unsigned char public_key[];

#else

#include "MD5/md5.h"

#endif

#include "file_stream.h"

namespace epro {

#ifndef EPRO_BINARY_SIGNING

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

class SignContextImpl {
private:
	using pubkey_ptr = std::unique_ptr<EVP_PKEY, void(*)(EVP_PKEY*)>;
	using mctx_ptr = std::unique_ptr<EVP_MD_CTX, void(*)(EVP_MD_CTX*)>;
	pubkey_ptr pub;
	mctx_ptr mctx;
public:
	SignContextImpl() :pub{ nullptr, [](auto*) {} }, mctx{ nullptr, [](auto*) {} } {
		pub = pubkey_ptr{
			d2i_PUBKEY_bio(std::unique_ptr<BIO, int(*)(BIO*)>{
				BIO_new_mem_buf(public_key, public_key_len),
				&BIO_free
			}.get(), nullptr),
			&EVP_PKEY_free
		};
		if(!pub)
			return;

		mctx = mctx_ptr{
			EVP_MD_CTX_new(),
			&EVP_MD_CTX_free
		};

		if(EVP_DigestVerifyInit(mctx.get(), nullptr, EVP_sha256(), nullptr, pub.get()) != 1) {
			return;
		}
	}

	bool update(void* data, size_t len) {
		return EVP_DigestVerifyUpdate(mctx.get(), data, len) == 1;
	}

	bool final(const std::array<uint8_t, 512>& signature) {
		return EVP_DigestVerifyFinal(mctx.get(), signature.data(), signature.size()) == 1;
	}
};

SignContext::SignContext(const std::array<uint8_t, 512>& signature) : signature(signature) {
	ctx = new SignContextImpl();
}

SignContext::~SignContext() {
	delete static_cast<SignContextImpl*>(ctx);
}
bool SignContext::update(void* data, size_t len) {
	return static_cast<SignContextImpl*>(ctx)->update(data, len);
}

bool SignContext::verify() {
	return static_cast<SignContextImpl*>(ctx)->final(signature);
}

std::optional<SignContext::digest> loadFileSignature(epro::path_stringview file) {
	SignContext::digest res{};
	FileStream instream{ epro::format(EPRO_TEXT("{}.sign"), file), FileStream::in | FileStream::binary | FileStream::ate };
	if(instream.fail())
		return std::nullopt;
	auto size = instream.tellg();
	instream.seekg(0, std::ios::beg);
	if(size != 512)
		return std::nullopt;
	instream.read(reinterpret_cast<char*>(res.data()), 512);
	return res;
}

bool verifyFileSignature(epro::path_stringview file, const SignContext::digest& signature) {
	SignContext s{ signature };

	FileStream instream{ file.data(), FileStream::in | FileStream::binary };
	if(instream.fail())
		return false;

	std::array<char, 512> buff;
	while(!instream.eof()) {
		instream.read(buff.data(), buff.size());
		if(!s.update(buff.data(), static_cast<size_t>(instream.gcount())))
			return false;
	}

	return s.verify();
}

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
