#ifndef MSDKBASE_H_
#define MSDKBASE_H_

// NOTE: Don't get rid of this macro, otherwise it will lead to some unknown
// strange question, it is just used to choose the MODE of MEMORY
// Now the library is encapsulated over and fixed to use the VIDEO_MEMORY
// Any question, please contact Ricci, in HuaPing, at 2015/1/8
#ifndef CONFIG_USE_MFXALLOCATOR
    #define CONFIG_USE_MFXALLOCATOR
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <mfxvideo++.h>
#include "measurement.h"
#include "general_allocator.h"

#define FRAME_POOL_SIZE 16
#define MSDK_ALIGN16(value)     (((value+15) >> 4) << 4)
#define MSDK_ALIGN32(value)     (((mfxU32)((value)+31)) & (~(mfxU32)31))

typedef enum {
    ELEMENT_DECODER = 0,
    ELEMENT_ENCODER,
    ELEMENT_VPP
} ElementType;

class MSDKCodecNotify {
public:
    virtual void OnGetMSDKCodecData(unsigned char* pData, int nLen, bool bKeyFrame, int nIndex) = 0;
};

class MSDKBase
{
public:
    explicit MSDKBase(ElementType type);
    virtual ~MSDKBase();

public:
    bool Start();
    void Stop();
    void WaitForStop() { pthread_join(m_threadID, NULL); }
    void NotifyCanAccess() { m_bAccessNextElem = true; }

    MFXVideoSession* GetMSDKSession() { return m_pSession; }
    static MFXVideoSession* CreateSession(VADisplay va_dpy);
    static void CloseSession(MFXVideoSession* pSession);

private:
    static void* ThreadFunc(void* arg);
    pthread_t m_threadID;

protected:
    virtual int HandleProcess() = 0;
    size_t GeneRandom(size_t left, size_t right) const  //generate random[left, right]
    { return (rand()%(right-left+1))+left; }
    bool CreateSessionAllocator(VADisplay* pVaDpy);
    int GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize);

    ElementType m_type;
    bool m_bInit, m_bWantToStop, m_bAccessNextElem;
    MFXVideoSession* m_pSession;
    MFXFrameAllocator* m_pMfxAllocator;
    mfxVideoParam m_mfxVideoParam;
    Measurement* m_pMeasuremnt;

    static unsigned int nDecChannels;
    static unsigned int nEncChannels;
};
#endif  //MSDKBASE_H_
