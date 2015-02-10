#include "CompEngine.h"
#include <sys/time.h>
#include <sys/resource.h>

VADisplay CompEngine::m_vaDpy = NULL;
int CompEngine::m_fdDri = -1;
int CompEngine::m_nEngineCnt = 0;

CompEngine::CompEngine(MSDKCodecNotify* pNotify)
:m_pNotify(pNotify)
,m_bInit(false)
,m_bRunning(false)
,m_pMainSession(NULL)
,m_pMainVpp(NULL)
,m_mainEncoder(pNotify, MASTER)
,m_pCompTrainer(NULL)
,m_measuremnt(*this)
{
    m_nEngineCnt++;
}

CompEngine::~CompEngine()
{
    for (std::vector<MemPool*>::iterator it = m_vMemPool.begin();
         it != m_vMemPool.end(); ++it)
        delete (*it);
    m_vMemPool.clear();

    for (std::list<MSDKDecode*>::iterator it = m_listDecoder.begin();
         it != m_listDecoder.end(); ++it)
        delete (*it);
    m_listDecoder.clear();

    if (m_pMainVpp)
    {
        delete m_pMainVpp;
        m_pMainVpp = NULL;
    }

    for (std::list<MSDKVpp*>::iterator it = m_listVpp.begin();
         it != m_listVpp.end(); ++it)
        delete (*it);
    m_listVpp.clear();

    for (std::list<MSDKEncode*>::iterator it = m_listEncoder.begin();
         it != m_listEncoder.end(); ++it)
        delete (*it);
    m_listEncoder.clear();

    if (m_pMainSession)
    {
        m_pMainSession->DisjoinSession();
        MSDKBase::CloseSession(m_pMainSession);
        m_pMainSession = NULL;
    }

    if (m_pCompTrainer)
    {
        delete m_pCompTrainer;
        m_pCompTrainer = NULL;
    }

    m_nEngineCnt--;
    if (!m_nEngineCnt && m_vaDpy)
        CompEngine::DestroyVAEnvDRM();
    m_bInit = false;
    FRMW_TRACE_INFO("Clean up all resources successfully\n");
}

VADisplay CompEngine::CreateVAEnvDRM()
{
    int major_version = 0, minor_version = 0;
    VAStatus va_res = VA_STATUS_SUCCESS;

    if (NULL == CompEngine::m_vaDpy) {
        m_fdDri = open("/dev/dri/card0", O_RDWR);
        if (-1 == m_fdDri) {
            FRMW_TRACE_ERROR("Open dri failed!\n");
            exit(EXIT_FAILURE);
        }

        m_vaDpy = vaGetDisplayDRM(m_fdDri);
        va_res = vaInitialize(m_vaDpy, &major_version, &minor_version);
        if (VA_STATUS_SUCCESS != va_res) {
            FRMW_TRACE_ERROR("vaInitialize failed\n");
            close(m_fdDri);
            m_fdDri = -1;
            exit(EXIT_FAILURE);
        }
    }
    return m_vaDpy;
}

void CompEngine::DestroyVAEnvDRM()
{
    if (m_vaDpy) {
        vaTerminate(m_vaDpy);
        m_vaDpy = NULL;
    }
    if (m_fdDri) {
        close(m_fdDri);
        m_fdDri = -1;
    }
}

bool CompEngine::AttachDecoder(mfxU16 nAddCnt)
{
    if (!m_bInit)
    {
        FRMW_TRACE_WARNI("[CompEngine]Attach decoder before init!\n");
        return false;
    }

    if (m_listDecoder.size()+nAddCnt > MAX_INPUTSTREAM_NUM)
    {
        FRMW_TRACE_ERROR("[CompEngine]The number of decoders will exceed limit of maximum %d\n", MAX_INPUTSTREAM_NUM);
        return false;
    }
    if (m_bRunning)
    {
        m_pMainVpp->Stop();
        m_measuremnt.EndTime();
    }

    //Attach decoders and MemPool
    MemPool* mp = NULL;
    MSDKDecode* pDecode = NULL;
    for (mfxU16 i = 0; i < nAddCnt; i++)
    {
        mp = new MemPool;
        mp->init();
        pDecode = new MSDKDecode;
        pDecode->SetDecodeParam(&m_vaDpy, mp, &m_measuremnt, m_bUseMeasure);
        m_vMemPool.push_back(mp);
        m_listDecoder.push_back(pDecode);
        m_pMainVpp->LinkPrevElement(pDecode);
        m_pMainSession->JoinSession(*(pDecode->GetMSDKSession()));
        if (m_bRunning)
            pDecode->Start();
    }
    m_pMainVpp->SetVPPCompNum(m_listDecoder.size());
    if (m_bRunning)
    {
        m_pMainVpp->Start();
        m_measuremnt.StartTime(m_vMemPool.size());
    }
    FRMW_TRACE_INFO("[CompEngine]Attach decoders %d\n", nAddCnt);
    return true;
}

