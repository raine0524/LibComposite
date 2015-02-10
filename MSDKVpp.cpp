#include <sys/time.h>
#include <math.h>
#include "MSDKVpp.h"

static unsigned int GetSysTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000*1000+tv.tv_usec;
}

MSDKVpp::MSDKVpp(VPP_MODE mode, MSDKVppCallback& rCallback)
:MSDKDecodeVpp(ELEMENT_VPP)
,m_mode(mode)
,m_pVpp(NULL)
,m_bReinit(false)
,m_tCompStc(0)
,m_frameRate(0)
,m_nSleepInterval(0)
,m_nInterFrameSpace(0)
,m_argMasterGrowthPotential(DBL_MIN)
,m_argSubGrowthPotential(log(3))
,m_argXPoint(LOGISTIC_INTERVAL_UPPER/2)
,m_rCallback(rCallback)
{
    memset(&m_vppCfg, 0, sizeof(m_vppCfg));
    m_vRect.reserve(MAX_INPUTSTREAM_NUM);
    m_mfxVideoParam.ExtParam = (mfxExtBuffer**)m_pExtBuf;
}

MSDKVpp::~MSDKVpp()
{
    if (m_pVpp) {
        m_pVpp->Close();
        delete m_pVpp;
        m_pVpp = NULL;
    }
}

bool MSDKVpp::SetVppParam(VADisplay* pVaDpy, VppConfig* pVppCfg, Measurement* pMeasuremnt, bool bUseMeasure)
{
    if (!pVppCfg)
    {
        VPP_TRACE_ERROR("Invalid input vpp parameters, set parameters failed\n");
        return false;
    }
    m_vppCfg = *pVppCfg;
    if (VPP_COMP == m_mode && bUseMeasure)
        m_pMeasuremnt = pMeasuremnt;

    if (!m_pVpp)
    {
#ifdef CONFIG_USE_MFXALLOCATOR
        if (!CreateSessionAllocator(pVaDpy))
            return false;
#else
        MFXVideoSession* pSession = CreateSession(*pVaDpy);
        if (!pSession)
        {
            VPP_TRACE_ERROR("[MSDKVpp]-----Create session failed\n");
            return false;
        }
        m_pSession = pSession;
#endif
        m_pVpp = new MFXVideoVPP(*m_pSession);
    }
    return true;
}

void MSDKVpp::SetVPPCompRect(int streamIndex, const VppRect* rect)
{
    if (streamIndex >= MAX_OUTPUT_NUM || !rect || !rect->w || !rect->h) {
        VPP_TRACE_ERROR("[MSDKVpp]-----invalid dst rect\n");
    }

    Locker<Mutex> l1(m_xMsdkInit);
    m_vRect[streamIndex] = *rect;
    if (m_bInit)
    {
        Locker<Mutex> l2(m_xMsdkReinit);
        if (!m_bReinit)
            m_bReinit = true;
    }
    VPP_TRACE_INFO("[MSDKVpp]Attach stream %d vpp rect, %d/%d/%d/%d\n",
           streamIndex, rect->x, rect->y, rect->w, rect->h);
}

void MSDKVpp::LinkPrevElement(MSDKDecodeVpp* pCodec)
{
    MediaBuf buf;
    buf.pRingBuf = pCodec->GetOutputRingBuf();
    if (VPP_COMP == m_mode) {
        VPP_TRACE_INFO("[MSDKVpp]-----This vpp is used to composite, link previous decoder %p\n", pCodec);
    } else {
        MSDKVpp* pVpp = dynamic_cast<MSDKVpp*>(pCodec);
        pVpp->AddNextElement(this);
        VPP_TRACE_INFO("[MSDKVpp]-----This vpp is used to resize, link previous vpp %p\n", pVpp);
    }
    buf.nDropFrameNums = 0;
    m_mapMediaBuf[pCodec] = buf;
}

void MSDKVpp::ReleaseSurface()
{
    if (VPP_COMP == m_mode)
    {
        mfxFrameSurface1* pFrameSurface = NULL;
        m_mfxRingBuf.Get(pFrameSurface);
        {
            Locker<Mutex> l(m_mutex);
            pFrameSurface->Data.Locked--;
            if (0 == pFrameSurface->Data.Locked)
            {
                m_mfxRingBuf.Pop(pFrameSurface);
                for (std::list<MSDKBase*>::iterator it = m_listNextElem.begin();
                     it != m_listNextElem.end(); ++it)
                    (*it)->NotifyCanAccess();
            }
        }
    }
}

