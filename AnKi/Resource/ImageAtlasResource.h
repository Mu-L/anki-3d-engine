// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Resource/ResourceObject.h>
#include <AnKi/Resource/ImageResource.h>
#include <AnKi/Gr.h>

namespace anki {

/// @addtogroup resource
/// @{

/// Image atlas resource class.
///
/// XML format:
/// @code
/// <imageAtlas>
/// 	<image>path/to/tex.ankitex</image>
/// 	<subImageMargin>N</subImageMargin>
/// 	<subImages>
/// 		<subImage>
/// 			<name>name</name>
/// 			<uv>0.1 0.2 0.5 0.6</uv>
/// 		</subImage>
/// 		<subImage>...</subImage>
/// 		...
/// 	</subImages>
/// </imageAtlas>
/// @endcode
class ImageAtlasResource : public ResourceObject
{
public:
	ImageAtlasResource(CString fname, U32 uuid)
		: ResourceObject(fname, uuid)
	{
	}

	~ImageAtlasResource() = default;

	/// Load the atlas.
	Error load(const ResourceFilename& filename, Bool async);

	const Texture& getTexture() const
	{
		return m_image->getTexture();
	}

	U32 getWidth() const
	{
		return m_size[0];
	}

	U32 getHeight() const
	{
		return m_size[1];
	}

	U32 getSubImageMargin() const
	{
		return m_margin;
	}

	/// Get the UV coordinates of a sub image.
	Error getSubImageInfo(CString name, F32 uv[4]) const;

private:
	class SubTex
	{
	public:
		CString m_name; ///< Points to ImageAtlas::m_subTexNames.
		Array<F32, 4> m_uv;
	};

	ImageResourcePtr m_image;
	ResourceDynamicArray<char> m_subTexNames;
	ResourceDynamicArray<SubTex> m_subTexes;
	Array<U32, 2> m_size;
	U32 m_margin = 0;
};
/// @}

} // end namespace anki
