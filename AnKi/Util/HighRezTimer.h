// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Util/Assert.h>
#include <AnKi/Util/StdTypes.h>

namespace anki {

/// @addtogroup util_time
/// @{

/// High resolution timer. All time in seconds
class HighRezTimer
{
public:
	/// Start the timer
	void start()
	{
		m_startTime = getCurrentTime();
		m_stopTime = 0.0;
	}

	/// Stop the timer
	void stop()
	{
		ANKI_ASSERT(m_startTime != 0.0);
		ANKI_ASSERT(m_stopTime == 0.0);
		m_stopTime = getCurrentTime();
	}

	/// Get the time elapsed between start and stop (if its stopped) or between start and the current time.
	Second getElapsedTime() const
	{
		return (m_stopTime == 0.0) ? getCurrentTime() - m_startTime : m_stopTime - m_startTime;
	}

	/// Get the current date's seconds
	static Second getCurrentTime();

	/// Get the current date's mili seconds
	static U64 getCurrentTimeMs();

	/// Get the current date's micro seconds
	static U64 getCurrentTimeUs();

	/// Micro sleep. The resolution is in nanoseconds.
	static void sleep(Second seconds);

private:
	Second m_startTime = 0.0;
	Second m_stopTime = 0.0;
};
/// @}

} // end namespace anki