void MSDKVpp::SetVppCompParam(mfxExtVPPComposite* pVppComp)
{
    memset(pVppComp, 0, sizeof(mfxExtVPPComposite));
    pVppComp->Header.BufferId = MFX_EXTBUFF_VPP_COMPOSITE;
    pVppComp->Header.BufferSz = sizeof(mfxExtVPPComposite);
    pVppComp->NumInputStream = (mfxU16)m_vppCfg.comp_num;
    //background color(black)
    pVppComp->Y = 0;
    pVppComp->U = 128;
    pVppComp->V = 128;
    pVppComp->InputStream = new mfxVPPCompInputStream[pVppComp->NumInputStream];
    memset(pVppComp->InputStream, 0, sizeof(mfxVPPCompInputStream)*pVppComp->NumInputStream);
    VPP_TRACE_INFO("[MSDKVpp]About to set composition parameter, number of stream is %d\n", pVppComp->NumInputStream);
    for (int i = 0; i < pVppComp->NumInputStream; i++)
    {
        pVppComp->InputStream[i].DstX = m_vRect[i].x;
        pVppComp->InputStream[i].DstY = m_vRect[i].y;
        pVppComp->InputStream[i].DstW = m_vRect[i].w;
        pVppComp->InputStream[i].DstH = m_vRect[i].h;
    }
}

#ifndef CONFIG_USE_MFXALLOCATOR
mfxStatus MSDKVpp::AllocFrames(mfxFrameAllocRequest* pRequest)
{
    //Allocate surfaces for VPP: Out
    mfxU16 width = (mfxU16)MSDK_ALIGN32(m_mfxVideoParam.vpp.Out.Width);
    mfxU16 height = (mfxU16)MSDK_ALIGN32(m_mfxVideoParam.vpp.Out.Height);
    mfxU32 nSurfSize = width*height*12/8;       //NV12 format is a 12 bits per pixel format
    if (m_pSurfaceBuffers)
    {
        delete[] m_pSurfaceBuffers;
        m_pSurfaceBuffers = NULL;
    }
    m_pSurfaceBuffers = (mfxU8*)new mfxU8[nSurfSize*m_nSurfaces];

    if (m_pSurfacePool)
    {
        for (int i = 0; i < m_nSurfaces; i++)
            delete m_pSurfacePool[i];
        delete[] m_pSurfacePool;
        m_pSurfacePool = NULL;
    }
    m_pSurfacePool = new mfxFrameSurface1*[m_nSurfaces];
    if (!m_pSurfacePool)
        return MFX_ERR_MEMORY_ALLOC;
    for (int i = 0; i < m_nSurfaces; i++) {
        m_pSurfacePool[i] = new mfxFrameSurface1;
        memset(m_pSurfacePool[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_pSurfacePool[i]->Info), &(pRequest->Info), sizeof(mfxFrameInfo));
        m_pSurfacePool[i]->Data.Y = &m_pSurfaceBuffers[nSurfSize*i];
        m_pSurfacePool[i]->Data.U = m_pSurfacePool[i]->Data.Y+width*height;
        m_pSurfacePool[i]->Data.V = m_pSurfacePool[i]->Data.U+1;
        m_pSurfacePool[i]->Data.Pitch = width;
    }
    return MFX_ERR_NONE;
}
#endif

