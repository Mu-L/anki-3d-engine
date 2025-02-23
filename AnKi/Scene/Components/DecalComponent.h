// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Scene/Components/SceneComponent.h>
#include <AnKi/Scene/GpuSceneArray.h>
#include <AnKi/Resource/ImageAtlasResource.h>
#include <AnKi/Collision/Obb.h>

namespace anki {

/// @addtogroup scene
/// @{

/// Decal component. Contains all the relevant info for a deferred decal.
class DecalComponent : public SceneComponent
{
	ANKI_SCENE_COMPONENT(DecalComponent)

public:
	static constexpr U32 kAtlasSubImageMargin = 16;

	DecalComponent(SceneNode* node);

	~DecalComponent();

	Bool isEnabled() const
	{
		return m_layers[LayerType::kDiffuse].m_bindlessTextureIndex != kMaxU32
			   || m_layers[LayerType::kRoughnessMetalness].m_bindlessTextureIndex != kMaxU32;
	}

	void loadDiffuseImageResource(CString fname, F32 blendFactor)
	{
		setLayer(fname, blendFactor, LayerType::kDiffuse);
	}

	void loadMetalRoughnessImageResource(CString fname, F32 blendFactor)
	{
		setLayer(fname, blendFactor, LayerType::kRoughnessMetalness);
	}

private:
	enum class LayerType : U8
	{
		kDiffuse,
		kRoughnessMetalness,
		kCount
	};

	class Layer
	{
	public:
		ImageResourcePtr m_image;
		F32 m_blendFactor = 1.0f;
		U32 m_bindlessTextureIndex = kMaxU32;
	};

	Array<Layer, U(LayerType::kCount)> m_layers;

	GpuSceneArrays::Decal::Allocation m_gpuSceneDecal;

	Bool m_dirty = true;

	void setLayer(CString fname, F32 blendFactor, LayerType type);

	Error update(SceneComponentUpdateInfo& info, Bool& updated) override;
};
/// @}

} // end namespace anki
