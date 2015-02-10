#ifndef MSDKVPP_H_
#define MSDKVPP_H_
#include <map>
#include <list>
#include <vector>
#include <float.h>
#include "MSDKDecode.h"

#define MAX_INPUTSTREAM_NUM 4
#define MAX_OUTPUT_NUM		3
#define MAGIC_NUMBER        40*1000 //40ms

#define LOGISTIC_INTERVAL_UPPER 100

typedef struct {
    unsigned x;
    unsigned y;
    unsigned w;
    unsigned h;
} VppRect;

typedef struct {
    mfxU8 comp_num;
    mfxU16 out_width;
    mfxU16 out_height;
} VppConfig;

typedef struct {
    RING_BUFFER* pRingBuf;
    unsigned int nDropFrameNums;
} MediaBuf;

typedef enum {
    VPP_COMP = 0,
    VPP_RESIZE
} VPP_MODE;

class MSDKVppCallback
{
public:
    virtual bool StartTrain() = 0;
    virtual void StopTrain() = 0;
};

class MSDKVpp : public MSDKDecodeVpp
{
public:
    MSDKVpp(VPP_MODE mode, MSDKVppCallback& rCallback);
    virtual ~MSDKVpp();

public:
    bool SetVppParam(VADisplay* pVaDpy, VppConfig* pVppCfg, Measurement* pMeasuremnt, bool bUseMeasure);
    void LinkPrevElement(MSDKDecodeVpp* pCodec);
    void UnlinkPrevElement(MSDKDecodeVpp* pCodec) { m_mapMediaBuf.erase(pCodec); }
    void SetVPPCompRect(int streamIndex, const VppRect* rect);
    void SetVPPCompNum(mfxU8 num) { m_vppCfg.comp_num = num; }
    void TrainSleepInterval(double regre_coeff);

    void AddNextElement(MSDKBase* pCodec) { m_listNextElem.push_front(pCodec); }
    void ReleaseSurface();

private:
    void SetVppCompParam(mfxExtVPPComposite* pVppComp);
    virtual int HandleProcess();
#ifndef CONFIG_USE_MFXALLOCATOR
    mfxStatus AllocFrames(mfxFrameAllocRequest* pRequest);
#endif
    mfxStatus InitVpp(mfxFrameSurface1* pFrameSurface);
    mfxStatus DoingVpp(mfxFrameSurface1* pInSurf, mfxFrameSurface1* pOutSurf);
    int PrepareVppCompFrames();
    double MasterLogisticEquation(double t);
    double SubLogisticEquation(double t);

private:
    VPP_MODE m_mode;
    MFXVideoVPP* m_pVpp;
    mfxExtBuffer* m_pExtBuf[1];
    bool m_bReinit;
    unsigned int m_tCompStc, m_frameRate, m_nSleepInterval, m_nInterFrameSpace;
    double m_argMasterGrowthPotential, m_argSubGrowthPotential, m_argXPoint;
    VppConfig m_vppCfg;
    std::vector<VppRect> m_vRect;
    std::map<MSDKDecodeVpp*, MediaBuf> m_mapMediaBuf;
    std::list<MSDKBase*> m_listNextElem;
    Mutex m_xMsdkInit, m_xMsdkReinit;
    MSDKVppCallback& m_rCallback;
};
#endif  //MSDKVPP_H_