mfxStatus MSDKVpp::InitVpp(mfxFrameSurface1* pFrameSurface)
{
    if (m_pMeasuremnt)
    {
        m_pMeasuremnt->GetLock();
        m_pMeasuremnt->TimeStpStart(VPP_INIT_TIME_STAMP, this);
        m_pMeasuremnt->RelLock();
    }

    //Input data
    m_mfxVideoParam.vpp.In.FrameRateExtN    = pFrameSurface->Info.FrameRateExtN;
    m_mfxVideoParam.vpp.In.FrameRateExtD    = pFrameSurface->Info.FrameRateExtD;
    m_mfxVideoParam.vpp.In.FourCC           = pFrameSurface->Info.FourCC;
    m_mfxVideoParam.vpp.In.ChromaFormat     = pFrameSurface->Info.ChromaFormat;
    m_mfxVideoParam.vpp.In.PicStruct        = pFrameSurface->Info.PicStruct;
    m_mfxVideoParam.vpp.In.CropX            = pFrameSurface->Info.CropX;
    m_mfxVideoParam.vpp.In.CropY            = pFrameSurface->Info.CropY;
    m_mfxVideoParam.vpp.In.CropW            = pFrameSurface->Info.CropW;
    m_mfxVideoParam.vpp.In.CropH            = pFrameSurface->Info.CropH;
    VPP_TRACE_INFO("[MSDKVpp]-----Init VPP, in dst %d/%d\n", m_mfxVideoParam.vpp.In.CropW, m_mfxVideoParam.vpp.In.CropH);
    m_mfxVideoParam.vpp.In.Width            = MSDK_ALIGN16(m_mfxVideoParam.vpp.In.CropW);
    m_mfxVideoParam.vpp.In.Height           = MSDK_ALIGN16(m_mfxVideoParam.vpp.In.CropH);

    //Output data
    MSDKDecode* pDecode = NULL;
    for (std::map<MSDKDecodeVpp*, MediaBuf>::iterator it = m_mapMediaBuf.begin();
         it != m_mapMediaBuf.end(); it++)
    {
        pDecode = dynamic_cast<MSDKDecode*>(it->first);
        //select the maximum frame rate among the multiplex stream to set the composite frame rate
        if (pDecode->GetFrameRateExtN()/pDecode->GetFrameRateExtD() > m_frameRate)
        {
            m_mfxVideoParam.vpp.Out.FrameRateExtN = pDecode->GetFrameRateExtN();
            m_mfxVideoParam.vpp.Out.FrameRateExtD = pDecode->GetFrameRateExtD();
            m_frameRate = m_mfxVideoParam.vpp.Out.FrameRateExtN/m_mfxVideoParam.vpp.Out.FrameRateExtD;
        }
    }
    for (std::map<MSDKDecodeVpp*, MediaBuf>::iterator it = m_mapMediaBuf.begin();
         it != m_mapMediaBuf.end(); it++)
        dynamic_cast<MSDKDecode*>(it->first)->SetCompFrameRate(m_frameRate);
    m_nInterFrameSpace = 1000*1000/m_frameRate;
    m_nSleepInterval = m_nInterFrameSpace;     //sleep full of the inter-frame space
    //calculate the arg `growth potential` in logistic equation
    m_argMasterGrowthPotential = log(2*m_nInterFrameSpace-1)/(LOGISTIC_INTERVAL_UPPER/2);
    VPP_TRACE_INFO("[MSDKVpp]-----Vpp output frame rate: %d\n", m_frameRate);
    m_mfxVideoParam.vpp.Out.FourCC          = m_mfxVideoParam.vpp.In.FourCC;
    m_mfxVideoParam.vpp.Out.ChromaFormat    = m_mfxVideoParam.vpp.In.ChromaFormat;
    m_mfxVideoParam.vpp.Out.PicStruct       = m_mfxVideoParam.vpp.In.PicStruct;
    m_mfxVideoParam.vpp.Out.CropX           = 0;
    m_mfxVideoParam.vpp.Out.CropY           = 0;
    m_mfxVideoParam.vpp.Out.CropW           = m_vppCfg.out_width;
    m_mfxVideoParam.vpp.Out.CropH           = m_vppCfg.out_height;
    m_mfxVideoParam.vpp.Out.Width           = MSDK_ALIGN16(m_mfxVideoParam.vpp.Out.CropW);
    m_mfxVideoParam.vpp.Out.Height          = MSDK_ALIGN16(m_mfxVideoParam.vpp.Out.CropH);
#ifdef CONFIG_USE_MFXALLOCATOR
    m_mfxVideoParam.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    VPP_TRACE_INFO("----------------vpp using video memory\n");
#else
    m_mfxVideoParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    VPP_TRACE_INFO("----------------vpp using system memory\n");
#endif

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest vppRequest[2];     //[0]-in, [1]-out
    memset(&vppRequest, 0, sizeof(mfxFrameAllocRequest)*2);
    sts = m_pVpp->QueryIOSurf(&m_mfxVideoParam, vppRequest);
    if (sts < MFX_ERR_NONE)
        return sts;
    m_nSurfaces = vppRequest[1].NumFrameSuggested+15;
    VPP_TRACE_INFO("[MSDKVpp]-----VPP suggest number of surfaces is in/out %d/%d\n",
           vppRequest[0].NumFrameSuggested, m_nSurfaces);

    VPP_TRACE_INFO("[MSDKVpp]-----Creating VPP surface pool, surface num %d\n", m_nSurfaces);
    sts = AllocFrames(&vppRequest[1]);

    mfxExtVPPComposite vppComp;
    if (VPP_COMP == m_mode) {
        SetVppCompParam(&vppComp);
        m_mfxVideoParam.ExtParam[0] = (mfxExtBuffer*)&vppComp;
        m_mfxVideoParam.NumExtParam = 1;
    }

    sts = m_pVpp->Init(&m_mfxVideoParam);
    if (MFX_WRN_FILTER_SKIPPED == sts) {
        VPP_TRACE_INFO("[MSDKVpp]-----Got MFX_WRN_FILTER_SKIPPED\n");
        sts = MFX_ERR_NONE;
    }
    if (VPP_COMP == m_mode)
        delete[] vppComp.InputStream;

    if (m_pMeasuremnt)
    {
        m_pMeasuremnt->GetLock();
        m_pMeasuremnt->TimeStpFinish(VPP_INIT_TIME_STAMP, this);
        m_pMeasuremnt->RelLock();
    }
    return sts;
}

