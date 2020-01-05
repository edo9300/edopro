#include "windbot_panel.h"
#include <fmt/format.h>
#include "data_manager.h"

namespace ygo {

void WindBotPanel::Refresh(int filterMasterRule) {
	deckBox->clear();
	for (auto& bot : bots) {
		switch (filterMasterRule) {
		case 0:
			deckBox->addItem(bot.name.c_str());
			break;
		case 3:
			if (bot.SupportsMasterRule3())
				deckBox->addItem(bot.name.c_str());
			break;
		case 4:
			if (bot.SupportsMasterRule4())
				deckBox->addItem(bot.name.c_str());
			break;
		default:
			break;
		}
	}
	UpdateDescription();
}

void WindBotPanel::UpdateDescription() {
	int sel = deckBox->getSelected();
	if (sel < 0 || sel >= bots.size()) {
		deckProperties->setText(L"");
		return;
	}
	auto& bot = bots[sel];
	int level = bot.GetDifficulty();
	// TODO: Consider using a string buffer here instead and account for i18n
	std::wstring params;
	if (level > 0)
		params.append(fmt::format(dataManager.GetSysString(2055), level));
	else if (level == 0)
		params.append(dataManager.GetSysString(2056));
	params.append(L"\n");
	std::wstring mr;
	if (bot.SupportsMasterRule3())
		mr.append(L"3");
	if (bot.SupportsMasterRule4()) {
		if (mr.size())
			mr.append(L",");
		mr.append(L"4");
	}
	if (mr.size()) {
		params.append(fmt::format(dataManager.GetSysString(2057), mr.c_str()));
		params.append(L"\n");
	}
	deckProperties->setText(params.c_str());
}

bool WindBotPanel::LaunchSelected(int port) {
	int sel = deckBox->getSelected();
	if (sel < 0 || sel >= bots.size()) return false;
	// 1 = scissors, 2 = rock, 3 = paper
	return bots[sel].Launch(port, !chkMute->isChecked(), chkThrowRock->isChecked() * 2);
}

}