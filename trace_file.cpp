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

/**
 *\file trace_file.cpp
 *\brief Implementation for trace file
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "trace_file.h"
using namespace std;

#define MAX_TIME_STR_LEN 40
#define MAX_PATH_NAME_LEN 256
#define MAX_STARTUP_LOG_LEN 128

static const long MAX_SIZE_IN_BYTE = 10000000;
static const int MAX_BAK_FILE_NUM = 5;
static const int MAX_LOG_LEN = 1280;

TraceFile *TraceFile::instance_ = NULL;
std::string TraceFile::LOG_FILE = "";  //there is no LOG file as default

void TraceFile::init(const char *filename)
{
    int sur_len = 0;
    int dst_len = 0;
    char logpath[MAX_PATH_NAME_LEN] = {0};

    if (NULL == filename) {
        no_file_ = true;
        return;
    } else {
        no_file_ = false;
        LOG_FILE = filename;
        sur_len = strlen(LOG_FILE.c_str());
        dst_len = sizeof(logpath);
        if (dst_len < sur_len)
            printf("WARING: log file name too long, will be cut short\n");
        snprintf(logpath, dst_len, "%s", LOG_FILE.c_str());

        ifstream tracefile(logpath);
        //get the size of trace log file
        tracefile.seekg(0, ios::end);
        size_ = tracefile.tellg();
        if (size_ == -1) {
            size_ = 0;
        }
        tracefile.close();
    }

    time_t now;
    time(&now);
    tm *pnow = localtime(&now);
    char timestr[MAX_TIME_STR_LEN] = {0};
    char msg[MAX_STARTUP_LOG_LEN] = {0};
    if (pnow)
        strftime(timestr, MAX_TIME_STR_LEN, "%Y %b %d %X", pnow);

    sur_len = strlen(timestr);
    dst_len = sizeof(msg);
    if (dst_len < sur_len)
        printf("WARING: msg string too long, will be cut short\n");
    snprintf(msg, dst_len,\
                       "\n\n----------------- startup @ %s -----------------\n",\
                       timestr);
    add_log(msg);
}

void TraceFile::add_trace(TraceInfo *trc)
{
    if (no_file_)
        return;

    int sur_len = 0;
    int dst_len = 0;
    char timestr[MAX_TIME_STR_LEN] = {0};
    char msg[MAX_LOG_LEN] = {0};
    tm *timeInfo = localtime(trc->time());
    if (timeInfo)
        strftime(timestr, MAX_TIME_STR_LEN, "%X", timeInfo);

    sur_len = strlen(TraceInfo::mod_name(trc->mod()) .c_str()) +\
              strlen(TraceInfo::level_name(trc->level()).c_str()) +\
              strlen(timestr) +\
              strlen(trc->desc().c_str()) + 10;
    dst_len = sizeof(msg);
    if (dst_len < sur_len)
        printf("WARING: msg string too long, will be cut short!\n");
    snprintf(msg, dst_len,\
                       "<%s>  <%s>  <%s>:  %s",\
                       TraceInfo::mod_name(trc->mod()) .c_str(),\
                       TraceInfo::level_name(trc->level()).c_str(),\
                       timestr,\
                       trc->desc().c_str());
    add_log(msg);
}

void TraceFile::add_log(const char *msg)
{
    check();
    open_file();
    size_ += strlen(msg) + strlen("\n");
    file_ << msg << endl;
}

void TraceFile::check()
{
    if (size_ >= MAX_SIZE_IN_BYTE) {
        file_.close();
        backup();
        size_ = 0;
    }
}

void TraceFile::open_file()
{
    char logpath[MAX_PATH_NAME_LEN] = {0};
    int sur_len = strlen(LOG_FILE.c_str());
    int dst_len = sizeof(logpath);
    if (dst_len < sur_len)
        printf("WARING: log file name too long, will be cut short!!!!!\n");
    snprintf(logpath, dst_len, "%s", LOG_FILE.c_str());

    //check the existence of the file
    if (0 != access(logpath, F_OK)) {
        file_.close();
        size_ = 0;
    }

    if (!file_.is_open())
    {
        file_.open(logpath, ios::app);
        if (!file_.is_open())
            printf("ERROR: fail to open log file, please check the log file path\n");
    }
}

void TraceFile::backup()
{
    char curtrclog[MAX_PATH_NAME_LEN] = {0};
    int sur_len = strlen(LOG_FILE.c_str());
    int dst_len = sizeof(curtrclog);
    if (dst_len < sur_len)
        printf("WARING: log file name too long, will be cut short!\n");
    snprintf(curtrclog, dst_len, "%s", LOG_FILE.c_str());

    for (int i = MAX_BAK_FILE_NUM - 1; i > 0; i--) {
        char oldname[MAX_PATH_NAME_LEN + 5] = {0}, newname[MAX_PATH_NAME_LEN + 5] = {0};

        sur_len = strlen(curtrclog) + 5;
        dst_len = sizeof(oldname);
        if (dst_len < sur_len)
            printf("WARING: log file name too long, will be cut short!!\n");
        snprintf(oldname, dst_len, "%s.bak%d", curtrclog, i);

        sur_len = strlen(curtrclog) + 5;
        dst_len = sizeof(newname);
        if (dst_len < sur_len)
            printf("WARING: log file name too long, will be cut short!!!\n");
        snprintf(newname, dst_len, "%s.bak%d", curtrclog, i + 1);

        rename(oldname, newname);
    }

    char baklog[MAX_PATH_NAME_LEN] = {0};
    sur_len = strlen(curtrclog);
    dst_len = sizeof(baklog);
    if (dst_len < sur_len)
        printf("WARING: log file name too long, will be cut short!!!!\n");
    snprintf(baklog, dst_len, "%s.bak1", curtrclog);

    rename(curtrclog, baklog);
}
