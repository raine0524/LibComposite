#include <assert.h>
#include "MSDKEncode.h"

MSDKEncode::MSDKEncode(MSDKCodecNotify* pNotify, ENCODE_TYPE type)
:MSDKBase(ELEMENT_ENCODER)
,m_type(type)
,m_pEncode(NULL)
,m_bForceKeyFrame(false)
,m_nLogicIndex(-1)
,m_pNotify(pNotify)
#ifdef CONFIG_READ_RAW_BUFFER
,m_pInputMem(NULL)
#endif
{
    memset(&m_outputBs, 0, sizeof(m_outputBs));
    memset(&m_encCtrl, 0, sizeof(m_encCtrl));
    memset(&m_codingOpt2, 0, sizeof(m_codingOpt2));
    m_encCtrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
}

MSDKEncode::~MSDKEncode()
{
    if (m_pEncode) {
        //Session closed automatically on destruction
        m_pEncode->Close();
        delete m_pEncode;
        m_pEncode = NULL;
    }
    if (m_outputBs.Data) {
        delete[] m_outputBs.Data;
        m_outputBs.Data = NULL;
    }
}

#ifdef CONFIG_READ_RAW_BUFFER
bool MSDKEncode::SetEncodeParam(VADisplay* pVaDpy, unsigned int width, unsigned int height, unsigned short bitrate, int nLogicIndex, Measurement* pMeasuremnt, bool bUseMeasure)
#else
bool MSDKEncode::SetEncodeParam(VADisplay* pVaDpy, unsigned short bitrate, int nLogicIndex, Measurement* pMeasuremnt, bool bUseMeasure)
#endif
{
    if (!bitrate)
    {
        H264E_TRACE_ERROR("Invalid input encode parameters, set parameters failed\n");
        return false;
    }

    if (bUseMeasure)
        m_pMeasuremnt = pMeasuremnt;
    m_nLogicIndex = nLogicIndex;
#ifdef CONFIG_USE_MFXALLOCATOR
        if (!CreateSessionAllocator(pVaDpy))
            return false;
#else
        MFXVideoSession* pSession = CreateSession(*pVaDpy);
        if (!pSession)
        {
            H264E_TRACE_ERROR("[MSDKVpp]-----Create session failed\n");
            return false;
        }
        m_pSession = pSession;
#endif
    m_pEncode = new MFXVideoENCODE(*m_pSession);
    m_mfxVideoParam.mfx.CodecId             = MFX_CODEC_AVC;
    m_mfxVideoParam.mfx.CodecProfile        = MFX_PROFILE_AVC_BASELINE;
    m_mfxVideoParam.mfx.CodecLevel          = MFX_LEVEL_UNKNOWN;     //SDK functions will determine the correct level
    m_mfxVideoParam.mfx.TargetUsage         = MFX_TARGETUSAGE_BALANCED;
    m_mfxVideoParam.mfx.GopPicSize          = GROUP_OF_PICTURE;
    m_mfxVideoParam.mfx.GopRefDist          = 1;    //distance between I- or P- key frames(1 means no B-frames)
    m_mfxVideoParam.mfx.RateControlMethod   = MFX_RATECONTROL_VBR;
    m_mfxVideoParam.mfx.TargetKbps          = bitrate;
    m_mfxVideoParam.mfx.NumSlice            = 0;
    m_mfxVideoParam.mfx.NumRefFrame         = 1;
    m_mfxVideoParam.mfx.EncodedOrder        = 0;    //specify the EncodedOrder as display order
#ifdef CONFIG_READ_RAW_BUFFER
    m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN = 30;
    m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD = 1;
    m_mfxVideoParam.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
    m_mfxVideoParam.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
    m_mfxVideoParam.mfx.FrameInfo.CropX         = 0;
    m_mfxVideoParam.mfx.FrameInfo.CropY         = 0;
    m_mfxVideoParam.mfx.FrameInfo.CropW         = width;
    m_mfxVideoParam.mfx.FrameInfo.CropH         = height;
    m_mfxVideoParam.mfx.FrameInfo.Width         = MSDK_ALIGN16(width);
    m_mfxVideoParam.mfx.FrameInfo.Height        = MSDK_ALIGN16(height);
    m_mfxVideoParam.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
#endif
    m_mfxVideoParam.AsyncDepth              = 1;
    return true;
}

