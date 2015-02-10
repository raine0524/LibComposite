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
 *\file trace_file.h
 *\brief Definition of trace file
 */

#ifndef TRACE_FILE_H_
#define TRACE_FILE_H_

#include <fstream>
#include <string>
#include "trace_info.h"

//Singleton Pattern
class TraceFile
{
public:
    static TraceFile *instance()
    {
        /* should use double-check to ensure it is right, but for this case
         *  concurrence is not heavy, double-check can be ignored;
         */
        if (instance_ == NULL) {
            //double checked ignored
            //if (instance_ == NULL)
                instance_ = new TraceFile();
        }
        return instance_;
    }
    static void close()
    {
        if (instance_)
            delete instance_;
    }

    void init(const char *filename = NULL);
    void add_trace(TraceInfo *);

private:
    TraceFile() : no_file_(true), size_(0) {}
    ~TraceFile()
    {
        if (file_.is_open())
            file_.close();
    }

    void add_log(const char *msg);
    void check();
    void backup();
    void open_file();

private:
    bool no_file_;
    std::ofstream file_;
    long size_;

    static TraceFile *instance_;
    static std::string LOG_FILE;

private:    //The Law of The Big Three in C++
    TraceFile(const TraceFile &);
    TraceFile operator=(const TraceFile &);
};
#endif