mfxStatus MSDKVpp::DoingVpp(mfxFrameSurface1* pInSurf, mfxFrameSurface1* pOutSurf)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint syncpV;

    do {
        for (;;) {
            //Process a frame asychronously(returns immediately).
            sts = m_pVpp->RunFrameVPPAsync(pInSurf, pOutSurf, NULL, &syncpV);
            if (sts > MFX_ERR_NONE && !syncpV) {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    usleep(500);
            } else {
                if (sts > MFX_ERR_NONE && syncpV)
                    sts = MFX_ERR_NONE;     //Ignore warnings if output is available.
                break;
            }
        }

        if (MFX_ERR_MORE_DATA == sts && NULL != pInSurf)
            break;      //Composite case, direct return

        sts = m_pSession->SyncOperation(syncpV, 60000);
        if (MFX_ERR_NONE == sts) {
            if (m_pMeasuremnt)
            {
                m_pMeasuremnt->GetLock();
                m_pMeasuremnt->TimeStpFinish(VPP_FRAME_TIME_STAMP, this);
                m_pMeasuremnt->RelLock();
            }

            while (m_mfxRingBuf.IsFull()) {
                //printf("[MSDKVpp]-----Ring buffer for %p is full, cann't push element any more\n", this);
                usleep(1000);
            }

            if (VPP_COMP == m_mode)
                pOutSurf->Data.Locked += m_listNextElem.size();
            else
                pOutSurf->Data.Locked++;
            m_mfxRingBuf.Push(pOutSurf);
        }
    } while(!pInSurf && sts >= MFX_ERR_NONE);
    return sts;
}

int MSDKVpp::HandleProcess()
{
    if (m_vppCfg.comp_num != m_mapMediaBuf.size())
    {
        VPP_TRACE_ERROR("[MSDKVpp]-----Something wrong here, all sinkpads detached\n");
        return -1;
    }
    std::map<MSDKDecodeVpp*, MediaBuf>::iterator it = m_mapMediaBuf.begin();
    mfxFrameSurface1* pInSurface = NULL;
    mfxStatus sts = MFX_ERR_NONE;
    int nIndex = 0;
    m_tCompStc = 0;

    while(!m_bWantToStop) {
        usleep(1000);

        if (VPP_COMP == m_mode) {
            //composite case, break if no more data when preparing vpp frames
            int prepare_result = PrepareVppCompFrames();
            if (-1 == prepare_result)
                break;
            if (1 == prepare_result)
            {
                m_bEndOfStream = true;
                break;
            }
            //printf("[MSDKVpp %p]-----Prepare composite frames over\n", this);
        } else {
            if (!m_bAccessNextElem)
                continue;

            if (it->second.pRingBuf->IsEmpty()) {       //No data
                if (it->first->GetDataEos())     //No more data in the furture
                    pInSurface = NULL;
                else
                    continue;
            } else {
                it->second.pRingBuf->Get(pInSurface);
            }
        }

        if (!m_bInit) {
            m_mapMediaBuf.begin()->second.pRingBuf->Get(pInSurface);

            Locker<Mutex> l(m_xMsdkInit);
            sts = InitVpp(pInSurface);
            if (MFX_ERR_NONE == sts) {
                m_bInit = true;
                VPP_TRACE_INFO("[MSDKVpp]VPP element %p init successfully\n", this);
            } else {
                VPP_TRACE_ERROR("[MSDKVpp]VPP create failed: %d\n", sts);
                break;
            }
        }

        if (m_bReinit) {
            //Re-init VPP.
            mfxU32 before_change = GetSysTime();
            VPP_TRACE_INFO("[MSDKVpp]stop/init vpp\n");
            m_pVpp->Close();

            VPP_TRACE_INFO("Re-init VPP...\n");
            m_mapMediaBuf.begin()->second.pRingBuf->Get(pInSurface);
            {
                Locker<Mutex> l(m_xMsdkReinit);
                sts = InitVpp(pInSurface);
                m_bReinit = false;
            }
            VPP_TRACE_INFO("[MSDKVpp]Re-Init VPP takes time %u(us)\n", GetSysTime()-before_change);
        }

        //malloc memory in InitVpp(pInSurface), then will get free surface slot
        nIndex = GetFreeSurfaceIndex(m_pSurfacePool, m_nSurfaces);
        if (MFX_ERR_NOT_FOUND == nIndex) {
            //printf("[MSDKVpp]-----Cann't get free surface slot, check if not free\n");
            continue;
        }

        if (VPP_COMP == m_mode) {
            if (m_pMeasuremnt)
            {
                m_pMeasuremnt->GetLock();
                m_pMeasuremnt->TimeStpStart(VPP_FRAME_TIME_STAMP, this);
                m_pMeasuremnt->RelLock();
            }

            for (it = m_mapMediaBuf.begin(); it != m_mapMediaBuf.end(); ++it) {
                it->second.pRingBuf->Pop(pInSurface);
                //printf("[MSDKVpp %p]Get pInSurface %p succ\n", pInSurface);
                sts = DoingVpp(pInSurface, m_pSurfacePool[nIndex]);
                pInSurface->Data.Locked--;
            }
            //printf("[MSDKVpp %p]-----Composite multiple frames into one frame successfully\n", this);
        } else {
            sts = DoingVpp(pInSurface, m_pSurfacePool[nIndex]);
            m_bAccessNextElem = false;
            dynamic_cast<MSDKVpp*>(it->first)->ReleaseSurface();
            if (!pInSurface)        //resize case, break when be notified end of stream
                break;
        }
    }

    if (!m_bWantToStop) {
        VPP_TRACE_INFO("[%p]This work flow finish video processing and will be stopped.\n", this);
        m_bWantToStop = true;
    }
    m_rCallback.StopTrain();
    return 0;
}

