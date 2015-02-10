#include "MSDKDecode.h"
#include <time.h>
#include <stdlib.h>

#ifdef CONFIG_WRITE_RAW_BUFFER
MSDKDecode::MSDKDecode(MSDKCodecNotify& rNotify)
#else
MSDKDecode::MSDKDecode()
#endif
:MSDKDecodeVpp(ELEMENT_DECODER)
,m_pDecode(NULL)
,m_pInputMem(NULL)
,m_pLastSurface(NULL)
,m_bFirstNotify(true)
,m_bFullStage(false)
,m_nFrameRateExtN(0)
,m_nFrameRateExtD(0)
,m_nSelfFrameRate(0)
,m_nCompFrameRate(0)
,m_nQuotients(0)
,m_nRemainder(0)
,m_nSurplusCompens(0)
,m_nCycleOffset(0)
#ifdef CONFIG_WRITE_RAW_BUFFER
,m_rNotify(rNotify)
#endif
{
#ifdef CONFIG_WRITE_RAW_BUFFER
    m_pRawBuffer = new mfxU8[1920*1080*3/2];
#endif
	memset(&m_inputBs, 0, sizeof(m_inputBs));
}

MSDKDecode::~MSDKDecode()
{
	if (m_pDecode)
	{
	    //Session closed automatically on destruction
		m_pDecode->Close();
		delete m_pDecode;
		m_pDecode = NULL;
	}
#ifdef CONFIG_WRITE_RAW_BUFFER
	if (m_pRawBuffer)
	{
	    delete[] m_pRawBuffer;
	    m_pRawBuffer = NULL;
	}
#endif
}

bool MSDKDecode::SetDecodeParam(VADisplay* pVaDpy, MemPool* pMem, Measurement* pMeasuremnt, bool bUseMeasure)
{
    if (!pMem)
    {
        H264D_TRACE_ERROR("Invalid memory pool, set parameters failed\n");
        return false;
    }
#ifdef CONFIG_USE_MFXALLOCATOR
    if (!CreateSessionAllocator(pVaDpy))
        return false;
#else
    MFXVideoSession* pSession = CreateSession(*pVaDpy);
    if (!pSession)
    {
        H264D_TRACE_ERROR("[MSDKDecode]-----Create session failed\n");
        return false;
    }
    m_pSession = pSession;
#endif

    m_pInputMem     = pMem;
    if (bUseMeasure)
        m_pMeasuremnt   = pMeasuremnt;
    m_pDecode       = new MFXVideoDECODE(*m_pSession);

    m_mfxVideoParam.mfx.CodecId = MFX_CODEC_AVC;
    m_mfxVideoParam.mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;
    m_mfxVideoParam.mfx.CodecLevel = MFX_LEVEL_UNKNOWN;
    m_mfxVideoParam.mfx.ExtendedPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
#ifdef CONFIG_USE_MFXALLOCATOR
    m_mfxVideoParam.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    H264D_TRACE_INFO("----------------decoder using video memory\n");
#else
    m_mfxVideoParam.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    H264D_TRACE_INFO("----------------decoder using system memory\n");
#endif
    m_mfxVideoParam.AsyncDepth = 1;

    m_inputBs.MaxLength = m_pInputMem->GetTotalBufSize();
    m_inputBs.Data = m_pInputMem->GetReadPtr();
    //Validate video decode parameters
    mfxStatus sts = m_pDecode->Query(&m_mfxVideoParam, &m_mfxVideoParam);
    if (sts >= MFX_ERR_NONE) {
        H264D_TRACE_INFO("[MSDKDecode]-----Create decoder and parameter is set correctly %d\n", sts);
        return true;
    } else {
        return false;
    }
}

