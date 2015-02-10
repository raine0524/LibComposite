#include "MSDKDecodeVpp.h"

MSDKDecodeVpp::MSDKDecodeVpp(ElementType type)
:MSDKBase(type)
,m_bEndOfStream(false)
,m_pSurfacePool(NULL)
,m_nSurfaces(0)
{
#ifdef CONFIG_USE_MFXALLOCATOR
    memset(&m_mfxResponse, 0, sizeof(m_mfxResponse));
#else
    m_pSurfaceBuffers = NULL;
#endif
}

MSDKDecodeVpp::~MSDKDecodeVpp()
{
    ReleaseSurfacePool();
#ifdef CONFIG_USE_MFXALLOCATOR
    if (m_pMfxAllocator)
        m_pMfxAllocator->Free(m_pMfxAllocator->pthis, &m_mfxResponse);
#else
    if (m_pSurfaceBuffers)
    {
        delete[] m_pSurfaceBuffers;
        m_pSurfaceBuffers = NULL;
    }
#endif
}

void MSDKDecodeVpp::ReleaseSurfacePool()
{
    if (m_pSurfacePool)
    {
        for (mfxU16 i = 0; i < m_nSurfaces; i++)
            delete m_pSurfacePool[i];
        delete[] m_pSurfacePool;
        m_pSurfacePool = NULL;
    }
}

// - For simplistic memory management, system memory surfaces are used to store raw frames
//   (Note that when using HW acceleration video surfaces are prefered, for better performance)
#ifdef CONFIG_USE_MFXALLOCATOR
mfxStatus MSDKDecodeVpp::AllocFrames(mfxFrameAllocRequest* pRequest)
{
    mfxStatus sts = MFX_ERR_NONE;
    pRequest->NumFrameMin = m_nSurfaces;
    pRequest->NumFrameSuggested = m_nSurfaces;
    printf("The number of surface suggested is %d\n", m_nSurfaces);

    sts = m_pMfxAllocator->Alloc(m_pMfxAllocator->pthis, pRequest, &m_mfxResponse);
    if (sts != MFX_ERR_NONE) {
        VPP_TRACE_ERROR("[MSDKDecodeVpp]-----AllocFrame failed %d\n", sts);
        return sts;
    }

    ReleaseSurfacePool();
    m_pSurfacePool = new mfxFrameSurface1*[m_nSurfaces];
    for (mfxU16 i = 0; i < m_nSurfaces; i++) {
        m_pSurfacePool[i] = new mfxFrameSurface1;
        memset(m_pSurfacePool[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_pSurfacePool[i]->Info), &(pRequest->Info), sizeof(mfxFrameInfo));
        m_pSurfacePool[i]->Data.MemId = m_mfxResponse.mids[i];
    }
    return MFX_ERR_NONE;
}
#endif
