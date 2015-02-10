#include "CompTrainer.h"
#include <numeric>
#include <math.h>

CompTrainer::CompTrainer(std::vector<MemPool*>& rMemPool, std::list<MSDKDecode*>& rListDecoder, MSDKVpp& rMainVpp)
:m_threadID(-1)
,m_bRunning(false)
,m_bWantToStop(false)
,m_rMemPool(rMemPool)
,m_rListDecoder(rListDecoder)
,m_rMainVpp(rMainVpp)
{
}

bool CompTrainer::Start()
{
    if (!m_bRunning)
    {
        bool bRun = (pthread_create(&m_threadID, NULL, ThreadFunc, this) == 0);
        if (!bRun)
            FRMW_TRACE_ERROR("[CompTrainer]Trainer %p start thread failed\n", this);
        else
            m_bRunning = true;
        return bRun;
    }
    return true;
}

void CompTrainer::Stop()
{
    if (m_bRunning)
    {
        m_bWantToStop = true;
        pthread_join(m_threadID, NULL);
        m_bRunning = false;
    }
}

void* CompTrainer::ThreadFunc(void* arg)
{
    CompTrainer* pTrainer = static_cast<CompTrainer*>(arg);
    pTrainer->Train();
    return NULL;
}

void CompTrainer::Train()
{
    /*First sampling for half a cycle(Time that sampling one set with size of STATIS_DATA_SIZE makes up of one cycle)
     *for each stream, and this operation is able to buffer the real stream.More important is that the trainer
     *calculate the sample which contains both the most recent data and outdated data every once half a cycle, the
     *sample what has mentioned above can reflect the trend and change more accurately.In one word, we always
     *hope that we can take less computation cost to obtain a better balanced effect.
     *By the way, if we ignore the computation cost(the computation in fact would cost not much time as the size of
     *the data set is not very large), we can consider the sampling procedure as a periodical behavior approximately.
     *The latter can maximum reduce the sampling error.
     */
    bool bStable;
    double max_regre_coeff;
    SampleEachStream(STATIS_DATA_SIZE/2);
    while (!m_bWantToStop)
    {
        bStable = true;
        max_regre_coeff = DBL_MIN;

        SampleEachStream(STATIS_DATA_SIZE/2);
        for (size_t i = 0; i < m_rMemPool.size(); i++)
        {
            CalcuStatis(i);
            if (!CheckStability(i))
            {
                bStable = false;
                double regre_coeff = CalculateRegreCoeff(i);
                if (regre_coeff > max_regre_coeff)
                    max_regre_coeff = regre_coeff;
            }
            usleep(1*1000); //sleep 1ms
        }
        if (!bStable)
            m_rMainVpp.TrainSleepInterval(max_regre_coeff);
    }
}

void CompTrainer::SampleEachStream(size_t nSampleCnt)
{
    for (size_t i = 0; i < nSampleCnt; i++)
    {
        for (size_t j = 0; j < m_rMemPool.size(); j++)
            TakeSample(j, m_rMemPool[j]->GetBufFullness());
        usleep(SAMPLE_INTERVAL*1000);
    }
}

bool CompTrainer::TakeSample(size_t streamIndex, double sampleData)
{
    if (streamIndex >= MAX_INPUTSTREAM_NUM)
    {
        FRMW_TRACE_ERROR("[CompTrainer]streamIndex %d is out of range, sample failed\n", streamIndex);
        return false;
    }

    STATIS_NUMERIC_CHARACTER& rStatis = m_rtSeriesStatis[streamIndex];
    if (rStatis.qDataSet.size() < STATIS_DATA_SIZE)
        rStatis.qDataSet.push_back(sampleData);
    else
    {
        rStatis.qDataSet.pop_front();
        rStatis.qDataSet.push_back(sampleData);
    }

    if (sampleData > rStatis.maximum)
        rStatis.maximum = sampleData;
    if (sampleData < rStatis.minimum)
        rStatis.minimum = sampleData;
    return true;
}

