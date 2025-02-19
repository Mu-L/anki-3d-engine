// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Gr/PipelineQuery.h>
#include <AnKi/Gr/Vulkan/VkQueryFactory.h>

namespace anki {

/// @addtogroup vulkan
/// @{

/// Pipeline query.
class PipelineQueryImpl final : public PipelineQuery
{
public:
	MicroQuery m_handle = {};

	PipelineQueryType m_type = PipelineQueryType::kCount;

	PipelineQueryImpl(CString name)
		: PipelineQuery(name)
	{
	}

	~PipelineQueryImpl();

	/// Create the query.
	Error init(PipelineQueryType type);
};
/// @}

} // end namespace anki
