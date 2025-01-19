#ifndef PORTING_PSVITA_H
#define PORTING_PSVITA_H

#include <vector>
#include "../text_types.h"
#include "../address.h"

namespace porting {

std::vector<epro::Address> getLocalIP();

void setupSqlite3();

void print(epro::stringview string);

}

#endif //PORTING_PSVITA_H
