#ifndef PREFIX
#define PREFIX(func) OCG_##func
#endif
/* double macro to expand PREFIX() */
#define X2(ret,name,...) X(ret,name,__VA_ARGS__)
#ifndef ONLY_DUEL_FUNCTIONS
X2(void, PREFIX(GetVersion), int* major, int* minor)
X2(int, PREFIX(CreateDuel), OCG_Duel* duel, const OCG_DuelOptions* options_ptr)
#else
#undef ONLY_DUEL_FUNCTIONS
#endif
X2(void, PREFIX(DestroyDuel), OCG_Duel duel)
X2(void, PREFIX(DuelNewCard), OCG_Duel duel, const OCG_NewCardInfo* info_ptr)
X2(int, PREFIX(StartDuel), OCG_Duel duel)
X2(int, PREFIX(DuelProcess), OCG_Duel duel)
X2(void*, PREFIX(DuelGetMessage), OCG_Duel duel, uint32_t* length)
X2(void, PREFIX(DuelSetResponse), OCG_Duel duel, const void* buffer, uint32_t length)
X2(int, PREFIX(LoadScript), OCG_Duel duel, const char* buffer, uint32_t length, const char* name)
X2(uint32_t, PREFIX(DuelQueryCount), OCG_Duel duel, uint8_t team, uint32_t loc)
X2(void*, PREFIX(DuelQuery), OCG_Duel duel, uint32_t* length, const OCG_QueryInfo* info_ptr)
X2(void*, PREFIX(DuelQueryLocation), OCG_Duel duel, uint32_t* length, const OCG_QueryInfo* info_ptr)
X2(void*, PREFIX(DuelQueryField), OCG_Duel duel, uint32_t* length)

#undef X
#undef X2
#undef PREFIX
