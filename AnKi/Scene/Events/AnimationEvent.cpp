// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Scene/Events/AnimationEvent.h>
#include <AnKi/Scene/SceneNode.h>
#include <AnKi/Scene/SceneGraph.h>
#include <AnKi/Resource/ResourceManager.h>

namespace anki {

AnimationEvent::AnimationEvent(CString animationFilename, CString channelName, SceneNode* movableSceneNode)
	: Event(0.0, 1.0)
{
	if(!ANKI_EXPECT(movableSceneNode))
	{
		markForDeletion();
		return;
	}

	if(!ANKI_EXPECT(!ResourceManager::getSingleton().loadResource(animationFilename, m_anim)))
	{
		markForDeletion();
		return;
	}

	m_channelIndex = 0;
	for(const AnimationChannel& channel : m_anim->getChannels())
	{
		if(channel.m_name == channelName)
		{
			break;
		}
		++m_channelIndex;
	}

	if(m_channelIndex == m_anim->getChannels().getSize())
	{
		ANKI_SCENE_LOGE("Can't initialize AnimationEvent. Channel not found: %s", channelName.cstr());
		markForDeletion();
		return;
	}

	init(m_anim->getStartingTime(), m_anim->getDuration());
	m_reanimate = true;
	m_associatedNodes.emplaceBack(movableSceneNode);
}

void AnimationEvent::update([[maybe_unused]] Second prevUpdateTime, Second crntTime)
{
	Vec3 pos;
	Quat rot;
	F32 scale = 1.0;
	m_anim->interpolate(m_channelIndex, crntTime, pos, rot, scale);

	Transform trf;
	trf.setOrigin(pos.xyz0());
	// trf.setOrigin(Vec4(0.0f));
	trf.setRotation(Mat3x4(Vec3(0.0f), rot));
	// trf.setRotation(Mat3x4::getIdentity());
	trf.setScale(Vec4(scale, scale, scale, 0.0f));

	m_associatedNodes[0]->setLocalTransform(trf);
}

} // end namespace anki