void MSDKDecode::SetCompFrameRate(mfxU32 nCompFrameRate)
{
    if (m_nSelfFrameRate != nCompFrameRate)
        m_nCompFrameRate = nCompFrameRate-1;    //reduce 1 since of existing of frame rate error

    if (m_nSelfFrameRate < m_nCompFrameRate)
    {
        mfxU32 nCompensate = m_nCompFrameRate-m_nSelfFrameRate;
        m_nQuotients = nCompensate/m_nSelfFrameRate;
        m_nRemainder = nCompensate%m_nSelfFrameRate;
    }
    H264D_TRACE_INFO("[MSDKDecode]-----Set composite frame rate %d while self frame rate is %d\n", m_nCompFrameRate, m_nSelfFrameRate);
}

void MSDKDecode::UpdateBitStream()
{
    m_inputBs.Data = m_pInputMem->GetReadPtr();
    m_inputBs.DataOffset = 0;
    m_inputBs.DataLength = m_pInputMem->GetFlatBufFullness();
}

#ifndef CONFIG_USE_MFXALLOCATOR
mfxStatus MSDKDecode::AllocFrames(mfxFrameAllocRequest* pRequest)
{
    //Allocate surfaces for decoder
    mfxU16 width = (mfxU16)MSDK_ALIGN32(pRequest->Info.Width);
    mfxU16 height = (mfxU16)MSDK_ALIGN32(pRequest->Info.Height);
    mfxU32 nSurfSize = width*height*12/8;       //NV12 format is a 12 bits per pixel format
    m_pSurfaceBuffers = (mfxU8*)new mfxU8[nSurfSize*m_nSurfaces];

    //Allocate surface headers(mfxFrameSurface1) for decoder
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

mfxStatus MSDKDecode::InitDecoder()
{
    if (m_pMeasuremnt)
    {
        m_pMeasuremnt->GetLock();
        m_pMeasuremnt->TimeStpStart(DEC_INIT_TIME_STAMP, this);
        m_pMeasuremnt->RelLock();
    }

    //retrieve decoding initialization parameters from the bitstream
    UpdateBitStream();
    mfxStatus sts = m_pDecode->DecodeHeader(&m_inputBs, &m_mfxVideoParam);
    //printf("[MSDKDecode]------DecodeHeader %d\n", sts);
    UpdateMemPool();

    if (MFX_ERR_MORE_DATA == sts) {
        usleep(1000);
        return sts;
    } else if (MFX_ERR_NONE == sts) {
        m_nFrameRateExtN = m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN;
        m_nFrameRateExtD = m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD;
        m_nSelfFrameRate = m_nFrameRateExtN/m_nFrameRateExtD;
        H264D_TRACE_INFO("[MSDKDecode]-----The DecodeHeader prompt that frame rate is %d and its resolution is %dx%d\n",
                         m_nFrameRateExtN/m_nFrameRateExtD, m_mfxVideoParam.mfx.FrameInfo.Width, m_mfxVideoParam.mfx.FrameInfo.Height);

        mfxFrameAllocRequest decRequest;
        //obtain the number of working frames required to reorder output frames
        sts = m_pDecode->QueryIOSurf(&m_mfxVideoParam, &decRequest);
        //printf("[MSDKDecode]------QueryIOSurf %d\n", sts);
        if ((MFX_ERR_NONE != sts) && (MFX_WRN_PARTIAL_ACCELERATION != sts)) {
            H264D_TRACE_ERROR("QueryIOSurf return %d, failed\n", sts);
            return sts;
        }

        m_nSurfaces = decRequest.NumFrameSuggested+10;
        H264D_TRACE_INFO("[MSDKDecode]-----Get memory type %x by pDecode->QueryIOSurf()\n", decRequest.Type);
        sts = AllocFrames(&decRequest);

        //Initialize the Media SDK decoder
        sts = m_pDecode->Init(&m_mfxVideoParam);
        //printf("[MSDKDecode]------Init %d\n", sts);

        if (m_pMeasuremnt)
        {
            m_pMeasuremnt->GetLock();
            m_pMeasuremnt->TimeStpFinish(DEC_INIT_TIME_STAMP, this);
            m_pMeasuremnt->RelLock();
        }

        if ((sts != MFX_ERR_NONE) && (MFX_WRN_PARTIAL_ACCELERATION != sts)) {
            return sts;
        } else {
            return sts;
        }
    }
}

int MSDKDecode::HandleProcess()
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint syncpD;
    mfxFrameSurface1* pmfxOutSurface = NULL;
    mfxBitstream* pBitstream = &m_inputBs;
    int nIndex;

    struct timeval cur_tm;
    gettimeofday(&cur_tm, NULL);
    srand(static_cast<unsigned int>(cur_tm.tv_usec));

    while (!m_bWantToStop) {
        usleep(1000);

        if (!m_pInputMem->GetBufFullness())     //No data
        {
            //printf("[MSDKDecode]-----There's no more data, just continue or exit the main loop\n");
            if (m_pInputMem->GetDataEof())      //No more data in the furture
                pBitstream = NULL;
            else
                continue;
        }

        if (!m_bInit) {
            //Init decoder, just run once after get the compressed bitstream
            sts = InitDecoder();
            if (MFX_ERR_MORE_DATA == sts) {
                if (pBitstream)
                    continue;
                else
                    return -1;
            } else if (MFX_ERR_NONE == sts) {
                if (m_pMeasuremnt)
                {
                    m_pMeasuremnt->GetLock();
                    pipelineinfo einfo;
                    einfo.mElementType  = m_type;
                    einfo.mChannelNum   = MSDKBase::nDecChannels;
                    MSDKBase::nDecChannels++;
                    m_pMeasuremnt->SetElementInfo(DEC_ENDURATION_TIME_STAMP, this, &einfo);
                    m_pMeasuremnt->TimeStpStart(DEC_ENDURATION_TIME_STAMP, this);
                    m_pMeasuremnt->RelLock();
                }

                m_bInit = true;
                H264D_TRACE_INFO("[MSDKDecode]Decoder %p init successfully\n", this);
            } else {
                H264D_TRACE_ERROR("Decode create failed: %d\n", sts);
                return -1;
            }
        }

        nIndex = GetFreeSurfaceIndex(m_pSurfacePool, m_nSurfaces);      //Find free frame surface slot
        if (MFX_ERR_NOT_FOUND == nIndex) {
            //printf("[MSDKDecode %p]-----Cann't get free surface slot, check if not free\n", this);
            continue;
        }

        if (m_pMeasuremnt)
        {
            m_pMeasuremnt->GetLock();
            m_pMeasuremnt->TimeStpStart(DEC_FRAME_TIME_STAMP, this);
            m_pMeasuremnt->RelLock();
        }

        if (pBitstream)
            UpdateBitStream();

        //Decode a frame asychronously(returns immediately)
        sts = m_pDecode->DecodeFrameAsync(pBitstream, m_pSurfacePool[nIndex], &pmfxOutSurface, &syncpD);
        //printf("[MSDKDecode %p]-----DecodeFrameAsync, ret code: %d\n", this, sts);
        if (!syncpD && sts < MFX_ERR_NONE && sts != MFX_ERR_MORE_SURFACE && sts != MFX_ERR_MORE_DATA)
            H264D_TRACE_ERROR("-----Decoder return error code: %d\n", sts);

        if (sts > MFX_ERR_NONE && syncpD)
            sts = MFX_ERR_NONE;     //Ignore warnings if output is available

        if (MFX_ERR_NONE == sts)
        {
            sts = m_pSession->SyncOperation(syncpD, 60000);
            if (MFX_ERR_NONE == sts)
            {
                if (m_pMeasuremnt)
                {
                    m_pMeasuremnt->GetLock();
                    m_pMeasuremnt->TimeStpFinish(DEC_FRAME_TIME_STAMP, this);
                    m_pMeasuremnt->RelLock();
                }
                WriteOutSurface(pmfxOutSurface);
            }
            else
                pmfxOutSurface->Data.Locked--;
        }

        if (pBitstream) {
            UpdateMemPool();
        } else {
            if (m_pMeasuremnt)
            {
                m_pMeasuremnt->GetLock();
                m_pMeasuremnt->TimeStpFinish(DEC_ENDURATION_TIME_STAMP, this);
                m_pMeasuremnt->RelLock();
            }

            m_bEndOfStream = true;
            break;
        }
    }

    if (!m_bWantToStop) {
        H264D_TRACE_INFO("[%p]This video stream finish decoding and will be stopped.\n", this);
        m_bWantToStop = true;
    }
    return 0;
}