bool CompEngine::DetachDecoder(mfxU16 nReduCnt)
{
    if (!m_bInit)
    {
        FRMW_TRACE_WARNI("[CompEngine]Detach decoder before init\n");
        return false;
    }

    if (m_listDecoder.size() <= nReduCnt)
    {
        FRMW_TRACE_ERROR("[CompEngine]Decoders detached are greater than the number held\n");
        return false;
    }
    if (m_bRunning)
    {
        m_pMainVpp->Stop();
        m_measuremnt.EndTime();
    }

    MSDKDecode* pDecode = NULL;
    MemPool* mp = NULL;
    for (mfxU16 i = 0; i < nReduCnt; i++)
    {
        pDecode = m_listDecoder.back();
        m_listDecoder.pop_back();
        if (m_bRunning)
            pDecode->Stop();
        m_pMainVpp->UnlinkPrevElement(pDecode);
        delete pDecode;
        mp = m_vMemPool.back();
        m_vMemPool.pop_back();
        delete mp;
    }
    m_pMainVpp->SetVPPCompNum(m_listDecoder.size());
    if (m_bRunning)
    {
        m_pMainVpp->Start();
        m_measuremnt.StartTime(m_vMemPool.size());
    }
    FRMW_TRACE_INFO("[CompEngine]Detach decoders %d\n", nReduCnt);
    return true;
}

bool CompEngine::AttachVppEncoder(mfxU16 width, mfxU16 height, mfxU16 bitrate, int nLogicIndex)
{
    if (!m_bInit)
    {
        FRMW_TRACE_WARNI("[CompEngine]Attach vpp/encoder before init\n");
        return false;
    }

    if (!width || !height || !bitrate)
    {
        FRMW_TRACE_ERROR("[CompEngine]Invalid input parameters\n");
        return false;
    }

    MSDKVpp* pVpp = new MSDKVpp(VPP_RESIZE, *this);
    VppConfig vppCfg;
    vppCfg.comp_num = 1;
    vppCfg.out_width = width;
    vppCfg.out_height = height;
    pVpp->SetVppParam(&m_vaDpy, &vppCfg, &m_measuremnt, m_bUseMeasure);
    m_pMainSession->JoinSession(*(pVpp->GetMSDKSession()));
    pVpp->LinkPrevElement(m_pMainVpp);

    MSDKEncode* pEncode = new MSDKEncode(m_pNotify, SLAVE);
    pEncode->SetEncodeParam(&m_vaDpy, bitrate, nLogicIndex, &m_measuremnt, m_bUseMeasure);
    m_pMainSession->JoinSession(*(pEncode->GetMSDKSession()));
    pEncode->LinkPrevElement(pVpp);
    m_listVpp.push_back(pVpp);
    m_listEncoder.push_back(pEncode);
    FRMW_TRACE_INFO("[CompEngine]Attach Vpp: %p, Encoder: %p Done.\n", pVpp, pEncode);
    return true;
}

void CompEngine::FeedData(unsigned char* pData, int nLen, int streamIndex)
{
    if (!m_bInit)
    {
        FRMW_TRACE_WARNI("[CompEngine]Feed data before init will lead to the Segmentation default\n");
        return;
    }

    if (!pData || streamIndex >= m_vMemPool.size())
    {
        FRMW_TRACE_ERROR("[CompEngine]Feed data failed since of invalid param\n");
        return;
    }

    int mempool_freeflat;
    while (true) {
        mempool_freeflat = m_vMemPool[streamIndex]->GetFreeFlatBufSize();
        if (mempool_freeflat < nLen) {
            usleep(1000);
            FRMW_TRACE_WARNI("[CompEngine]-----Mempool %p for streamIndex %d is nearly full, can not feed data any more\n", m_vMemPool[streamIndex], streamIndex);
            continue;
        }
        break;
    }
    unsigned char* mempool_wrptr = m_vMemPool[streamIndex]->GetWritePtr();
    memcpy(mempool_wrptr, pData, nLen);
    m_vMemPool[streamIndex]->UpdateWritePtrCopyData(nLen);
}

