//#define TEST_MEMORYLEAK       //test memory leak need receive real stream
//#define TEST_UNIT
#define TEST_INTEGRATION

#if !defined(TEST_UNIT) && !defined(TEST_INTEGRATION)
#error "must define one macro of TEST_UNIT and TEST_INTEGRATION"
#endif

#define CONFIG_ENABLE_RLANG

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "CompEngine.h"
#include "H264FrameParser.h"

#ifdef TEST_MEMORYLEAK
#include "NETEC/NETEC_Setting.h"
#include "NETEC/NETEC_Node.h"
#include "NETEC/NETEC_Core.h"
#include "NETEC/NETEC_MediaReceiver.h"
#include "VIDEC/VIDEC_Header.h"
#include "HPCompositeDefine.h"
#endif

#define PAGE_SIZE 4096

bool g_bWantToStop = false;
void sig_callback(int signo)
{
    if (2 == signo)
    {
        printf("Get a signal -- SIGINT");
        g_bWantToStop = true;
    }
}

class MSDKCodec : public MSDKCodecNotify
{
public:
	MSDKCodec() : m_pFile(NULL), m_pLog(NULL) {
	    m_pFile = fopen("testCodec.h264", "wb");
	    //m_pLog = fopen("CheckKeyFrame.log", "w");
	    //fprintf(m_pLog, "Check result: \n");
	    m_pStubBuf = new char[1920*1080*2];
	}
	~MSDKCodec() {
		if (m_pFile) {
			fclose(m_pFile);
			m_pFile = NULL;
		}
		if (m_pLog) {
            fclose(m_pLog);
            m_pLog = NULL;
		}
		if (m_pStubBuf) {
            delete []m_pStubBuf;
            m_pStubBuf = NULL;
        }
	}
	virtual void OnGetMSDKCodecData(unsigned char* pData, int nLen, bool bKeyFrame, int nIndex);

private:
	FILE* m_pFile, *m_pLog;
	char* m_pStubBuf;
};

void MSDKCodec::OnGetMSDKCodecData(unsigned char* pData, int nLen, bool bKeyFrame, int nIndex)
{
    static int nTotal = 0;
    nTotal++;
    if (0 == nTotal % 512)
        printf("[OnGetMSDKCodecData]-----Get total packets %d %p %d\n", nTotal, pData, nLen);
    int outLen = 1920*1080*2, spsSize = 0, ppsSize = 0;
    bool isKeyframe = false;
    int width = 0, height = 0;
    ParseH264Frame((const char*)pData, nLen, m_pStubBuf, outLen, NULL,
                   spsSize, NULL, ppsSize, isKeyframe, width, height);
    if (isKeyframe)
    {
        printf("[MSDKCodec]-----Get key frame: %p %d\tseq=%d\n", pData, nLen, nTotal);
    }
    if (bKeyFrame != isKeyframe)
    {
        printf("[MSDKCodec]@@@@@@@@@@@@@@@@@@@@@@@check wrong!! seq: %d\tMSDKEncode %d\tParseH264Frame %d %p %d\n", nTotal, bKeyFrame, isKeyframe, pData, nLen);
        //fprintf(m_pLog, "Check wrong, seq: %d\n", nTotal);
    }
	fwrite(pData, 1, nLen, m_pFile);
}

#ifdef TEST_MEMORYLEAK
class HPStreamParse
:public NETEC_MediaReceiverCallback
{
public:
#ifdef TEST_UNIT
	HPStreamParse(MemPool* mp) : m_pEngine(NULL), m_pInputMem(mp), m_pIAVReceiver(NULL), m_bGetData(false) {}
#endif
#ifdef TEST_INTEGRATION
    HPStreamParse(CompEngine* pEngine)
    :m_pEngine(pEngine)
    ,m_pInputMem(NULL)
    ,m_pIAVReceiver(NULL)
    ,m_bGetData(false)
    ,m_pFile(NULL)
    { m_pFile = fopen("record.h264", "wb"); }
#endif
	virtual ~HPStreamParse() { if (m_pFile) { fclose(m_pFile); m_pFile = NULL; }}

public:
	bool Connect(const TASK_STREAM& stTaskStream);
	void Release();
    virtual void OnNETEC_MediaReceiverCallbackAudioPacket(unsigned char* pData, int nLen) {}
    virtual void OnNETEC_MediaReceiverCallbackVideoPacket(unsigned char* pData, int nLen);

private:
    CompEngine* m_pEngine;
    MemPool* m_pInputMem;
	NETEC_MediaReceiver* m_pIAVReceiver;
	FILE* m_pFile;
	bool m_bGetData;
};

