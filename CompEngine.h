#ifndef COMPENGINE_H_
#define COMPENGINE_H_
#include "MSDKDecode.h"
#include "MSDKVpp.h"
#include "MSDKEncode.h"
#include "CompTrainer.h"

typedef enum {
    STOP_ATONCE = 0,
    STOP_DELAY
} STOP_MODE;

/**
 * \brief CompEngine class.
 * \details Manages decoder, vpp and encoder objects.
 */
class CompEngine
:public MSDKVppCallback
,public MeasureCallback
{
public:
    CompEngine(MSDKCodecNotify* pNotify);
    ~CompEngine();

public:
    bool Init(mfxU8 uCompNum, mfxU16 width, mfxU16 height, mfxU16 bitrate, int nLogicIndex, bool bUseMeasure = true);
    //Modify task interfaces
    bool AttachDecoder(mfxU16 nAddCnt);
    bool DetachDecoder(mfxU16 nReduCnt);
    bool AttachVppEncoder(mfxU16 width, mfxU16 height, mfxU16 bitrate, int nLogicIndex);
    void SetSingleRect(unsigned int streamIndex, VppRect* rect) { m_pMainVpp->SetVPPCompRect(streamIndex, rect); }

    bool Start();
    void FeedData(unsigned char* pData, int nLen, int streamIndex);
    unsigned long Stop(STOP_MODE mode, bool bShowVppInfo = false);
    void SetDataEos(unsigned int streamIndex);
    void ForceKeyFrame(unsigned int nLogicIndex);

    void SetMemPoolName(unsigned int streamIndex, const char* name);

    ///callback function
    bool StartTrain();
    void StopTrain();

    const char* GetMemPoolName(unsigned int streamIndex);
    int GetMemDataSize(unsigned int streamIndex);

private:
    static VADisplay CreateVAEnvDRM();
    static void DestroyVAEnvDRM();

    static VADisplay m_vaDpy;
    static int m_fdDri;
    static int m_nEngineCnt;

    MSDKCodecNotify* m_pNotify;
    bool m_bInit, m_bRunning, m_bUseMeasure;
    MFXVideoSession* m_pMainSession;
    MSDKVpp* m_pMainVpp;
    MSDKEncode m_mainEncoder;
    std::vector<MemPool*> m_vMemPool;
    std::list<MSDKDecode*> m_listDecoder;
    std::list<MSDKVpp*> m_listVpp;
    std::list<MSDKEncode*> m_listEncoder;
    CompTrainer* m_pCompTrainer;
    Measurement m_measuremnt;

private:
    CompEngine(const CompEngine&);
    CompEngine& operator=(const CompEngine&);
};

#endif  //COMPENGINE_H_
