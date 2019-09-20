#include "data_manager.h"
#include "game.h"
#include <stdio.h>
#include <fstream>

namespace ygo {

DataManager dataManager;

bool DataManager::LoadDB(const std::string& file) {
	sqlite3* pDB;
	if(sqlite3_open_v2(file.c_str(), &pDB, SQLITE_OPEN_READONLY, 0) != SQLITE_OK)
		return Error(pDB);
	sqlite3_stmt* pStmt;
	const char* sql = "select * from datas";
	if(sqlite3_prepare_v2(pDB, sql, -1, &pStmt, 0) != SQLITE_OK)
		return Error(pDB);
	int step = 0;
	do {
		step = sqlite3_step(pStmt);
		if(step == SQLITE_BUSY || step == SQLITE_ERROR || step == SQLITE_MISUSE)
			return Error(pDB, pStmt);
		else if(step == SQLITE_ROW) {
			CardDataC* cd = new CardDataC();
			cd->code = sqlite3_column_int(pStmt, 0);
			cd->ot = sqlite3_column_int(pStmt, 1);
			cd->alias = sqlite3_column_int(pStmt, 2);
			cd->setcode = sqlite3_column_int64(pStmt, 3);
			cd->type = sqlite3_column_int(pStmt, 4);
			cd->attack = sqlite3_column_int(pStmt, 5);
			cd->defense = sqlite3_column_int(pStmt, 6);
			if(cd->type & TYPE_LINK) {
				cd->link_marker = cd->defense;
				cd->defense = 0;
			} else
				cd->link_marker = 0;
			int level = sqlite3_column_int(pStmt, 7);
			if(level < 0) {
				cd->level = -(level & 0xff);
			}
			else
				cd->level = level & 0xff;
			cd->lscale = (level >> 24) & 0xff;
			cd->rscale = (level >> 16) & 0xff;
			cd->race = sqlite3_column_int(pStmt, 8);
			cd->attribute = sqlite3_column_int(pStmt, 9);
			cd->category = sqlite3_column_int(pStmt, 10);
			_datas[cd->code] = cd;
		}
	} while(step != SQLITE_DONE);
	sqlite3_finalize(pStmt);
	sqlite3_close(pDB);
	return true;
}
bool DataManager::Error(sqlite3* pDB, sqlite3_stmt* pStmt) {
	auto error = sqlite3_errmsg(pDB);
	if(pStmt)
		sqlite3_finalize(pStmt);
	sqlite3_close(pDB);
	mainGame->AddDebugMsg(error);
	return false;
}
bool DataManager::GetData(int code, CardData* pData) {
	auto cdit = _datas.find(code);
	if(cdit == _datas.end())
		return false;
	if(pData)
		*pData = *((CardData*)cdit->second);
	return true;
}
CardDataC* DataManager::GetCardData(int code) {
	auto it = _datas.find(code);
	if(it != _datas.end())
		return it->second;
	return nullptr;
}
void DataManager::CardReader(void* payload, int code, CardData* data) {
	if(!static_cast<DataManager*>(payload)->GetData(code, (CardData*)data))
		memset(data, 0, sizeof(CardData));
}

}
