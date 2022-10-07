#ifndef LOGGING_H
#define LOGGING_H

#include "text_types.h"

namespace ygo {

void ErrorLog(epro::stringview msg);

//Explicitly get a T parameter to be sure that the function is called only with 2+ arguments
//to avoid it capturing calls that would fall back to the default stringview implementation
template<std::size_t N, typename T, typename...Arg>
inline void ErrorLog(char const (&format)[N], T&& arg1, Arg&&... args) {
	ErrorLog(fmt::vformat(format, { fmt::make_format_args(std::forward<T>(arg1), std::forward<Arg>(args)...) }));
}

}

#endif
