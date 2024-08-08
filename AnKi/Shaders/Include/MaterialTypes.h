// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Shaders/Include/Common.h>

ANKI_BEGIN_NAMESPACE

/// Common data for all materials.
struct MaterialGlobalUniforms
{
	Mat4 m_viewProjectionMatrix;
	Mat4 m_previousViewProjectionMatrix;
	Mat3x4 m_viewTransform;
	Mat3x4 m_cameraTransform;

	Vec4 m_viewport;
};
static_assert(sizeof(MaterialGlobalUniforms) == 15 * sizeof(Vec4));

#define ANKI_MATERIAL_REGISTER_TILINEAR_REPEAT_SAMPLER s0
#define ANKI_MATERIAL_REGISTER_GLOBAL_UNIFORMS b0
#define ANKI_MATERIAL_REGISTER_GPU_SCENE t0

#define ANKI_MATERIAL_REGISTER_MESHLET_BOUNDING_VOLUMES t1 ///< Points to the unified geom buffer
#define ANKI_MATERIAL_REGISTER_MESHLET_GEOMETRY_DESCRIPTORS t2 ///< Points to the unified geom buffer
#define ANKI_MATERIAL_REGISTER_MESHLET_INSTANCES t3
#define ANKI_MATERIAL_REGISTER_RENDERABLES t4
#define ANKI_MATERIAL_REGISTER_MESH_LODS t5
#define ANKI_MATERIAL_REGISTER_PARTICLE_EMITTERS t6
#define ANKI_MATERIAL_REGISTER_TRANSFORMS t7
#define ANKI_MATERIAL_REGISTER_NEAREST_CLAMP_SAMPLER s1
#define ANKI_MATERIAL_REGISTER_FIRST_MESHLET t8

// For FW shading:
#define ANKI_MATERIAL_REGISTER_LINEAR_CLAMP_SAMPLER s2
#define ANKI_MATERIAL_REGISTER_SHADOW_SAMPLER s3
#define ANKI_MATERIAL_REGISTER_SCENE_DEPTH t9
#define ANKI_MATERIAL_REGISTER_LIGHT_VOLUME t10
#define ANKI_MATERIAL_REGISTER_CLUSTER_SHADING_UNIFORMS b1
#define ANKI_MATERIAL_REGISTER_CLUSTER_SHADING_POINT_LIGHTS t11
#define ANKI_MATERIAL_REGISTER_CLUSTER_SHADING_SPOT_LIGHTS t12
#define ANKI_MATERIAL_REGISTER_SHADOW_ATLAS t13
#define ANKI_MATERIAL_REGISTER_CLUSTERS t14

// Always last because it's variable. Texture buffer bindings pointing to unified geom buffer:
#define ANKI_MATERIAL_REGISTER_UNIFIED_GEOMETRY_START t15

ANKI_END_NAMESPACE
