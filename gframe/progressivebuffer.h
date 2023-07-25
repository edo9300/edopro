// Copyright (c) 2019-2022 Edoardo Lolletti <edoardo762@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
// Refer to the COPYING file included.

#ifndef PROGRESSIVEBUFFER_H_
#define PROGRESSIVEBUFFER_H_

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>

class ProgressiveBuffer {
public:
	std::vector<uint8_t> data;
	ProgressiveBuffer() {};
	void clear() {
		data.clear();
	}
	template<class T>
	T at(const size_t pos) const {
		static constexpr auto valsize = sizeof(T);
		size_t size = (pos + 1) * valsize;
		T ret{};
		if(data.size() < size)
			return ret;
		memcpy(&ret, data.data() + pos * valsize, sizeof(T));
		return ret;
	}
	template<class T>
	void set(const size_t pos, T val) {
		static constexpr auto valsize = sizeof(T);
		size_t size = (pos + 1) * valsize;
		if(data.size() < size)
			data.resize(size);
		memcpy(data.data() + pos * valsize, &val, sizeof(T));
	}
	bool bitGet(const size_t pos) const {
		size_t real_pos = pos / 8u;
		uint32_t index = pos % 8u;
		size_t size = real_pos + 1;
		if(data.size() < size)
			return false;
		return !!(data[real_pos] & (1 << index));
	}
	void bitToggle(const size_t pos, bool set) {
		size_t real_pos = pos / 8u;
		uint32_t index = pos % 8u;
		size_t size = real_pos + 1;
		if(data.size() < size) {
			data.resize(size);
		}
		if(set)
			data[real_pos] |= (1 << index);
		else {
			data[real_pos] &= ~(1 << index);
		}

	}
};

#endif /* PROGRESSIVEBUFFER_H_ */