void MSDKDecode::WriteOutSurface(mfxFrameSurface1* pmfxOutSurface)
{
    if (m_bFirstNotify)
    {
        if (m_mfxRingBuf.IsFull() || MFX_ERR_NOT_FOUND == GetFreeSurfaceIndex(m_pSurfacePool, m_nSurfaces))
        {
            m_bFullStage = true;
            m_bFirstNotify = false;
        }
    }

    WaitAndPushSurface(pmfxOutSurface);
    if (m_nCompFrameRate)
        MakeCompensate(pmfxOutSurface);
    //save the last frame surface for the stream interrupt exception
    {
        Locker<Mutex> l(m_mutex);
        if (m_pLastSurface)
            m_pLastSurface->Data.Locked--;
        m_pLastSurface = pmfxOutSurface;
        m_pLastSurface->Data.Locked++;
    }
    //printf("[MSDKDecode]-----pmfxOutSurface %p and it's locks is %d\n", pmfxOutSurface, pmfxOutSurface->Data.Locked);

#ifdef CONFIG_WRITE_RAW_BUFFER
    mfxU32 nLen = WriteRawFrameToBuffer(pmfxOutSurface);
    m_rNotify.OnGetProcessedData(m_pRawBuffer, nLen);
#endif
}

void MSDKDecode::RequestLastSurface()
{
    Locker<Mutex> l(m_mutex);
    WaitAndPushSurface(m_pLastSurface);
}

