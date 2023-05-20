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
	void SetActiveVertices(int three_columns, int not_separate_pzones);

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
//vs2015's array's const operator[] loses the type information if built with _ITERATOR_DEBUG_LEVEL
//return a non const reference to the array in that case, so that it uses the non const operator[]
#if !defined(_MSC_VER) || _MSC_VER != 1900 || !defined(_ITERATOR_DEBUG_LEVEL) || _ITERATOR_DEBUG_LEVEL == 0
#define const_auto const auto
#else
#define const_auto auto
#endif
	const_auto& getSzone() const { return *vActiveSzone; }
	const_auto& getDeck() const { return *vActiveDeck; }
	const_auto& getExtra() const { return *vActiveExtra; }
	const_auto& getGrave() const { return *vActiveGrave; }
	const_auto& getRemove() const { return *vActiveRemove; }
	const_auto& getSkill() const { return *vActiveSkill; }
#undef const_auto
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