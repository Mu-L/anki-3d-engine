// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Gr/D3D/D3DFence.h>
#include <AnKi/Gr/D3D/D3DGrManager.h>

namespace anki {

MicroFence::MicroFence()
{
	ANKI_D3D_CHECKF(getDevice().CreateFence(m_value.getNonAtomically(), D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	m_event = CreateEvent(nullptr, false, false, nullptr);
	if(!m_event)
	{
		ANKI_D3D_LOGF("CreateEvent() failed");
	}
}

void MicroFence::gpuSignal(GpuQueueType queue)
{
	ANKI_D3D_CHECKF(getGrManagerImpl().getCommandQueue(queue).Signal(m_fence, m_value.fetchAdd(1) + 1));
}

void MicroFence::gpuWait(GpuQueueType queue)
{
	ANKI_D3D_CHECKF(getGrManagerImpl().getCommandQueue(queue).Wait(m_fence, m_value.load()));
}

Bool MicroFence::clientWait(Second seconds)
{
	Bool signaled = false;

	if(seconds == 0.0)
	{
		signaled = done();
	}
	else
	{
		seconds = min<Second>(seconds, g_gpuTimeoutCVar);

		ANKI_D3D_CHECKF(m_fence->SetEventOnCompletion(m_value.load(), m_event));

		const DWORD msec = DWORD(seconds * 1000.0);
		const DWORD result = WaitForSingleObjectEx(m_event, msec, FALSE);

		if(result == WAIT_OBJECT_0)
		{
			signaled = true;
		}
		else if(result == WAIT_TIMEOUT)
		{
			signaled = false;
		}
		else
		{
			ANKI_D3D_LOGF("WaitForSingleObjectEx() returned %u", result);
		}
	}

	return signaled;
}

void MicroFencePtrDeleter::operator()(MicroFence* f)
{
	FenceFactory::getSingleton().deleteFence(f);
}

FenceFactory::~FenceFactory()
{
	trimSignaledFences(true);

	ANKI_ASSERT(m_fences.getSize() == 0);
	ANKI_ASSERT(m_aliveFenceCount == 0);
}

MicroFence* FenceFactory::newFence()
{
	MicroFence* out = nullptr;

	{
		LockGuard<Mutex> lock(m_mtx);

		for(U32 i = 0; i < m_fences.getSize(); ++i)
		{
			const Bool signaled = m_fences[i]->done();
			if(signaled)
			{
				out = m_fences[i];

				// Pop it
				m_fences.erase(m_fences.getBegin() + i);
				break;
			}
		}

		if(out)
		{
			// Recycle
		}
		else
		{
			// Create new

			++m_aliveFenceCount;

			if(m_aliveFenceCount > kMaxAliveFences)
			{
				ANKI_D3D_LOGW("Too many alive fences (%u). You may run out of file descriptors", m_aliveFenceCount);
			}
		}
	}

	if(out == nullptr)
	{
		// Create a new one
		out = anki::newInstance<MicroFence>(GrMemoryPool::getSingleton());
	}
	else
	{
		// Recycle, do nothing
	}

	ANKI_ASSERT(out->m_refcount.getNonAtomically() == 0);
	return out;
}

void FenceFactory::trimSignaledFences(Bool wait)
{
	LockGuard<Mutex> lock(m_mtx);

	GrDynamicArray<MicroFence*> unsignaledFences;
	for(MicroFence* fence : m_fences)
	{
		const Bool signaled = fence->clientWait((wait) ? g_gpuTimeoutCVar : 0.0);
		if(signaled)
		{
			deleteInstance(GrMemoryPool::getSingleton(), fence);

			ANKI_ASSERT(m_aliveFenceCount > 0);
			--m_aliveFenceCount;
		}
		else
		{
			unsignaledFences.emplaceBack(fence);
		}
	}

	m_fences.destroy();
	if(unsignaledFences.getSize())
	{
		m_fences = std::move(unsignaledFences);
	}
}

void FenceFactory::deleteFence(MicroFence* fence)
{
	ANKI_ASSERT(fence);

	LockGuard<Mutex> lock(m_mtx);
	m_fences.emplaceBack(fence);
}

Fence* Fence::newInstance()
{
	return anki::newInstance<FenceImpl>(GrMemoryPool::getSingleton(), "N/A");
}

Bool Fence::clientWait(Second seconds)
{
	ANKI_D3D_SELF(FenceImpl);
	return self.m_fence->clientWait(seconds);
}

} // end namespace anki