void MSDKEncode::LinkPrevElement(MSDKVpp* pVpp)
{
    m_mapRingBuf[pVpp] = pVpp->GetOutputRingBuf();
    if (MASTER == m_type)      //master, link the VPP_COMP
        pVpp->AddNextElement(this);
    H264E_TRACE_INFO("[MSDKEncode]-----Link previous vpp %p\n", pVpp);
}

#ifdef CONFIG_READ_RAW_BUFFER
mfxStatus MSDKEncode::AllocFrames(mfxFrameAllocRequest* pRequest)
{
    //Allocate surfaces for encoder
    mfxU16 width = (mfxU16)MSDK_ALIGN32(pRequest->Info.Width);
    mfxU16 height = (mfxU16)MSDK_ALIGN32(pRequest->Info.Height);
    mfxU32 nSurfSize = width*height*12/8;       //NV12 format is a 12 bits per pixel format
    m_pSurfaceBuffers = (mfxU8*)new mfxU8[nSurfSize*m_nSurfaces];

    //Allocate surface headers(mfxFrameSurface1) for encoder
    m_pSurfacePool = new mfxFrameSurface1*[m_nSurfaces];
    if (!m_pSurfacePool)
        return MFX_ERR_MEMORY_ALLOC;
    for (int i = 0; i < m_nSurfaces; i++) {
        m_pSurfacePool[i] = new mfxFrameSurface1;
        memset(m_pSurfacePool[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_pSurfacePool[i]->Info), &(m_mfxVideoParam.mfx.FrameInfo), sizeof(mfxFrameInfo));
        m_pSurfacePool[i]->Data.Y = &m_pSurfaceBuffers[nSurfSize*i];
        m_pSurfacePool[i]->Data.U = m_pSurfacePool[i]->Data.Y+width*height;
        m_pSurfacePool[i]->Data.V = m_pSurfacePool[i]->Data.U+1;
        m_pSurfacePool[i]->Data.Pitch = width;
    }
    return MFX_ERR_NONE;
}
#endif

mfxStatus MSDKEncode::InitEncoder(mfxFrameSurface1* pFrameSurface)
{
    if (m_pMeasuremnt)
    {
        m_pMeasuremnt->GetLock();
        m_pMeasuremnt->TimeStpStart(ENC_INIT_TIME_STAMP, this);
        m_pMeasuremnt->RelLock();
    }

    if (pFrameSurface)
    {
        m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN = pFrameSurface->Info.FrameRateExtN;
        m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD = pFrameSurface->Info.FrameRateExtD;
        m_mfxVideoParam.mfx.FrameInfo.FourCC        = pFrameSurface->Info.FourCC;
        m_mfxVideoParam.mfx.FrameInfo.ChromaFormat  = pFrameSurface->Info.ChromaFormat;
        m_mfxVideoParam.mfx.FrameInfo.CropX         = pFrameSurface->Info.CropX;
        m_mfxVideoParam.mfx.FrameInfo.CropY         = pFrameSurface->Info.CropY;
        m_mfxVideoParam.mfx.FrameInfo.CropW         = pFrameSurface->Info.CropW;
        m_mfxVideoParam.mfx.FrameInfo.CropH         = pFrameSurface->Info.CropH;
        m_mfxVideoParam.mfx.FrameInfo.Width         = pFrameSurface->Info.Width;
        m_mfxVideoParam.mfx.FrameInfo.Height        = pFrameSurface->Info.Height;
        m_mfxVideoParam.mfx.FrameInfo.PicStruct     = pFrameSurface->Info.PicStruct;
    }

    //extcoding option2
    m_codingOpt2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    m_codingOpt2.Header.BufferSz = sizeof(m_codingOpt2);
    m_codingOpt2.RepeatPPS       = MFX_CODINGOPTION_OFF;     //No repeat pps
    m_encExtParams.push_back(reinterpret_cast<mfxExtBuffer*>(&m_codingOpt2));
#ifdef CONFIG_USE_MFXALLOCATOR
    m_mfxVideoParam.IOPattern               = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    H264E_TRACE_INFO("----------------encoder using video memory\n");
#else
    m_mfxVideoParam.IOPattern               = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    H264E_TRACE_INFO("----------------encoder using system memory\n");
#endif
    //set mfx video ExtParam
    if (!m_encExtParams.empty())
    {
        m_mfxVideoParam.ExtParam    = &m_encExtParams.front();  //vector is stored linearly in memory
        m_mfxVideoParam.NumExtParam = (mfxU16)m_encExtParams.size();
        H264E_TRACE_INFO("[MSDKEncode]-----init encoder, ext param number is %d\n", m_mfxVideoParam.NumExtParam);
    }

    mfxFrameAllocRequest encRequest;
    memset(&encRequest, 0, sizeof(encRequest));
    mfxStatus sts = m_pEncode->QueryIOSurf(&m_mfxVideoParam, &encRequest);
    if (MFX_ERR_NONE > sts)
        return sts;
    H264E_TRACE_INFO("[MSDKEncode]-----Enc suggest surfaces %d\n", encRequest.NumFrameSuggested);

#ifdef CONFIG_READ_RAW_BUFFER
    m_nSurfaces = encRequest.NumFrameSuggested+FRAME_POOL_SIZE;
    sts = AllocFrames(&encRequest);
#endif

    sts = m_pEncode->Init(&m_mfxVideoParam);
    if (sts != MFX_ERR_NONE) {
        H264E_TRACE_ERROR("[MSDKEncode]-----enc init return %d\n", sts);
        assert(sts >= MFX_ERR_NONE);
    }

    mfxVideoParam par;
    memset(&par, 0, sizeof(par));
    sts = m_pEncode->GetVideoParam(&par);

    //Prepare Media SDK bit stream buffer for encoder
    memset(&m_outputBs, 0, sizeof(m_outputBs));
    m_outputBs.MaxLength = par.mfx.BufferSizeInKB*2000;
    m_outputBs.Data = new mfxU8[m_outputBs.MaxLength];
    if (!m_outputBs.Data)
        return MFX_ERR_MEMORY_ALLOC;
    H264E_TRACE_INFO("[MSDKEncode]-----output bitstream buffer size %d\n", m_outputBs.MaxLength);

    if (m_pMeasuremnt)
    {
        m_pMeasuremnt->GetLock();
        m_pMeasuremnt->TimeStpFinish(ENC_INIT_TIME_STAMP, this);
        m_pMeasuremnt->RelLock();
    }
    return sts;
}

