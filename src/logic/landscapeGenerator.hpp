#pragma once

#include <grend/gameObject.hpp>
#include <grend/animation.hpp>
#include <grend/ecs/ecs.hpp>
#include <grend/ecs/collision.hpp>
#include <thread>

#include "generatorEvent.hpp"

using namespace grendx;
using namespace grendx::ecs;

class landscapeGenerator : public worldGenerator {
	public:
		landscapeGenerator(unsigned seed = 0xcafebabe);
		virtual ~landscapeGenerator(void);
		virtual void setPosition(gameMain *game, glm::vec3 position);

	private:
		void generateLandscape(gameMain *game, glm::vec3 curpos, glm::vec3 lastpos);
		std::future<bool> genjob;
		gameObject::ptr returnValue;
};

// XXX: global variable, TODO: something else
//      (could just put it in the landscapeGenerator contructor)
inline material::ptr landscapeMaterial;
inline gameObject::ptr landscapeNodes = std::make_shared<gameObject>();
inline gameModel::ptr treeNode;