bool HPStreamParse::Connect(const TASK_STREAM& stTaskStream)
{
    if (NULL == m_pIAVReceiver)
        m_pIAVReceiver = NETEC_MediaReceiver::Create(*this);
    if (0 != m_pIAVReceiver->Open(stTaskStream.strNodeID.c_str(),
                                  stTaskStream.strNATAddr.c_str(),
                                  stTaskStream.usLocalPort,
                                  stTaskStream.strLocalAddr.c_str(),
                                  stTaskStream.usLocalPort,
                                  stTaskStream.strMCUID.c_str(),
                                  stTaskStream.strMCUAddr.c_str(),
                                  stTaskStream.usMCUPort))
    {
        printf("[HPCOMP]m_pIAVReceiver->Open failed.\n");
        return false;
    }
    m_pIAVReceiver->SetAudioID(stTaskStream.ulAudioID);
    m_pIAVReceiver->SetVideoID(stTaskStream.ulVideoID);
    m_pIAVReceiver->StartAudio();
    m_pIAVReceiver->StartVideo();
    m_pIAVReceiver->RequestKeyFrame();
    printf("[HPStreamParse]Connect to node successfully, start to receive video data...\n");
    return true;
}

void HPStreamParse::Release()
{
    if (m_pIAVReceiver)
    {
        m_pIAVReceiver->EnableAudio(0);
        m_pIAVReceiver->EnableVideo(0);
        m_pIAVReceiver->StopAudio();
        m_pIAVReceiver->StopVideo();
        m_pIAVReceiver->Close();
        delete m_pIAVReceiver;
        m_pIAVReceiver = NULL;
    }
}

void HPStreamParse::OnNETEC_MediaReceiverCallbackVideoPacket(unsigned char* pData, int nLen)
{
    static int nTotal = 0;
    nTotal++;
    if (0 == nTotal % 512)
        printf("[OnNETEC_MediaReceiver]-----Get total packets %d\n", nTotal);
    if (!m_bGetData)
    {
        printf("[HPStreamParse]Receiving the data now, wait to be decoded\n");
        if (VIDEC_HEADER_EXT_GET_KEYFRAME(pData))
            printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Really key frame!\n");
        m_bGetData = true;
    }
    if (true == VIDEC_HEADER_EXT_IS_VALID(pData))
    {
        int nHeaderLen = VIDEC_HEADER_EXT_GET_LEN(pData);    //discard the header of private protocol
        fwrite(pData+nHeaderLen, 1, nLen-nHeaderLen, m_pFile);
#ifdef TEST_INTEGRATION
        m_pEngine->FeedData(pData+nHeaderLen, nLen-nHeaderLen, 0);
#endif
#ifdef TEST_UNIT
        while (true){
            int mempool_freeflat = m_pInputMem->GetFreeFlatBufSize();
            if (mempool_freeflat < nLen-nHeaderLen) {
                usleep(1000);
                continue;
			}
			unsigned char* mempool_wrptr = m_pInputMem->GetWritePtr();
			memcpy(mempool_wrptr, pData+nHeaderLen, nLen-nHeaderLen);
			m_pInputMem->UpdateWritePtrCopyData(nLen-nHeaderLen);
			break;
        }
#endif
    }
}
#endif

