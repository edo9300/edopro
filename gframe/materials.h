#ifndef MATERIALS_H
#define MATERIALS_H

#include <S3DVertex.h>
#include <SMaterial.h>

namespace ygo {

class Materials {
public:
	Materials();
	void GenArrow(float y);

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
	QuadVertex vFieldDeck[2][2];
	QuadVertex vFieldGrave[2][2][2];
	QuadVertex vFieldExtra[2][2];
	QuadVertex vFieldRemove[2][2][2];
	QuadVertex vFieldMzone[2][7];
	QuadVertex vFieldSzone[2][8][2][2];
	QuadVertex vSkillZone[2][2][2];
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
};

extern Materials matManager;

}

#endif //MATERIALS_H