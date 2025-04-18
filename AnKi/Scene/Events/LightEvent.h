// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Scene/Events/Event.h>
#include <AnKi/Math.h>

namespace anki {

/// @addtogroup scene
/// @{

/// An event for light animation
class LightEvent : public Event
{
public:
	LightEvent(Second startTime, Second duration, SceneNode* light);

	/// Implements Event::update
	void update(Second prevUpdateTime, Second crntTime) override;

	void setRadiusMultiplier(F32 v)
	{
		m_radiusMultiplier = v;
	}

	void setIntensityMultiplier(const Vec4& v)
	{
		m_intensityMultiplier = v;
	}

	/// Set the frequency of changes.
	/// @param freq The higher it is the faster things happen.
	/// @param deviation Add a randomization to the frequency.
	void setFrequency(F32 freq, F32 deviation)
	{
		if(ANKI_EXPECT(freq > 0.0))
		{
			m_freq = freq;
		}

		if(ANKI_EXPECT(freq > deviation))
		{
			m_freqDeviation = deviation;
		}
	}

private:
	F32 m_freq = 1.0;
	F32 m_freqDeviation = 0.0;
	F32 m_radiusMultiplier = 0.0;
	Vec4 m_intensityMultiplier = Vec4(0.0);

	F32 m_originalRadius;
	Vec4 m_originalDiffColor;
};
/// @}

} // end namespace anki