int MSDKEncode::HandleProcess()
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameSurface1* pFrameSurface = NULL;
    mfxSyncPoint syncpE;
    mfxEncodeCtrl* pEncCtrl = NULL;
    int startP = -1;
#ifndef CONFIG_READ_RAW_BUFFER
    std::map<MSDKVpp*, RING_BUFFER*>::iterator it = m_mapRingBuf.begin();
#endif

    while (!m_bWantToStop) {
        usleep(1000);

        //Check if need to generate key frame
        if (m_bForceKeyFrame)
            pEncCtrl = &m_encCtrl;
        else
            pEncCtrl = NULL;

#ifndef CONFIG_READ_RAW_BUFFER
        if (it->second->IsEmpty())      //No data
        {
            //printf("[MSDKEncode]-----There's no more data, just continue or exit the main loop\n");
            if (it->first->GetDataEos())     //No more data in the furture
            {
                pEncCtrl = NULL;
                pFrameSurface = NULL;
            }
            else
                continue;
        }
        else
        {
            if (MASTER == m_type)
            {
                if (!m_bAccessNextElem)
                    continue;
                it->second->Get(pFrameSurface);
            }
            else
                it->second->Pop(pFrameSurface);
        }
        //printf("[MSDKEncode]-----Get next frame surface successfully\n");
#endif

        if (!m_bInit) {
#ifdef CONFIG_READ_RAW_BUFFER
            sts = InitEncoder(NULL);
#else
            sts = InitEncoder(pFrameSurface);
#endif
            if (MFX_ERR_NONE == sts) {
                if (m_pMeasuremnt)
                {
                    m_pMeasuremnt->GetLock();
                    pipelineinfo einfo;
                    einfo.mElementType  = m_type;
                    einfo.mChannelNum   = MSDKBase::nEncChannels;
                    MSDKBase::nEncChannels++;
                    m_pMeasuremnt->SetElementInfo(ENC_ENDURATION_TIME_STAMP, this, &einfo);
                    m_pMeasuremnt->TimeStpStart(ENC_ENDURATION_TIME_STAMP, this);
                    m_pMeasuremnt->RelLock();
                }

                m_bInit = true;
                H264E_TRACE_INFO("[MSDKEncode]Encoder %p init successfully\n", this);
            } else {
                H264E_TRACE_ERROR("Encode init failed: %d\n", sts);
                return -1;
            }
        }

#ifdef CONFIG_READ_RAW_BUFFER
        int nIndex = GetFreeSurfaceIndex(m_pSurfacePool, m_nSurfaces);      //Find free frame surface slot
        if (MFX_ERR_NOT_FOUND == nIndex)
            continue;
        else
            pFrameSurface = m_pSurfacePool[nIndex];

        sts = LoadRawFrame(m_pSurfacePool[nIndex]);
        if (MFX_ERR_MORE_DATA == sts) {
            if (m_pInputMem->GetDataEof()) {        //No more data in the future
                pEncCtrl = NULL;
                pFrameSurface = NULL;
            } else {
                continue;
            }
        }
        //printf("[MSDKEncode]-----Get the free surface slot and complete load of the raw frame\n");
#endif

        if (m_pMeasuremnt)
        {
            m_pMeasuremnt->GetLock();
            m_pMeasuremnt->TimeStpStart(ENC_FRAME_TIME_STAMP, this);
            m_pMeasuremnt->RelLock();
        }

        for (;;) {
            //Encode a frame asychronously(returns immediately)
            sts = m_pEncode->EncodeFrameAsync(pEncCtrl, pFrameSurface, &m_outputBs, &syncpE);
            //printf("[MSDKEncode]-----EncodeFrameAsync ret code: %d\n", sts);
            if (sts > MFX_ERR_NONE && !syncpE) {        //Repeat the call if warning and no output
                if (MFX_WRN_DEVICE_BUSY == sts)
                    usleep(1000);     //wait if device is busy, then repeat the same call
            } else {
                if (sts > MFX_ERR_NONE && syncpE)
                    sts = MFX_ERR_NONE;     //ignore warnings if output is available
                if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
                    H264E_TRACE_WARNI("[MSDKEncode]-----The size of buffer allocated for encoder is too small\n");
                break;
            }
        }

        if (MFX_ERR_NONE == sts) {
            sts = m_pSession->SyncOperation(syncpE, 60000);
            if (m_pMeasuremnt)
            {
                m_pMeasuremnt->GetLock();
                m_pMeasuremnt->TimeStpFinish(ENC_FRAME_TIME_STAMP, this);
                m_pMeasuremnt->RelLock();
            }

            //check if output is key frame: startP = 0-key frame otherwise not
            //A group of pictures sizeof(GROUP_OF_PICTURE*2) have a key frame
            startP = (++startP)%GROUP_OF_PICTURE;

            //release surface pool slot
            if (MASTER == m_type) {
                m_bAccessNextElem = false;
                it->first->ReleaseSurface();
            } else {
                pFrameSurface->Data.Locked--;
            }

            m_pNotify->OnGetMSDKCodecData(m_outputBs.Data+m_outputBs.DataOffset, m_outputBs.DataLength, (!startP || m_bForceKeyFrame), m_nLogicIndex);
            m_outputBs.DataLength = 0;
            if (m_bForceKeyFrame) {
                m_bForceKeyFrame = false;
                //printf("[MSDKEncode]-----Force key frame successfully\n");
                startP = 0;
            }
        }

        if (!pFrameSurface)
        {
            if (m_pMeasuremnt)
            {
                m_pMeasuremnt->GetLock();
                m_pMeasuremnt->TimeStpFinish(ENC_ENDURATION_TIME_STAMP, this);
                m_pMeasuremnt->RelLock();
            }
            break;
        }
    }

    if (!m_bWantToStop) {
        H264E_TRACE_INFO("[MSDKEncode]Got EOS in Encoder %p\n", this);
        m_bWantToStop = true;
    }
    return 0;
}

#ifdef CONFIG_READ_RAW_BUFFER
mfxStatus MSDKEncode::LoadRawFrame(mfxFrameSurface1* pSurface)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16 w, h, i, pitch;
    mfxU8* ptr;
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxFrameData* pData = &pSurface->Data;

    if (pInfo->CropW > 0 && pInfo->CropH > 0)
    {
        w = pInfo->CropW;
        h = pInfo->CropH;
    }
    else
    {
        w = pInfo->Width;
        h = pInfo->Height;
    }
    pitch = pData->Pitch;
    ptr = pData->Y+pInfo->CropX+pInfo->CropY*pitch;
    if (m_pInputMem->GetBufFullness() < w*h*3/2)
        return MFX_ERR_MORE_DATA;

    //read luminance plane
    for (i = 0; i < h; i++)
        m_pInputMem->ReadSpecSizeData(ptr+i*pitch, w);

    //load UV
    h >>= 1;
    ptr = pData->UV+pInfo->CropX+(pInfo->CropY/2)*pitch;
    for (i = 0; i < h; i++)
        m_pInputMem->ReadSpecSizeData(ptr+i*pitch, w);
    return MFX_ERR_NONE;
}
#endif
