#ifndef CONFIG_H
#define CONFIG_H

#include "compiler_features.h"
#include "ocgapi_types.h"
#include "text_types.h"

extern uint16_t PRO_VERSION;
extern bool is_from_discord;
extern bool open_file;
extern epro::path_string open_file_name;
extern bool show_changelog;

#define EDOPRO_VERSION_MAJOR 41
#define EDOPRO_VERSION_MINOR 0
#define EDOPRO_VERSION_PATCH 0
#define EDOPRO_VERSION_CODENAME "Bagooska"
#define EDOPRO_VERSION_STRING_DEBUG "EDOPro version " STR(EDOPRO_VERSION_MAJOR) "." STR(EDOPRO_VERSION_MINOR) "." STR(EDOPRO_VERSION_PATCH)
#define EDOPRO_VERSION_STRING L"Project Ignis: EDOPro | " STR(EDOPRO_VERSION_MAJOR) "." STR(EDOPRO_VERSION_MINOR) "." STR(EDOPRO_VERSION_PATCH) " \"" EDOPRO_VERSION_CODENAME "\""
#define CLIENT_VERSION ((EDOPRO_VERSION_MAJOR & 0xff) | ((EDOPRO_VERSION_MINOR & 0xff) << 8) | ((OCG_VERSION_MAJOR & 0xff) << 16) | ((OCG_VERSION_MINOR & 0xff) << 24))
#define GET_CLIENT_VERSION_MAJOR(ver) (ver & 0xff)
#define GET_CLIENT_VERSION_MINOR(ver) ((ver >> 8) & 0xff)
#define GET_CORE_VERSION_MAJOR(ver) ((ver >> 16) & 0xff)
#define GET_CORE_VERSION_MINOR(ver) ((ver >> 24) & 0xff)
#define EXPAND_VERSION(ver) GET_CLIENT_VERSION_MAJOR(ver), GET_CLIENT_VERSION_MINOR(ver), GET_CORE_VERSION_MAJOR(ver), GET_CORE_VERSION_MINOR(ver)

#endif