int main(int argc, char* argv[])
{
    signal(SIGINT, sig_callback);
    Trace::SetTraceLevel(SYS_DEBUG);
    Trace::start();

	MSDKCodec writer;
#ifdef TEST_INTEGRATION
    CompEngine* pEngine = new CompEngine(&writer);
    pEngine->Init(1, 1080, 720, 2000, 0, true);
    VppRect vppRect;
    vppRect.x = 200;
    vppRect.y = 200;
    vppRect.w = 400;
    vppRect.h = 400;
    pEngine->SetSingleRect(0, &vppRect);
	pEngine->Start();
#endif

#ifdef TEST_UNIT
    //set codec parameter and create the codec thread
	MemPool* mp = new MemPool;
	mp->init();
	MSDKDecode decoder;
	decoder.SetDecodeParam(va_dpy, mp);

	MSDKVpp vpper(VPP_COMP);
	VppConfig vppCfg;
	vppCfg.comp_num = 1;
	vppCfg.out_width = 1920;
	vppCfg.out_height = 1080;
	vpper.SetVppParam(va_dpy, &vppCfg);
	VppRect rect;
	rect.x = 200;
	rect.y = 200;
	rect.w = 1080;
	rect.h = 720;
	vpper.AddVPPCompRect(&rect);

	MSDKEncode encoder(writer, MASTER);
	encoder.SetEncodeParam(va_dpy, 2000, 0);

	vpper.LinkPrevElement(&decoder);
	encoder.LinkPrevElement(&vpper);
	decoder.Start();
	printf("[TEST_MSDKCODEC]-----Set Decoder's param successfully and start decoding...\n");
	vpper.Start();
	printf("[TEST_MSDKCODEC]-----Set vpper's param successfully and start vpping...\n");
	encoder.Start();
	printf("[TEST_MSDKCODEC]-----Set encoder's param successfully and start encoding...\n");
#endif

#ifdef TEST_MEMORYLEAK
    NETEC_Core::Start();
    NETEC_Setting::SetVideoProtocolType(NETEC_Setting::PT_TCP);
    NETEC_Setting::SetAudioProtocolType(NETEC_Setting::PT_TCP);
    NETEC_Node::SetServerIP("192.168.11.65");
    NETEC_Node::SetServerPort(4222);
    NETEC_Node::Start();

    TASK_STREAM taskStream;
    taskStream.strNodeID    = "0-DBF08730-DBFCEC65";
    taskStream.strDevID     = "danbing244";
    taskStream.strCHLID     = "danbing244_00";
    taskStream.strMCUID     = "MCU-001";
    taskStream.strMCUAddr   = "192.168.11.65";
    taskStream.usMCUPort    = 4222;
    taskStream.strNATAddr   = "192.168.5.244";
    taskStream.strLocalAddr = "192.168.5.244";
    taskStream.usLocalPort  = 4222;
    taskStream.ulAudioID    = 3272600263;
    taskStream.ulVideoID    = 3272600270;
    taskStream.ulWndIndex   = 0;
#ifdef TEST_INTEGRATION
    HPStreamParse streamReceiver(pEngine);
#endif
#ifdef TEST_UNIT
    HPStreamParse streamReceiver(mp);
#endif
    streamReceiver.Connect(taskStream);
#else   //TEST_MEMORYLEAK
    char* input_file = "test.h264";
    FILE* fSource = fopen(input_file, "rb");
    printf("open file %s successfully\n", input_file);
    unsigned char* pBuffer = new unsigned char[PAGE_SIZE];
    while (!feof(fSource) && !g_bWantToStop) {
        usleep(2*1000);
#ifdef TEST_INTEGRATION
        int read_size = fread(pBuffer, 1, PAGE_SIZE, fSource);
        pEngine->FeedData(pBuffer, read_size, 0);
#else   //TEST_INTEGRATION
        int mempool_freeflat = mp->GetFreeFlatBufSize();
        if (!mempool_freeflat)
            continue;

        unsigned char* mempool_wrptr = mp->GetWritePtr();
        int read_size = fread(mempool_wrptr, 1, mempool_freeflat, fSource);
        mp->UpdateWritePtrCopyData(read_size);
#endif
    }
    printf("End of File %s/want to stop\n", input_file);
#ifdef TEST_INTEGRATION
    pEngine->SetDataEos(0);
#else
    mp->SetDataEof(true);
#endif
    delete[] pBuffer;
    fclose(fSource);
#endif

#ifdef TEST_INTEGRATION
#ifdef TEST_MEMORYLEAK
    while (!g_bWantToStop) {
        usleep(2000000);    //sleep 2s
        //engine.ForceKeyFrame(0);
        //printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@engine force key frame!\n");
    }
    pEngine->Stop(STOP_ATONCE, true);
#else
    pEngine->Stop(STOP_DELAY,true);
#endif
#endif

#ifdef TEST_UNIT
    encoder.WaitForStop();
    vpper.WaitForStop();
    decoder.WaitForStop();
    delete mp;
#endif
    printf("[TEST_MSDKCODEC]-----codec finish!\n");

    //cleanup resource
#ifdef TEST_MEMORYLEAK
    streamReceiver.Release();
    NETEC_Node::Stop();
    NETEC_Core::Stop();
    printf("[TEST_MEMORYLEAK]-----release stream and stop network!\n");
#endif

    Trace::stop();
	return EXIT_SUCCESS;
}
