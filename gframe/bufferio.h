#ifndef BUFFERIO_H
#define BUFFERIO_H

#include <string>
#include <vector>
#include <cstring>
#include <wchar.h>
#include <cstdint>
#include "text_types.h"

class BufferIO {
public:
	static void insert_data(std::vector<uint8_t>& vec, void* val, size_t len) {
		const auto vec_size = vec.size();
		const auto val_size = len;
		vec.resize(vec_size + val_size);
		std::memcpy(&vec[vec_size], val, val_size);
	}
	template<typename T>
	static void insert_value(std::vector<uint8_t>& vec, T val) {
		insert_data(vec, &val, sizeof(T));
	}
	template<typename input_type>
	static void Read(input_type*& p, void* dest, size_t size) {
		static_assert(std::is_same<std::remove_cv_t<input_type>, uint8_t>::value == true, "only uint8_t supported as buffer input");
		std::memcpy(dest, p, size);
		p += size;
	}
	template<typename T, typename input_type>
	static T Read(input_type*& p) {
		static_assert(std::is_same<std::remove_cv_t<input_type>, uint8_t>::value == true, "only uint8_t supported as buffer input");
		T ret;
		Read(p, &ret, sizeof(T));
		return ret;
	}
	template<typename T>
	static void Write(uint8_t*& p, T value) {
		std::memcpy(p, &value, sizeof(T));
		p += sizeof(T);
	}
	template<typename T>
	static int CopyStr(const T* src, T* pstr, size_t bufsize) {
		size_t l = 0;
		for(; src[l] && l < bufsize - 1; ++l)
			pstr[l] = src[l];
		pstr[l] = 0;
		return static_cast<int>(l);
	}
private:
	static constexpr inline bool isUtf16 = sizeof(wchar_t) == 2;
	template<bool check_output_size = false>
	static int EncodeUTF8internal(epro::wstringview source, char* out, size_t size = 0) {
		(void)size;
		char* pstr = out;
		auto GetNextSize = [](auto cur) -> size_t {
			if(cur < 0x80u)
				return 1;
			if(cur < 0x800u)
				return 2;
			if constexpr(!isUtf16) {
				if(cur < 0x10000u && (cur < 0xd800u || cur > 0xdfffu))
					return 3;
				return 4;
			} else {
				return 3;
			}
		};
		while(!source.empty()) {
			auto first_codepoint = static_cast<char32_t>(*source.begin());
			const auto codepoint_size = GetNextSize(first_codepoint);
			if constexpr(check_output_size) {
				if(size != 0 && ((out - pstr) + codepoint_size) >= (size - 1))
					break;
			}
			source.remove_prefix(1);
			switch(codepoint_size) {
			case 1:
				*out = static_cast<char>(first_codepoint);
				break;
			case 2:
				out[0] = static_cast<char>(((first_codepoint >> 6) & 0x1fu) | 0xc0u);
				out[1] = static_cast<char>(((first_codepoint) & 0x3fu) | 0x80u);
				break;
			case 3:
				out[0] = static_cast<char>(((first_codepoint >> 12) & 0xfu) | 0xe0u);
				out[1] = static_cast<char>(((first_codepoint >> 6) & 0x3fu) | 0x80u);
				out[2] = static_cast<char>(((first_codepoint) & 0x3fu) | 0x80u);
				break;
			case 4:
				char32_t unicode_codepoint = 0;
				if constexpr(isUtf16) {
					if(source.empty())
						break;
					auto second_codepoint = static_cast<char32_t>(*source.begin());
					source.remove_prefix(1);
					unicode_codepoint |= (first_codepoint & 0x3ffu) << 10;
					unicode_codepoint |= second_codepoint & 0x3ffu;
					unicode_codepoint += 0x10000u;
				} else {
					unicode_codepoint = first_codepoint;
				}
				out[0] = static_cast<char>(((unicode_codepoint >> 18) & 0x7u) | 0xf0u);
				out[1] = static_cast<char>(((unicode_codepoint >> 12) & 0x3fu) | 0x80u);
				out[2] = static_cast<char>(((unicode_codepoint >> 6) & 0x3fu) | 0x80u);
				out[3] = static_cast<char>((unicode_codepoint & 0x3fu) | 0x80u);
				break;
			}
			out += codepoint_size;
		}
		*out = 0;
		return static_cast<int>(out - pstr);
	}
	template<bool check_output_size = false>
	static int DecodeUTF8internal(epro::stringview source, wchar_t* out, size_t size = 0) {
		(void)size;
		wchar_t* pstr = out;
		while(!source.empty()) {
			auto first_codepoint = static_cast<unsigned char>(*source.begin());
			source.remove_prefix(1);
			if constexpr(check_output_size) {
				if(size != 0) {
					const size_t len = out - pstr;
					if(len >= (size - 1))
						break;
					if constexpr(isUtf16) {
						if((first_codepoint & 0xf8u) == 0xf0u && len >= (size - 2))
							break;
					}
				}
			}
			if((first_codepoint & 0x80u) == 0) {
				*out = static_cast<wchar_t>(first_codepoint);
			} else if((first_codepoint & 0xe0u) == 0xc0u) {
				if(source.size() < 1)
					break;
				auto second_codepoint = *source.begin();
				*out = static_cast<wchar_t>(((first_codepoint & 0x1fu) << 6) | (second_codepoint & 0x3fu));
				source.remove_prefix(1);
			} else if((first_codepoint & 0xf0u) == 0xe0u) {
				if(source.size() < 2)
					break;
				auto [second_codepoint, third_codepoint] = [data = source.data()] {return std::tuple{data[0], data[1]};}();
				*out = static_cast<wchar_t>(((first_codepoint & 0xfu) << 12) | ((second_codepoint & 0x3fu) << 6) | (third_codepoint & 0x3f));
				source.remove_prefix(2);
			} else if((first_codepoint & 0xf8u) == 0xf0) {
				if(source.size() < 3)
					break;
				auto [second_codepoint, third_codepoint, fourth_codepoint] = [data = source.data()]{return std::tuple{data[0], data[1], data[2]};}();
				char32_t unicode_codepoint = ((first_codepoint & 0x7u) << 18) | ((second_codepoint & 0x3fu) << 12) | ((third_codepoint & 0x3fu) << 6) | (fourth_codepoint & 0x3fu);
				if constexpr(isUtf16) {
					unicode_codepoint -= 0x10000u;
					*out++ = static_cast<wchar_t>((unicode_codepoint >> 10) | 0xd800u);
					*out = static_cast<wchar_t>((unicode_codepoint & 0x3ffu) | 0xdc00u);
				} else {
					*out = static_cast<wchar_t>(unicode_codepoint);
				}
				source.remove_prefix(3);
			}
			out++;
		}
		*out = 0;
		return static_cast<int>(out - pstr);
	}
public:
	// UTF-16/UTF-32 to UTF-8
	static int EncodeUTF8(const wchar_t* wsrc, char* out, size_t size) {
		return EncodeUTF8internal<true>(wsrc, out, size);
	}
	static std::string EncodeUTF8(epro::wstringview source) {
		std::string res(source.size() * 4 + 1, L'\0');
		res.resize(EncodeUTF8internal<false>(source, &res[0]));
		return res;
	}
	// UTF-8 to UTF-16/UTF-32
	static int DecodeUTF8(const char* src, wchar_t* out, size_t size) {
		return DecodeUTF8internal<true>(src, out, size);
	}
	static std::wstring DecodeUTF8(epro::stringview source) {
		std::wstring res(source.size() + 1, '\0');
		res.resize(DecodeUTF8internal<false>(source.data(), &res[0]));
		return res;
	}
	// UTF-16 to UTF-16/UTF-32
	static int DecodeUTF16(epro::basic_string_view<uint16_t> source, wchar_t* out, size_t size) {
		if constexpr(isUtf16) {
			auto src_size = std::min<size_t>(source.size() + 1, size);
			std::copy_n(source.begin(), src_size, out);
			out[size - 1] = 0;
			return static_cast<int>(src_size);
		} else {
			wchar_t* pstr = out;
			while(source.empty()) {
				const size_t len = out - pstr;
				if(len >= (size - 1))
					break;
				auto first_codepoint = *source.begin();
				source.remove_prefix(1);
				if((first_codepoint - 0xd800u) >= 0x800u) {
					*out++ = static_cast<wchar_t>(first_codepoint);
				} else if((first_codepoint & 0xfffffc00u) == 0xd800u) {
					if(source.empty())
						break;
					auto second_codepoint = *source.begin();
					if((second_codepoint & 0xfffffc00u) == 0xdc00u) {
						*out++ = static_cast<wchar_t>((static_cast<uint32_t>(first_codepoint) << 10) + static_cast<uint32_t>(second_codepoint) - 0x35fdc00u);
						source.remove_prefix(1);
					}
				}
			}
			*out = 0;
			return static_cast<int>(out - pstr);
		}
	}
	// UTF-16/UTF-32 to UTF-16
	static int EncodeUTF16(epro::wstringview source, uint16_t* out, size_t size) {
		if constexpr(isUtf16) {
			auto src_size = std::min<size_t>(source.size() + 1, size);
			std::copy_n(source.data(), src_size, out);
			out[src_size] = 0;
			return static_cast<int>(src_size);
		} else {
			auto* pstr = out;
			for(char32_t codepoint : source) {
				const size_t len = out - pstr;
				if(len >= (size - 1))
					break;
				if(codepoint >= 0x10000 && len >= (size - 2))
					break;
				if(codepoint < 0x10000) {
					*out++ = static_cast<uint16_t>(codepoint);
				} else {
					uint32_t unicode = codepoint - 0x10000u;
					*out++ = static_cast<uint16_t>((unicode >> 10) | 0xd800);
					*out++ = static_cast<uint16_t>((unicode & 0x3ff) | 0xdc00);
				}
			}
			*out = 0;
			return static_cast<int>(out - pstr);
		}
	}
	static uint32_t GetVal(const wchar_t* pstr) {
		uint32_t ret = 0;
		while(*pstr >= L'0' && *pstr <= L'9') {
			ret = ret * 10 + (*pstr - L'0');
			pstr++;
		}
		if(*pstr == 0)
			return ret;
		return 0;
	}

	template<typename T>
	static T getStruct(const void* data, size_t len) {
		T pkt{};
		memcpy(&pkt, data, std::min(sizeof(T), len));
		return pkt;
	}
};

#endif //BUFFERIO_H
