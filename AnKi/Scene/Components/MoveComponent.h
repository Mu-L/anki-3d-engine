// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Scene/Components/SceneComponent.h>

namespace anki {

/// @addtogroup scene
/// @{

/// A simple implicit component that updates the SceneNode's transform.
class MoveComponent : public SceneComponent
{
	ANKI_SCENE_COMPONENT(MoveComponent)

public:
	MoveComponent(SceneNode* node)
		: SceneComponent(node, kClassType)
	{
	}

	~MoveComponent() = default;

private:
	void update(SceneComponentUpdateInfo& info, Bool& updated) override;
};
/// @}

} // end namespace anki