void MSDKDecode::MakeCompensate(mfxFrameSurface1* pmfxOutSurface)
{
    for (size_t i = 0; i < m_nQuotients; i++)
        WaitAndPushSurface(pmfxOutSurface);

    if (m_nRemainder)
    {
        if (!m_nCycleOffset)
        {
            m_nCycleOffset = m_nSelfFrameRate;
            m_nSurplusCompens = m_nRemainder;
        }
        m_nCycleOffset--;
        if (GeneRandom(0, m_nCycleOffset) < m_nSurplusCompens)
        {
            WaitAndPushSurface(pmfxOutSurface);
            m_nSurplusCompens--;
        }
    }
}

void MSDKDecode::WaitAndPushSurface(mfxFrameSurface1* pmfxOutSurface)
{
    //may be blocked here!
    while (m_mfxRingBuf.IsFull())
    {
        //printf("[MSDKDecode]-----Ring buffer for %p is full, RingBuf elem num is %d\n", this, m_mfxRingBuf.DataCount());
        usleep(1000);
    }
    pmfxOutSurface->Data.Locked++;
    m_mfxRingBuf.Push(pmfxOutSurface);
}

#ifdef CONFIG_WRITE_RAW_BUFFER
mfxU32 MSDKDecode::WriteRawFrameToBuffer(mfxFrameSurface1* pSurface)
{
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxFrameData* pData = &pSurface->Data;
    mfxU32 nLen = 0;
    mfxU32 i, h, w, pitch;
    mfxU8* ptr, *pRawBuffer = m_pRawBuffer;

    if (pInfo->CropH && pInfo->CropW > 0) {
        w = pInfo->CropW;
        h = pInfo->CropH;
    } else {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    pitch = pData->Pitch;
    ptr = pData->Y+pInfo->CropX+(pInfo->CropY)*pitch;
    for (i = 0; i < h; i++)
    {
        memcpy(pRawBuffer, ptr+i*pitch, w);
        pRawBuffer += w;
    }
    //write UV data
    h >>= 1;
    ptr = pData->UV+pInfo->CropX+(pInfo->CropY>>1)*pitch;
    for (i = 0; i < h; i++)
    {
        memcpy(pRawBuffer, ptr+i*pitch, w);
        pRawBuffer += w;
    }
    return (pRawBuffer-m_pRawBuffer);
}
#endif
