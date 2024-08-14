// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Renderer/Common.h>
#include <AnKi/Util/StdTypes.h>
#include <AnKi/Gr.h>
#include <AnKi/Resource/ResourceManager.h>
#include <AnKi/Resource/ShaderProgramResource.h>
#include <AnKi/Core/GpuMemory/RebarTransientMemoryPool.h>

namespace anki {

// Forward
class Renderer;
class ResourceManager;

/// @addtogroup renderer
/// @{

/// Renderer object.
class RendererObject
{
public:
	RendererObject() = default;

	virtual ~RendererObject() = default;

	virtual void getDebugRenderTarget([[maybe_unused]] CString rtName, [[maybe_unused]] Array<RenderTargetHandle, kMaxDebugRenderTargets>& handles,
									  [[maybe_unused]] ShaderProgramPtr& optionalShaderProgram) const
	{
		ANKI_ASSERT(!"Object doesn't support that");
	}

protected:
	static ANKI_PURE Renderer& getRenderer();

	/// Used in fullscreen quad draws.
	static void drawQuad(CommandBuffer& cmdb)
	{
		cmdb.draw(PrimitiveTopology::kTriangles, 3, 1);
	}

	/// Dispatch a compute job equivelent to drawQuad
	static void dispatchPPCompute(CommandBuffer& cmdb, U32 workgroupSizeX, U32 workgroupSizeY, U32 outImageWidth, U32 outImageHeight)
	{
		const U32 sizeX = (outImageWidth + workgroupSizeX - 1) / workgroupSizeX;
		const U32 sizeY = (outImageHeight + workgroupSizeY - 1) / workgroupSizeY;
		cmdb.dispatchCompute(sizeX, sizeY, 1);
	}

	static void dispatchPPCompute(CommandBuffer& cmdb, U32 workgroupSizeX, U32 workgroupSizeY, U32 workgroupSizeZ, U32 outImageWidth,
								  U32 outImageHeight, U32 outImageDepth)
	{
		const U32 sizeX = (outImageWidth + workgroupSizeX - 1) / workgroupSizeX;
		const U32 sizeY = (outImageHeight + workgroupSizeY - 1) / workgroupSizeY;
		const U32 sizeZ = (outImageDepth + workgroupSizeZ - 1) / workgroupSizeZ;
		cmdb.dispatchCompute(sizeX, sizeY, sizeZ);
	}

	template<typename T>
	static T* allocateAndBindConstants(CommandBuffer& cmdb, U32 reg, U32 space)
	{
		T* ptr;
		const RebarAllocation alloc = RebarTransientMemoryPool::getSingleton().allocateFrame(1, ptr);
		ANKI_ASSERT(isAligned(alignof(T), ptrToNumber(ptr)));
		cmdb.bindConstantBuffer(reg, space, alloc);
		return ptr;
	}

	template<typename T>
	static T* allocateAndBindSrvStructuredBuffer(CommandBuffer& cmdb, U32 reg, U32 space, U32 count = 1)
	{
		T* ptr;
		const RebarAllocation alloc = RebarTransientMemoryPool::getSingleton().allocateFrame(count, ptr);
		ANKI_ASSERT(isAligned(alignof(T), ptrToNumber(ptr)));
		cmdb.bindSrv(reg, space, alloc);
		return ptr;
	}

	template<typename T>
	static T* allocateAndBindUavStructuredBuffer(CommandBuffer& cmdb, U32 reg, U32 space, U32 count = 1)
	{
		T* ptr;
		const RebarAllocation alloc = RebarTransientMemoryPool::getSingleton().allocateFrame(count, ptr);
		ANKI_ASSERT(isAligned(alignof(T), ptrToNumber(ptr)));
		cmdb.bindUav(reg, space, alloc);
		return ptr;
	}

	void registerDebugRenderTarget(CString rtName);

	static Error loadShaderProgram(CString filename, ShaderProgramResourcePtr& rsrc, ShaderProgramPtr& grProg)
	{
		ANKI_CHECK(loadShaderProgram(filename, {}, rsrc, grProg));
		return Error::kNone;
	}

	class SubMutation
	{
	public:
		CString m_mutatorName;
		MutatorValue m_value;
	};

	static Error loadShaderProgram(CString filename, std::initializer_list<SubMutation> mutators, ShaderProgramResourcePtr& rsrc,
								   ShaderProgramPtr& grProg, CString technique = {}, ShaderTypeBit shaderTypes = ShaderTypeBit::kNone);

	static void zeroBuffer(Buffer* buff);

	/// Temp pass name.
	static CString generateTempPassName(const Char* fmt, ...);
};
/// @}

} // end namespace anki
