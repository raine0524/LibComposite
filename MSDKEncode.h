#ifndef MSDKENCODE_H_
#define MSDKENCODE_H_
#include <map>
#include "MSDKVpp.h"

//#define CONFIG_READ_RAW_BUFFER
#define GROUP_OF_PICTURE    5400

typedef enum {
    MASTER = 0,
    SLAVE
} ENCODE_TYPE;

class MSDKEncode : public MSDKBase
{
public:
    MSDKEncode(MSDKCodecNotify* pNotify, ENCODE_TYPE type);
    virtual ~MSDKEncode();

public:
    //nLogicIndex: 0-high_bitrate 1-mid_bitrate 2-low_bitrate
#ifdef CONFIG_READ_RAW_BUFFER
    bool SetEncodeParam(VADisplay* pVaDpy, unsigned int width, unsigned int height, unsigned short bitrate, int nLogicIndex, Measurement* pMeasuremnt, bool bUseMeasure);
#else
    bool SetEncodeParam(VADisplay* pVaDpy, unsigned short bitrate, int nLogicIndex, Measurement* pMeasuremnt, bool bUseMeasure);
#endif
    void LinkPrevElement(MSDKVpp* pVpp);
    void ForceKeyFrame() { m_bForceKeyFrame = true; }

private:
    mfxStatus InitEncoder(mfxFrameSurface1* pFrameSurface);
    virtual int HandleProcess();
#ifdef CONFIG_READ_RAW_BUFFER
    mfxStatus AllocFrames(mfxFrameAllocRequest* pRequest);
    mfxStatus LoadRawFrame(mfxFrameSurface1* pSurface);
#endif

private:
    ENCODE_TYPE m_type;
    MFXVideoENCODE* m_pEncode;
    mfxBitstream m_outputBs;
    mfxEncodeCtrl m_encCtrl;
    mfxExtCodingOption2 m_codingOpt2;
    std::vector<mfxExtBuffer*> m_encExtParams;

    bool m_bForceKeyFrame;
    int m_nLogicIndex;
    MSDKCodecNotify* m_pNotify;
#ifdef CONFIG_READ_RAW_BUFFER
    MemPool* m_pInputMem;
#else
    std::map<MSDKVpp*, RING_BUFFER*> m_mapRingBuf;
#endif
};
#endif  //MSDKENCODE_H_
