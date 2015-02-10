#include "MSDKBase.h"

unsigned int MSDKBase::nDecChannels = 0;
unsigned int MSDKBase::nEncChannels = 0;

MSDKBase::MSDKBase(ElementType type)
:m_threadID(0)
,m_type(type)
,m_bInit(false)
,m_bWantToStop(false)
,m_bAccessNextElem(true)
,m_pSession(NULL)
,m_pMfxAllocator(NULL)
,m_pMeasuremnt(NULL)
{
    memset(&m_mfxVideoParam, 0, sizeof(m_mfxVideoParam));
}

MSDKBase::~MSDKBase()
{
    if (m_pSession)
    {
        m_pSession->Close();
        delete m_pSession;
        m_pSession = NULL;
    }
    if (m_pMfxAllocator)
    {
        delete m_pMfxAllocator;
        m_pMfxAllocator = NULL;
    }
}

bool MSDKBase::Start()
{
    bool bRun = (pthread_create(&m_threadID, NULL, ThreadFunc, this) == 0);
    if (!bRun)
        FRMW_TRACE_ERROR("MSDKBase element %p start failed\n", this);
    return bRun;
}

void* MSDKBase::ThreadFunc(void* arg)
{
    MSDKBase* base = static_cast<MSDKBase*>(arg);
    base->HandleProcess();
    return NULL;
}

void MSDKBase::Stop()
{
    m_bWantToStop = true;
    WaitForStop();
}

MFXVideoSession* MSDKBase::CreateSession(VADisplay va_dpy)
{
    mfxIMPL impl = MFX_IMPL_AUTO_ANY;
    mfxVersion ver = {{3, 1}};
    mfxStatus sts = MFX_ERR_NONE;

    MFXVideoSession* pSession = new MFXVideoSession;
    if (!pSession)
        return NULL;

    sts = pSession->Init(impl, &ver);
    if (MFX_ERR_NONE != sts) {
        delete pSession;
        return NULL;
    }
    sts = pSession->SetHandle((mfxHandleType)MFX_HANDLE_VA_DISPLAY, va_dpy);
    if (MFX_ERR_NONE != sts) {
        delete pSession;
        return NULL;
    }
    return pSession;
}

void MSDKBase::CloseSession(MFXVideoSession* pSession)
{
    if (!pSession) {
        FRMW_TRACE_ERROR("Invalid input session\n");
        return;
    }
    pSession->Close();
    delete pSession;
}

bool MSDKBase::CreateSessionAllocator(VADisplay* pVaDpy)
{
    mfxStatus sts = MFX_ERR_NONE;
    GeneralAllocator* pAllocator = new GeneralAllocator;
    if (!pAllocator) {
        FRMW_TRACE_ERROR("Create allocator failed\n");
        return false;
    }

    sts = pAllocator->Init(pVaDpy);
    if (MFX_ERR_NONE != sts) {
        FRMW_TRACE_ERROR("Init allocator failed\n");
        delete pAllocator;
        return false;
    }

    MFXVideoSession* pSession = CreateSession(*pVaDpy);
    if (!pSession) {
        FRMW_TRACE_ERROR("Init session failed\n");
        delete pAllocator;
        return false;
    }

    sts = pSession->SetFrameAllocator(pAllocator);
    if (MFX_ERR_NONE != sts) {
        FRMW_TRACE_ERROR("Set frame allocator failed\n");
        delete pAllocator;
        CloseSession(pSession);
        return false;
    }

    m_pSession = pSession;
    m_pMfxAllocator = pAllocator;
    FRMW_TRACE_INFO("------Create session and set its initialized MFXFrameAllocator.\n");
    return true;
}

int MSDKBase::GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize)
{
    if (pSurfacesPool) {
        for (mfxU16 i = 0; i < nPoolSize; i++) {
            if (0 == pSurfacesPool[i]->Data.Locked)
                return i;
        }
    }
    return MFX_ERR_NOT_FOUND;
}
