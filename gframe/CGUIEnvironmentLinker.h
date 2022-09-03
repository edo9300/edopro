#ifndef C_GUI_ENVIRONMENT_LINKER_H
#define C_GUI_ENVIRONMENT_LINKER_H

#include <IGUIElement.h>
#include <IGUIEnvironment.h>
#include <vector2d.h>

namespace irr {
namespace gui {
template<typename T>
class EnvironmentLinker : public IGUIElement {
private:
	EnvironmentLinker(IGUIEnvironment* env, T* toDelete) :
		IGUIElement(EGUIET_ELEMENT, env, env->getRootGUIElement(), -1, { }), m_toDelete(toDelete) {
		IsVisible = false;
	};
	T* m_toDelete;
public:
	virtual ~EnvironmentLinker() override {
		delete m_toDelete;
	}
	virtual bool isPointInside(const core::vector2d<s32>& point) const override { return false; };
	static void tie(IGUIEnvironment* env, T* toDelete) {
		(new EnvironmentLinker(env, toDelete))->drop();
	}
};
}
}

#endif