int CompTrainer::CalcuStatis(size_t streamIndex)
{
    if (streamIndex >= MAX_INPUTSTREAM_NUM)
    {
        FRMW_TRACE_ERROR("[CompTrainer]streamIndex %d is out of range\n", streamIndex);
        return -1;
    }

    STATIS_NUMERIC_CHARACTER& rStatis = m_rtSeriesStatis[streamIndex];
    if (rStatis.qDataSet.size() < STATIS_DATA_SIZE)
    {
        FRMW_TRACE_WARNI("[CompTrainer]The dataset size %d is less than the required\n", rStatis.qDataSet.size());
        return -2;
    }
    rStatis.mean_value = std::accumulate(rStatis.qDataSet.begin(), rStatis.qDataSet.end(), 0.0)/rStatis.qDataSet.size();
    double accum = 0.0;
    for (std::deque<double>::iterator it = rStatis.qDataSet.begin(); it != rStatis.qDataSet.end(); it++)
        accum += ((*it)-rStatis.mean_value)*((*it)-rStatis.mean_value);
    /*divide n-1 instead of n allow us to use the smaller sample set to approach the population standard deviation
     *more accurately, this is the so-called unbiased estimation
     */
    rStatis.variance = accum/(rStatis.qDataSet.size()-1);
    CalculateACF(rStatis, accum);
}

void CompTrainer::CalculateACF(STATIS_NUMERIC_CHARACTER& rStatis, double accum)
{
    for (size_t k = 1; k < rStatis.qDataSet.size(); k++)
    {
        double var = 0.0;
        for (size_t t = 0; t < rStatis.qDataSet.size()-k; t++)
            var += (rStatis.qDataSet[t]-rStatis.mean_value)*(rStatis.qDataSet[t+k]-rStatis.mean_value);
        rStatis.vAutoCorrelaCoeff.push_back(var/accum);
    }
}

/*The sequence is generated by the random process, and we assume that the sequence doesn't exist the
 *sequence correlation, so the sequence can be supposed as a white noise.
 */
bool CompTrainer::CheckStability(size_t streamIndex)
{
    STATIS_NUMERIC_CHARACTER& rStatis = m_rtSeriesStatis[streamIndex];
    double lower_limit = STDNORM_CONFIDENCE_INTERVAL_LOWER*sqrt(1.0/STATIS_DATA_SIZE);
    double upper_limit = STDNORM_CONFIDENCE_INTERVAL_UPPER*sqrt(1.0/STATIS_DATA_SIZE);
    for (std::vector<double>::iterator it = rStatis.vAutoCorrelaCoeff.begin();
         it != rStatis.vAutoCorrelaCoeff.end(); it++) {
        if ((*it) < lower_limit || (*it) > upper_limit)
            return false;   //statistical significance
    }
    return true;
}

/*The reason why only get the regression coefficient of the fitting linear regression equation instead of the
 *analytical expression, that is we only need to get the measure of the tendency
 */
double CompTrainer::CalculateRegreCoeff(size_t streamIndex)
{
    STATIS_NUMERIC_CHARACTER& rStatis = m_rtSeriesStatis[streamIndex];
    double alpha = 0.0;
    for (size_t i = 0; i < STATIS_DATA_SIZE; i++)
        alpha += (i+1)*rStatis.qDataSet[i];

    double beta = 0.0;
    for (size_t i = 0; i < STATIS_DATA_SIZE; i++)
        beta += (i+1)*(i+1);

    double theta = (1+STATIS_DATA_SIZE)*STATIS_DATA_SIZE/2;
    double regre_coeff = (alpha-alpha/STATIS_DATA_SIZE)/(beta-theta*theta/STATIS_DATA_SIZE);
    FRMW_TRACE_INFO("[CompTrainer]The regression coefficient is %lf\n", regre_coeff);
    return regre_coeff;
}
