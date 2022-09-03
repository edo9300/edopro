#ifndef MATERIALS_H
#define MATERIALS_H

#include <S3DVertex.h>
#include <SMaterial.h>
#include <array>

namespace ygo {

class Materials {
public:
	Materials();
	void GenArrow(float y);
	void SetActiveVertices(int speed, int field);

	using QuadVertex = irr::video::S3DVertex[4];
	
	QuadVertex vCardFront;
	QuadVertex vCardOutline;
	QuadVertex vCardOutliner;
	QuadVertex vCardBack;
	QuadVertex vSymbol;
	QuadVertex vNegate;
	QuadVertex vChainNum;
	QuadVertex vActivate;
	QuadVertex vField;
	QuadVertex vFieldSpell[2];
	QuadVertex vFieldSpell1[2];
	QuadVertex vFieldSpell2[2];
	//irr::video::S3DVertex vBackLine[76];
	QuadVertex vFieldMzone[2][7];
	const auto& getSzone() const { return *vActiveSzone; }
	const auto& getDeck() const { return *vActiveDeck; }
	const auto& getExtra() const { return *vActiveExtra; }
	const auto& getGrave() const { return *vActiveGrave; }
	const auto& getRemove() const { return *vActiveRemove; }
	const auto& getSkill() const { return *vActiveSkill; }
	irr::core::vector3df vFieldContiAct[2][4];
	irr::video::S3DVertex vArrow[40];
	irr::video::SColor c2d[4];
	irr::u16 iRectangle[6];
	//irr::u16 iBackLine[116];
	irr::u16 iArrow[40];
	irr::video::SMaterial mCard;
	irr::video::SMaterial mTexture;
	irr::video::SMaterial mBackLine;
	irr::video::SMaterial mOutLine;
	irr::video::SMaterial mSelField;
	irr::video::SMaterial mLinkedField;
	irr::video::SMaterial mMutualLinkedField;
	irr::video::SMaterial mTRTexture;
	irr::video::SMaterial mATK;
private:
	std::array<std::array<std::array<std::array<QuadVertex, 8>, 2>, 2>, 2> vFieldSzone;
	std::array<std::array<QuadVertex, 8>, 2>* vActiveSzone;
	std::array<QuadVertex, 2> vFieldDeck[2];
	std::array<QuadVertex, 2>* vActiveDeck;
	std::array<QuadVertex, 2> vFieldExtra[2];
	std::array<QuadVertex, 2>* vActiveExtra;
	std::array<std::array<std::array<QuadVertex, 2>, 2>, 2> vFieldGrave;
	std::array<QuadVertex, 2>* vActiveGrave;
	std::array<std::array<std::array<QuadVertex, 2>, 2>, 2> vFieldRemove;
	std::array<QuadVertex, 2>* vActiveRemove;
	std::array<std::array<std::array<QuadVertex, 2>, 2>, 2> vSkillZone;
	std::array<QuadVertex, 2>* vActiveSkill;
};

extern Materials matManager;

}

#endif //MATERIALS_H