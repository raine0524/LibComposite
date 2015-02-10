#ifndef RLANGPLOT_H_
#define RLANGPLOT_H_

#define CSTACK_DEFNS

#include <deque>
#include "R.h"
#include "Rdefines.h"
#include "Rinterface.h"
#include "Rembedded.h"

#define MAX_COMBINATION_NUM 4

#define RLANG_FUNC_NOT_EXIST(ret_code, close_pro, pro_code) \
    do  \
    {   \
        if (close_pro)  \
            UNPROTECT(pro_code);    \
        return ret_code;    \
    } while(0)  \

typedef enum {
    DIST_UNIF = 0,
    DIST_NORM,
    DIST_EXP
} CRAN_DIST;

typedef enum {
    COLOR_RED = 0,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_BLACK,
    COLOR_RAINBOW
} CRAN_COLOR;

typedef enum {
    POINT_SHAPE_DEFAULT = 0,
    POINT_SHAPE_DOT,
    POINT_SHAPE_TRIANGLE,
    POINT_SHAPE_PLUSMARK,
    POINT_SHAPE_XMARK,
    POINT_SHAPE_DIAMOND
} CRAN_POINT_SHAPE;

typedef enum {
    LINE_DEFAULT = 0,
    LINE_SOLID,
    LINE_IMAGINARY,
    LINE_DOTTED
} CRAN_LINE_MODE;

class RLangPlot
{
public:
    RLangPlot() : m_bInit(false), m_nCombCnt(0), m_nVecSlots(0) {}
    ~RLangPlot() {}

public:
    int Init_R(size_t nCombCnt, size_t nVecSlots);
    void End_R();

    SEXP GenerateRandom(CRAN_DIST dist, R_xlen_t num);
    /*SetLayout set the layout of multi-pictures which are created by the
     *plot command, otherwise every other plot will cover the previous
     *picture, the upper limit of combination is 4, of course you can
     *customized the function for more count
     *return 0 means exec successfullly otherwise not
    */
    int SetLayout();
    void SetPlotParam(CRAN_POINT_SHAPE point, CRAN_LINE_MODE line);
    int AppendNumeric(size_t vec_index, double data);
    /* return 0 means succ otherwise not */
    int Plot(size_t vec_index, CRAN_COLOR col, const char* pYLable);

private:
    /*This routinue is used to check whether the function specified by
     *`pFuncName` exists in the global envrionment of R
     *return 1 means exist while 0 not
    */
    int CheckFunc(const char* pFuncName, SEXP& fun);
    SEXP EvalString(SEXP e);

    SEXP GetSEXPColor(CRAN_COLOR col);
    int SetParPoint(CRAN_POINT_SHAPE point);
    int SetParLine(CRAN_LINE_MODE line);

    RLangPlot(const RLangPlot&);
    RLangPlot& operator=(const RLangPlot&);

private:
    bool m_bInit;
    size_t m_nCombCnt, m_nVecSlots;
    std::deque<double> m_vec[MAX_COMBINATION_NUM];
};
#endif  //RLANGPLOT_H_
