#include "RLangPlot.h"

int RLangPlot::Init_R(size_t nCombCnt, size_t nVecSlots)
{
    if (!nCombCnt || nCombCnt > MAX_COMBINATION_NUM)
    {
        Rprintf("The upper limit of layout is %d\n", MAX_COMBINATION_NUM);
        return 0;
    }

    char* defaultArgv[] = {"R", "--silent", "--no-save"};
    Rf_initEmbeddedR(sizeof(defaultArgv)/sizeof(defaultArgv[0]), defaultArgv);
    /* If ignore the followed operation, then R-engine will print error info:
     * "Error: C stack usage is too close to the limit".It's used to disable
     * the C stack check as the R-exts doc say, and it should be set *after*
     * Rf_initialize_R.
     */
    R_CStackLimit = -1;

    m_nCombCnt  = nCombCnt;
    m_nVecSlots = nVecSlots;
    m_bInit = true;
    Rprintf("Startup R-engine successfully!\n");
    return 1;
}

void RLangPlot::End_R()
{
    if (m_bInit)
    {
        Rf_endEmbeddedR(0);
        m_nCombCnt = 0;
        m_bInit = false;
        Rprintf("Close R-engine\n");
    }
}

SEXP RLangPlot::GenerateRandom(CRAN_DIST dist, R_xlen_t num)
{
    if (!m_bInit)
    {
        Rprintf("It's forbidden to generate rand before init\n");
        return R_NilValue;
    }

    double (*rand)() = NULL;
    switch(dist)
    {
    case DIST_UNIF: rand = unif_rand;   break;
    case DIST_NORM: rand = norm_rand;   break;
    case DIST_EXP:  rand = exp_rand;    break;
    }

    SEXP arg;
    PROTECT(arg = NEW_NUMERIC(num));

    GetRNGstate();
    for (int i = 0; i < LENGTH(arg); i++)
        REAL(arg)[i] = rand();
    PutRNGstate();

    UNPROTECT(1);
    return arg;
}

int RLangPlot::AppendNumeric(size_t vec_index, double data)
{
    if (!m_bInit || vec_index >= m_nCombCnt)
    {
        Rprintf("Do init first or check the boundary of `vec_index`, append fail\n");
        return 0;
    }

    if (m_nVecSlots)
    {
        if (m_vec[vec_index].size() < m_nVecSlots)
            m_vec[vec_index].push_back(data);
        else
        {
            m_vec[vec_index].pop_front();
            m_vec[vec_index].push_back(data);
        }
    }
    return 1;
}

int RLangPlot::Plot(size_t vec_index, CRAN_COLOR col, const char* pYLable)
{
    if (!m_bInit || vec_index >= m_nCombCnt)
    {
        Rprintf("plot failed before init or out of the boundary\n");
        return -1;
    }

    SEXP e, fun, arg;
    //PROTECT(e = lang2(install("plot"), vec));
    PROTECT(e = allocVector(LANGSXP, 5));
    if (!CheckFunc("plot", fun))
        RLANG_FUNC_NOT_EXIST(-2, true, 1);

    SETCAR(e, fun);
    PROTECT(arg = NEW_NUMERIC(m_vec[vec_index].size()));
    for (size_t i = 0; i < LENGTH(arg); i++)
        REAL(arg)[i] = m_vec[vec_index][i];
    SETCADR(e, arg);

    SETCADDR(e, GetSEXPColor(col));
    SET_TAG(CDR(CDR(e)), install("col"));
    SETCADDDR(e, mkString(pYLable));
    SET_TAG(CDR(CDR(CDR(e))), install("ylab"));
    SETCAD4R(e, mkString("b"));
    SET_TAG(CDR(CDR(CDR(CDR(e)))), install("type"));
    EvalString(e);
    UNPROTECT(1);
    return 0;
}

void RLangPlot::SetPlotParam(CRAN_POINT_SHAPE point, CRAN_LINE_MODE line)
{
    if (!m_bInit)
    {
        Rprintf("Set plot param before init\n");
        return;
    }
    SetParPoint(point);
    SetParLine(line);
    Rprintf("Set the point shape and line mode succ\n");
}

