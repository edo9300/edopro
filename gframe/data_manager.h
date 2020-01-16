#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include "config.h"
#include "client_card.h"
#include <unordered_map>

struct sqlite3;
struct sqlite3_stmt;

namespace ygo {

class DataManager {
public:
	DataManager() {}
	~DataManager() {
		for(auto& card : _datas) {
			if(card.second) {
				if(card.second->setcodes_p)
					delete card.second->setcodes_p;
				delete card.second;
			}
		}
	}
	bool LoadDB(const std::string& file);
	bool Error(sqlite3* pDB, sqlite3_stmt* pStmt = 0);
	bool GetData(int code, CardData* pData);
	CardDataC* GetCardData(int code);

	std::unordered_map<unsigned int, CardDataC*> _datas;

	static void CardReader(void* payload, int code, CardData* data);

};

extern DataManager dataManager;

}

#endif // DATAMANAGER_H
