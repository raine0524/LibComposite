/*
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef MEASUREMENT_H_
#define MEASUREMENT_H_

#include <vector>
#include <string>
#include <stdio.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <pthread.h>
#include "trace.h"

#ifdef CONFIG_ENABLE_RLANG
    #include "RLangPlot.h"
#endif

#define MAX_TIMESTAMP_NUM   4096

typedef struct {
    unsigned long start_t;
    unsigned long finish_t;
} timerec;
typedef std::vector<timerec> timestamp;

typedef struct {
    unsigned int mElementType;
    unsigned int mChannelNum;
} pipelineinfo;

typedef struct {
    pipelineinfo pinfo;
    void *id;
    timestamp FrameTimeStp;
    timestamp Enduration;
    timestamp InitStp;
} codecdata;
typedef std::vector<codecdata> codecgroup;

typedef enum {
    ENC_FRAME_TIME_STAMP = 1,   //record time cost of each frame
    VPP_FRAME_TIME_STAMP,
    DEC_FRAME_TIME_STAMP,

    ENC_ENDURATION_TIME_STAMP,  //the enduration of the element
    VPP_ENDURATION_TIME_STAMP,  //stub
    DEC_ENDURATION_TIME_STAMP,

    ENC_INIT_TIME_STAMP,
    VPP_INIT_TIME_STAMP,
    DEC_INIT_TIME_STAMP
} StampType;

typedef enum {
    MEASUREMNT_ERROR_NONE = 0,
    MEASUREMNT_ERROR_CODECDATA,
    MEASUREMNT_FINISHSTP_NOT_SET
} MeasurementError;

class MeasureCallback
{
public:
    virtual const char* GetMemPoolName(unsigned int streamIndex) = 0;
    virtual int GetMemDataSize(unsigned int streamIndex) = 0;
};

class Measurement
{
public:
    Measurement(MeasureCallback& rCallback);
    virtual ~Measurement();

    void StartTime(size_t nPoolCnt);
    unsigned long EndTime();    //unit is millisecond
    //void    Log(unsigned char level, const char* fmt, ...);

    void GetLock() { pthread_mutex_lock(&mutex_bch); }
    void RelLock() { pthread_mutex_unlock(&mutex_bch); }

    void SetElementInfo(StampType st, void *hdl, pipelineinfo *pif);
    int TimeStpStart(StampType st, void *hdl);
    int TimeStpFinish(StampType st, void *hdl);

    //show the performance data, or do something user wanted with the data
    void ShowPerformanceInfo();

public:
    static void GetFmtTime(char *time);

private:
#ifdef CONFIG_ENABLE_RLANG
    static void* ThreadFunc(void* arg);
    void MonitorPlot();
#endif

    void GetCurTime(unsigned long *time);
    unsigned long CalcInterval(timerec *tr);
    codecdata *GetCodecData(StampType st, void *id);

private:
    bool m_bStartTime;
    struct timeval ttime_;

    /*performance data*/
    codecgroup mEncGrp;
    codecgroup mDecGrp;
    codecgroup mVppGrp;
    pthread_mutex_t mutex_bch;
    MeasureCallback& m_rCallback;

    static unsigned int cnt_bench, sum;
    static unsigned long init_enc, init_vpp, init_dec;

#ifdef CONFIG_ENABLE_RLANG
    bool m_bWantToStop, m_bRunning;
    size_t m_nPlotCnt;
    pthread_t m_thread;
    RLangPlot* m_pPlot;
#endif
};
#endif /* _TRACE_H_*/
