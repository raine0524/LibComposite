#include <cmath>

#include <stdint.h>
#include "H264FrameParser.h"

const char* AVCFindStartCodeInternal(const char *p, const char *end)
{
    const char *a = p + 4 - ((intptr_t)p & 3);
    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        unsigned int x = *(const unsigned int*)p;
        //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
        //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
        if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }
    return end + 3;
}

const char* AVCFindStartCode(const char *p, const char *end)
{
    const char *out= AVCFindStartCodeInternal(p, end);
    if(p<out && out<end && !out[-1]) out--;
    return out;
}

void AVCParseNalUnits(const char *bufIn, int inSize, char* bufOut, int* outSize)
{
    const char *p = bufIn;
    const char *end = p + inSize;
    const char *nal_start, *nal_end;
    char* pbuf = bufOut;

    *outSize = 0;
    nal_start = AVCFindStartCode(p, end);
    while (nal_start < end)
    {
        while(!*(nal_start++));
        nal_end = AVCFindStartCode(nal_start, end);

        unsigned int nal_size = nal_end - nal_start;
        pbuf = UI32ToBytes(pbuf, nal_size);
        memcpy(pbuf, nal_start, nal_size);
        pbuf += nal_size;
        nal_start = nal_end;
    }
    *outSize = (pbuf - bufOut);
}

UINT Ue(BYTE *pBuff, UINT nLen, UINT &nStartBit)
{
    //Ӌ��0bit�Ă���
    UINT nZeroNum = 0;
    while (nStartBit < nLen * 8)
    {
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8))) //&:��λ�c��%ȡ�N
        {
            break;
        }
        nZeroNum++;
        nStartBit++;
    }
    nStartBit ++;

    //Ӌ��Y��
    DWORD dwRet = 0;
    for (UINT i=0; i<nZeroNum; i++)
    {
        dwRet <<= 1;
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
        {
            dwRet += 1;
        }
        nStartBit++;
    }
    return (1 << nZeroNum) - 1 + dwRet;
}


int Se(BYTE *pBuff, UINT nLen, UINT &nStartBit)
{
    int UeVal=Ue(pBuff,nLen,nStartBit);
    double k=UeVal;
    int nValue=ceil(k/2);//ceil(2)=ceil(1.2)=cei(1.5)=2.00
    if (UeVal % 2==0)
        nValue=-nValue;
    return nValue;
}