int RLangPlot::SetLayout()
{
    if (!m_bInit)
    {
        Rprintf("Set layout before init\n");
        return -1;
    }

    SEXP e, fun, arg;
    if (2 == m_nCombCnt || 4 == m_nCombCnt)
    {
        PROTECT(e = allocVector(LANGSXP, 2));
        if (!CheckFunc("par", fun))
            RLANG_FUNC_NOT_EXIST(-2, true, 1);
        SETCAR(e, fun);
    }
    if (3 == m_nCombCnt)
    {
        PROTECT(e = allocVector(LANGSXP, 5));
        if (!CheckFunc("matrix", fun))
            RLANG_FUNC_NOT_EXIST(-2, true, 1);
        SETCAR(e, fun);
    }
    switch(m_nCombCnt)
    {
    case 2:
        {
            //corresponding R-sentence `par(mfrow = c(2, 1))`
            PROTECT(arg = NEW_INTEGER(2));
            INTEGER(arg)[0] = 2;
            INTEGER(arg)[1] = 1;
        }
        break;
    case 3:
        {
            //`layout(matrix(c(1, 1, 2, 3), 2, 2, byrow=TRUE))`
            PROTECT(arg = NEW_INTEGER(4));
            INTEGER(arg)[0] = 1;
            INTEGER(arg)[1] = 1;
            INTEGER(arg)[2] = 2;
            INTEGER(arg)[3] = 3;
            SETCADR(e, arg);
            SETCADDR(e, ScalarInteger(2));
            SETCADDDR(e, ScalarInteger(2));
            SETCAD4R(e, ScalarLogical(TRUE));
            SET_TAG(CDR(CDR(CDR(CDR(e)))), install("byrow"));
            arg = EvalString(e);

            PROTECT(e = allocVector(LANGSXP, 2));
            if (!CheckFunc("layout", fun))
                RLANG_FUNC_NOT_EXIST(-2, true, 1);
            SETCAR(e, fun);
        }
        break;
    case 4:
        {
            //`par(mfrow = c(2, 2))`
            PROTECT(arg = NEW_INTEGER(2));
            INTEGER(arg)[0] = 2;
            INTEGER(arg)[1] = 2;
        }
        break;
    }
    if (1 != m_nCombCnt)
    {
        SETCADR(e, arg);
        if (2 == m_nCombCnt || 4 == m_nCombCnt)
            SET_TAG(CDR(e), install("mfrow"));
        EvalString(e);
    }
    UNPROTECT(1);
    return 0;
}

SEXP RLangPlot::GetSEXPColor(CRAN_COLOR col)
{
    SEXP color;
    switch(col)
    {
    case COLOR_RED:     color = mkString("red");      break;
    case COLOR_GREEN:   color = mkString("green");    break;
    case COLOR_BLUE:    color = mkString("blue");     break;
    case COLOR_BLACK:   color = mkString("black");    break;
    case COLOR_RAINBOW:
        {
            SEXP rainbow, f;
            PROTECT(rainbow = allocVector(LANGSXP, 2));
            if (!CheckFunc("rainbow", f))
                RLANG_FUNC_NOT_EXIST(0, true, 1);
            SETCAR(rainbow, f);
            SETCADR(rainbow, ScalarInteger(7));
            color = EvalString(rainbow);
            UNPROTECT(1);
        }
        break;
    }
    return color;
}

int RLangPlot::SetParPoint(CRAN_POINT_SHAPE point)
{
    if (POINT_SHAPE_DEFAULT == point)
        return 1;

    SEXP e, fun, arg;
    PROTECT(e = allocVector(LANGSXP, 2));
    if (!CheckFunc("par", fun))
        RLANG_FUNC_NOT_EXIST(0, true, 1);
    SETCAR(e, fun);
    switch(point)
    {
    case POINT_SHAPE_DOT:       SETCADR(e, ScalarInteger(1)); break;
    case POINT_SHAPE_TRIANGLE:  SETCADR(e, ScalarInteger(2)); break;
    case POINT_SHAPE_PLUSMARK:  SETCADR(e, ScalarInteger(3)); break;
    case POINT_SHAPE_XMARK:     SETCADR(e, ScalarInteger(4)); break;
    case POINT_SHAPE_DIAMOND:   SETCADR(e, ScalarInteger(5)); break;
    }
    SET_TAG(CDR(e), install("pch"));
    EvalString(e);
    UNPROTECT(1);
    return 1;
}

int RLangPlot::SetParLine(CRAN_LINE_MODE line)
{
    if (LINE_DEFAULT == line)
        return 1;

    SEXP e, fun, arg;
    PROTECT(e = allocVector(LANGSXP, 2));
    if (!CheckFunc("par", fun))
        RLANG_FUNC_NOT_EXIST(0, true, 1);
    SETCAR(e, fun);
    switch(line)
    {
    case LINE_SOLID:        SETCADR(e, ScalarInteger(1)); break;
    case LINE_IMAGINARY:    SETCADR(e, ScalarInteger(2)); break;
    case LINE_DOTTED:       SETCADR(e, ScalarInteger(3)); break;
    }
    SET_TAG(CDR(e), install("lty"));
    EvalString(e);
    UNPROTECT(1);
    return 1;
}

SEXP RLangPlot::EvalString(SEXP e)
{
    int errorOccurred;
    //PrintValue(e);  //print expression constructed by `PROTECT` macro
    /* Evaluate the call to the R function.*/
    SEXP val = R_tryEval(e, R_GlobalEnv, &errorOccurred);
    if (errorOccurred) {
        Rprintf("Caught an unknown error calling\n");
        fflush(stdout);
    } else {
        //if (R_NilValue != val)
        //    Rf_PrintValue(val);
    }
    return val;
}

int RLangPlot::CheckFunc(const char* pFuncName, SEXP& fun)
{
    fun = findFun(install(pFuncName), R_GlobalEnv);
    if (R_NilValue == fun) {
        Rprintf("No definition for function %s\n", pFuncName);
        return 0;
    }
    return 1;
}
