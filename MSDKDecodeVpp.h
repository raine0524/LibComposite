#ifndef MSDKDECODEVPP_H_
#define MSDKDECODEVPP_H_

#include "MSDKBase.h"
#include "mutex.h"
#include "mem_pool.h"
#include "ring_buffer.h"

typedef RingBuffer<mfxFrameSurface1*, FRAME_POOL_SIZE+1> RING_BUFFER;

class MSDKDecodeVpp : public MSDKBase
{
public:
    explicit MSDKDecodeVpp(ElementType type);
    virtual ~MSDKDecodeVpp();

public:
    void SetDataEos() { m_bEndOfStream = true; }
    bool GetDataEos() { return m_bEndOfStream; }
    RING_BUFFER* GetOutputRingBuf() { return &m_mfxRingBuf; }

private:
    void ReleaseSurfacePool();

protected:
#ifdef CONFIG_USE_MFXALLOCATOR
    mfxStatus AllocFrames(mfxFrameAllocRequest* pRequest);
#else
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest* pRequest) = 0;
#endif

    bool m_bEndOfStream;
    mfxFrameSurface1** m_pSurfacePool;
    unsigned int m_nSurfaces;
#ifdef CONFIG_USE_MFXALLOCATOR
    mfxFrameAllocResponse m_mfxResponse;    //memory allocation response for decode or vpp
#else
    mfxU8* m_pSurfaceBuffers;
#endif
    RING_BUFFER m_mfxRingBuf;
    Mutex m_mutex;
};
#endif  //MSDKDECODEVPP_H_