void CompEngine::SetDataEos(unsigned int streamIndex)
{
    if (!m_bInit)
    {
        FRMW_TRACE_WARNI("[CompEngine]Set the MemPool not exist before init\n");
        return;
    }

    if (streamIndex >= m_vMemPool.size())
    {
        FRMW_TRACE_ERROR("[CompEngine]streamIndex is out of range, fail to set eos\n");
        return;
    }
    m_vMemPool[streamIndex]->SetDataEof(true);
}

void CompEngine::SetMemPoolName(unsigned int streamIndex, const char* name)
{
    if (streamIndex >= m_vMemPool.size())
    {
        FRMW_TRACE_WARNI("[CompEngine]streamIndex is out of range, fail to set its name\n");
        return;
    }
    m_vMemPool[streamIndex]->SetInputName(name);
}

bool CompEngine::StartTrain()
{
    if (!m_pCompTrainer)
        m_pCompTrainer = new CompTrainer(m_vMemPool, m_listDecoder, *m_pMainVpp);
    return m_pCompTrainer->Start();
}

void CompEngine::StopTrain()
{
    if (m_pCompTrainer)
        m_pCompTrainer->Stop();
}

const char* CompEngine::GetMemPoolName(unsigned int streamIndex)
{
    if (streamIndex >= m_vMemPool.size())
    {
        FRMW_TRACE_WARNI("[CompEngine]Get MemPool name failed since of out of range %d\n");
        return NULL;
    }
    return m_vMemPool[streamIndex]->GetInputName();
}

int CompEngine::GetMemDataSize(unsigned int streamIndex)
{
    if (streamIndex >= m_vMemPool.size())
    {
        FRMW_TRACE_WARNI("[CompEngine]streamIndex is out of range, get data size failed\n");
        return -1;
    }
    return m_vMemPool[streamIndex]->GetBufFullness();
}

bool CompEngine::Init(mfxU8 uCompNum, mfxU16 width, mfxU16 height, mfxU16 bitrate, int nLogicIndex, bool bUseMeasure /*= true*/)
{
    if (!uCompNum || uCompNum > MAX_INPUTSTREAM_NUM || !width || !height || !bitrate)
    {
        FRMW_TRACE_ERROR("Init parameter invalid, return\n");
        return false;
    }

    if (1 == m_nEngineCnt)
        CompEngine::CreateVAEnvDRM();
    m_bUseMeasure = bUseMeasure;
    //Create main session
    m_pMainSession = MSDKBase::CreateSession(m_vaDpy);
    if (!m_pMainSession) {
        FRMW_TRACE_ERROR("Init main session failed\n");
        return false;
    }

    //Init all decoders
    MemPool* mp = NULL;
    MSDKDecode* pDecode = NULL;
    for (mfxU8 i = 0; i < uCompNum; i++)
    {
        mp = new MemPool;
        mp->init();
        pDecode = new MSDKDecode;
        pDecode->SetDecodeParam(&m_vaDpy, mp, &m_measuremnt, m_bUseMeasure);
        m_vMemPool.push_back(mp);
        m_listDecoder.push_back(pDecode);
        m_pMainSession->JoinSession(*(pDecode->GetMSDKSession()));
    }
    FRMW_TRACE_INFO("[CompEngine]Alloc mempool, create corresponding decoders and complete their initialization\n");

    //Init main vpp
    if (!m_pMainVpp)
        m_pMainVpp = new MSDKVpp(VPP_COMP, *this);
    VppConfig vppCfg;
    vppCfg.comp_num = uCompNum;
    vppCfg.out_width = width;
    vppCfg.out_height = height;
    m_pMainVpp->SetVppParam(&m_vaDpy, &vppCfg, &m_measuremnt, m_bUseMeasure);
    m_pMainSession->JoinSession(*(m_pMainVpp->GetMSDKSession()));
    for (std::list<MSDKDecode*>::iterator it = m_listDecoder.begin();
         it != m_listDecoder.end(); ++it)
        m_pMainVpp->LinkPrevElement(*it);
    FRMW_TRACE_INFO("[CompEngine]Create/Re-set the main_vpp, it is used to composite multiple frames\n");

    //Init main encoder
    m_mainEncoder.SetEncodeParam(&m_vaDpy, bitrate, nLogicIndex, &m_measuremnt, m_bUseMeasure);
    m_pMainSession->JoinSession(*(m_mainEncoder.GetMSDKSession()));
    m_mainEncoder.LinkPrevElement(m_pMainVpp);

    m_bInit = true;
    FRMW_TRACE_INFO("[CompEngine]Init %d decoders, main_vpp: %p, main_encoder: %p Done.\n",
                    m_listDecoder.size(), m_pMainVpp, &m_mainEncoder);
    return true;
}

