#ifndef COMPTRAINER_H_
#define COMPTRAINER_H_
#include <deque>
#include "MSDKVpp.h"

#define STATIS_DATA_SIZE    50
#define SAMPLE_INTERVAL     20  //ms

//The level of confidence interval as [-1.96, 1.96] in standard normal distribution is 95%
#define STDNORM_CONFIDENCE_INTERVAL_LOWER -1.96
#define STDNORM_CONFIDENCE_INTERVAL_UPPER 1.96

typedef struct _tagSTATIS_NUMERIC_CHARACTER {
    std::deque<double> qDataSet;   //sample data
    //the sample numerical characteristics
    double minimum, maximum;
    double mean_value;
    double variance;
    std::vector<double> vAutoCorrelaCoeff;  //autocorrelation coefficient

    _tagSTATIS_NUMERIC_CHARACTER()
    :minimum(DBL_MAX)
    ,maximum(DBL_MIN)
    ,mean_value(0)
    ,variance(0)
    {
        vAutoCorrelaCoeff.reserve(STATIS_DATA_SIZE-1);
    }
} STATIS_NUMERIC_CHARACTER;

class CompTrainer
{
public:
    CompTrainer(std::vector<MemPool*>& rMemPool, std::list<MSDKDecode*>& rListDecoder, MSDKVpp& rMainVpp);
    ~CompTrainer() {}

public:
    bool Start();
    void Stop();

private:
    static void* ThreadFunc(void* arg);
    void Train();

    void SampleEachStream(size_t nSampleCnt);
    int CalcuStatis(size_t streamIndex);
    bool CheckStability(size_t streamIndex);
    double CalculateRegreCoeff(size_t streamIndex);

    bool TakeSample(size_t streamIndex, double sampleData);
    void CalculateACF(STATIS_NUMERIC_CHARACTER& rStatis, double accum);

private:
    pthread_t m_threadID;
    bool m_bRunning, m_bWantToStop;
    std::vector<MemPool*>& m_rMemPool;
    std::list<MSDKDecode*>& m_rListDecoder;
    MSDKVpp& m_rMainVpp;
    STATIS_NUMERIC_CHARACTER m_rtSeriesStatis[MAX_INPUTSTREAM_NUM];

private:
    CompTrainer(const CompTrainer&);
    CompTrainer& operator=(const CompTrainer&);
};
#endif  //COMPTRAINER_H_