DWORD u(UINT BitCount,BYTE * buf,UINT &nStartBit)
{
    DWORD dwRet = 0;
    for (UINT i=0; i<BitCount; i++)
    {
        dwRet <<= 1;
        if (buf[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
        {
            dwRet += 1;
        }
        nStartBit++;
    }
    return dwRet;
}


bool h264_decode_seq_parameter_set(BYTE * buf,UINT nLen,int &Width,int &Height)
{
    UINT StartBit=0;
    int forbidden_zero_bit=u(1,buf,StartBit);
    int nal_ref_idc=u(2,buf,StartBit);
    int nal_unit_type=u(5,buf,StartBit);
    if(nal_unit_type==7)
    {
        int profile_idc=u(8,buf,StartBit);
        int constraint_set0_flag=u(1,buf,StartBit);//(buf[1] & 0x80)>>7;
        int constraint_set1_flag=u(1,buf,StartBit);//(buf[1] & 0x40)>>6;
        int constraint_set2_flag=u(1,buf,StartBit);//(buf[1] & 0x20)>>5;
        int constraint_set3_flag=u(1,buf,StartBit);//(buf[1] & 0x10)>>4;
        int reserved_zero_4bits=u(4,buf,StartBit);
        int level_idc=u(8,buf,StartBit);

        int seq_parameter_set_id=Ue(buf,nLen,StartBit);

        if( profile_idc == 100 || profile_idc == 110 ||
            profile_idc == 122 || profile_idc == 144 )
        {
            int chroma_format_idc=Ue(buf,nLen,StartBit);
            if( chroma_format_idc == 3 )
                int residual_colour_transform_flag=u(1,buf,StartBit);
            int bit_depth_luma_minus8=Ue(buf,nLen,StartBit);
            int bit_depth_chroma_minus8=Ue(buf,nLen,StartBit);
            int qpprime_y_zero_transform_bypass_flag=u(1,buf,StartBit);
            int seq_scaling_matrix_present_flag=u(1,buf,StartBit);

            int seq_scaling_list_present_flag[8];
            if( seq_scaling_matrix_present_flag )
            {
                for( int i = 0; i < 8; i++ ) {
                    seq_scaling_list_present_flag[i]=u(1,buf,StartBit);
                }
            }
        }
        int log2_max_frame_num_minus4=Ue(buf,nLen,StartBit);
        int pic_order_cnt_type=Ue(buf,nLen,StartBit);
        if( pic_order_cnt_type == 0 )
            int log2_max_pic_order_cnt_lsb_minus4=Ue(buf,nLen,StartBit);
        else if( pic_order_cnt_type == 1 )
        {
            int delta_pic_order_always_zero_flag=u(1,buf,StartBit);
            int offset_for_non_ref_pic=Se(buf,nLen,StartBit);
            int offset_for_top_to_bottom_field=Se(buf,nLen,StartBit);
            int num_ref_frames_in_pic_order_cnt_cycle=Ue(buf,nLen,StartBit);

            int *offset_for_ref_frame=new int[num_ref_frames_in_pic_order_cnt_cycle];
            for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
                offset_for_ref_frame[i]=Se(buf,nLen,StartBit);
            delete [] offset_for_ref_frame;
        }
        int num_ref_frames=Ue(buf,nLen,StartBit);
        int gaps_in_frame_num_value_allowed_flag=u(1,buf,StartBit);
        int pic_width_in_mbs_minus1=Ue(buf,nLen,StartBit);
        int pic_height_in_map_units_minus1=Ue(buf,nLen,StartBit);

        Width=(pic_width_in_mbs_minus1+1)*16;
        Height=(pic_height_in_map_units_minus1+1)*16;

        return true;
    }
    else
        return false;
}

void ParseH264Frame(const char* nalsbuf, int size, char* outBuf, int& outLen,
    char* spsBuf, int& spsSize, char* ppsBuf, int& ppsSize,
    bool& isKeyframe, int& width, int& height)
{
    int tmp_len = 0;

    AVCParseNalUnits(nalsbuf, size, (char*)outBuf, &outLen);

    char* start = outBuf;
    char* end = outBuf + outLen;

    /* look for sps and pps */
    while (start < end)
    {
        unsigned int size = BytesToUI32(start);
        unsigned char nal_type = start[4] & 0x1f;

        if (nal_type == 7 && spsBuf)        /* SPS */
        {
            spsSize = size;
            memcpy(spsBuf, start + 4, spsSize);

            h264_decode_seq_parameter_set((BYTE*)start+4, size, width, height);
        }
        else if (nal_type == 8 && ppsBuf)   /* PPS */
        {
            ppsSize = size;
            memcpy(ppsBuf, start + 4, ppsSize);
        }
        else if (nal_type == 5)
        {
            isKeyframe = true;
        }
        start += size + 4;
    }
}

bool GetWidthHeightFromFrame(const char* frameBuf, int bufLen, int& width, int& height)
{
    const char *p = frameBuf;
    const char *end = p + bufLen;
    const char *nal_start, *nal_end;

    nal_start = AVCFindStartCode(p, end);
    while (nal_start < end)
    {
        while(!*(nal_start++));

        nal_end = AVCFindStartCode(nal_start, end);

        unsigned int nal_size = nal_end - nal_start;
        unsigned char nal_type = nal_start[0] & 0x1f;

        if (nal_type == 7)  // sps buf
        {
            h264_decode_seq_parameter_set((BYTE*)nal_start, nal_size, width, height);
            return true;
        }
        nal_start = nal_end;
    }
    return false;
}