bool CompEngine::Start()
{
    if (!m_bInit)
    {
        FRMW_TRACE_WARNI("[CompEngine]Start before init, there's no thread will be startup\n");
        return false;
    }
    FRMW_TRACE_INFO("[CompEngine]-----Start pipeline...\n");
    m_measuremnt.StartTime(m_vMemPool.size());
    bool res = true;

    //Start decoders
    for (std::list<MSDKDecode*>::iterator it = m_listDecoder.begin();
         it != m_listDecoder.end(); ++it)
        res &= (*it)->Start();

    //Start vpp
    res &= m_pMainVpp->Start();
    for (std::list<MSDKVpp*>::iterator it = m_listVpp.begin();
         it != m_listVpp.end(); ++it)
        res &= (*it)->Start();

    //Start encoders
    res &= m_mainEncoder.Start();
    for (std::list<MSDKEncode*>::iterator it = m_listEncoder.begin();
         it != m_listEncoder.end(); ++it)
        res &= (*it)->Start();

    FRMW_TRACE_INFO("[CompEngine]-----Start pipeline done.\n");
    m_bRunning = true;
    return res;
}

unsigned long CompEngine::Stop(STOP_MODE mode, bool bShowVppInfo /*= false*/)
{
    if (!m_bRunning)
    {
        FRMW_TRACE_WARNI("[CompEngine]No thread is running, do not need to STOP\n");
        return 0;
    }

    //Stop decoders
    for (std::list<MSDKDecode*>::iterator it = m_listDecoder.begin();
         it != m_listDecoder.end(); ++it)
    {
        if (STOP_ATONCE == mode)
            (*it)->Stop();
        else    //STOP_DELAY == mode
            (*it)->WaitForStop();
    }

    //Stop main vpp & encoder
    if (STOP_ATONCE == mode)
    {
        m_pMainVpp->Stop();
        m_mainEncoder.Stop();
    }
    else
    {
        m_pMainVpp->WaitForStop();
        m_mainEncoder.WaitForStop();
    }

    for (std::list<MSDKVpp*>::iterator it = m_listVpp.begin();
         it != m_listVpp.end(); ++it)
    {
        if (STOP_ATONCE == mode)
            (*it)->Stop();
        else
            (*it)->WaitForStop();
    }

    for (std::list<MSDKEncode*>::iterator it = m_listEncoder.begin();
         it != m_listEncoder.end(); ++it)
    {
        if (STOP_ATONCE == mode)
            (*it)->Stop();
        else
            (*it)->WaitForStop();
    }

    FRMW_TRACE_INFO("[CompEngine]-----Stop pipeline done.\n");
    m_bRunning = false;
    unsigned long last_time = m_measuremnt.EndTime();
    if (bShowVppInfo)
        m_measuremnt.ShowPerformanceInfo();
    return last_time;
}

void CompEngine::ForceKeyFrame(unsigned int nLogicIndex)
{
    if (!m_bInit)
    {
        FRMW_TRACE_WARNI("[CompEngine]Force key frame before objects constructed\n");
        return;
    }

    if (0 == nLogicIndex)
    {
        m_mainEncoder.ForceKeyFrame();
        return;
    }

    int i = 1;
    bool bFind = false;
    for (std::list<MSDKEncode*>::iterator it = m_listEncoder.begin();
         it != m_listEncoder.end(); ++it, ++i)
    {
        if (nLogicIndex == i)
        {
            bFind = true;
            (*it)->ForceKeyFrame();
            break;
        }
    }
    if (!bFind)
        FRMW_TRACE_WARNI("[CompEngine]Force key frame failed since of not find the corresponding encoder!\n");
}