int MSDKVpp::PrepareVppCompFrames()
{
    std::map<MSDKDecodeVpp*, MediaBuf>::iterator it;
    MSDKDecode* pDecode = NULL;

    if (0 == m_tCompStc) {
        //Init stage. Synchronous reference time sequence
        //Wait for all the frame to generate the 1st composited one.
        for (it = m_mapMediaBuf.begin(); it != m_mapMediaBuf.end(); ++it)
        {
            pDecode = dynamic_cast<MSDKDecode*>(it->first);
            while (!pDecode->FullStage())
            {
                //pDecode->RING_BUFFER is not in full stage, just sleep and wait
                if (m_bWantToStop)
                    return -1;
                usleep(1000);
            }
        }
        m_rCallback.StartTrain();
    } else {
        usleep(m_nSleepInterval);
        unsigned int nEosCnt = 0;
        for (it = m_mapMediaBuf.begin(); it != m_mapMediaBuf.end(); ++it)
        {
            pDecode = dynamic_cast<MSDKDecode*>(it->first);
            if (pDecode->GetDataEos() && it->second.pRingBuf->IsEmpty())
            {
                nEosCnt++;
                if (m_mapMediaBuf.size() == nEosCnt)
                {
                    VPP_TRACE_INFO("[MSDKVpp]-----All video stream EOS, can end VPP now: %d\n",nEosCnt);
                    return 1;
                }
                pDecode->RequestLastSurface();
                continue;
            }
            //Frames comes late.
            //Use last surface, maybe need to drop 1 frame in future.
            if (it->second.pRingBuf->IsEmpty())
            {
                pDecode->RequestLastSurface();
                //it->second.nDropFrameNums++;
            }
        }
    }
    m_tCompStc = GetSysTime();
    return 0;
}

void MSDKVpp::TrainSleepInterval(double regre_coeff)
{
    if (0.0 == regre_coeff)
        return;

    double module = abs(regre_coeff);
    double weight = SubLogisticEquation(module);
    double shift = 0.0;
    if (regre_coeff > 0)
    {
        shift = m_argXPoint*weight;
        m_argXPoint -= shift;
    }
    else    //regre_coeff < 0
    {
        shift = (LOGISTIC_INTERVAL_UPPER-m_argXPoint)*weight;
        m_argXPoint += shift;
    }
    m_nSleepInterval = static_cast<unsigned int>(MasterLogisticEquation(m_argXPoint));
}

double MSDKVpp::MasterLogisticEquation(double t)
{
    return 2*m_nInterFrameSpace/(1+(2*m_nInterFrameSpace-1)/exp(m_argMasterGrowthPotential*t));
}

double MSDKVpp::SubLogisticEquation(double t)
{
    return 2/(1+1/exp(m_argSubGrowthPotential*t))-1;
}
