#This makefile is used to compile these sources in current directory.
#As the shared library encapsulated maybe cause problems at any time, 
#so I prepare a test script for its automatic testing.It would use the
#functionality of receiving stream supported by libnetec.so and libdts.so
#Also it will test whether the current frame is key frame, thus it would
#use `H264FrameParse.cpp` file.But beyond all what I have mentioned, 
#these files are the sources of libhpcomp.so

#Note: H264* & main.cpp these files are used to construct the test script
#Author: Ricci, 2015/1/19

TARGET_TEST 		= testcase
TARGET_LIB 		= libhpcomp.so
OBJPATH 		= .
CXX 			= g++

HEADERS_LIB 		= $(filter-out H264%.h RLang%.h, $(wildcard *.h))
ifeq ($(ENABLE_R), 1)
	HEADERS_LIB += $(wildcard RLang*.h)
endif

SOURCES_TEST 		= $(wildcard main.cpp H264*.cpp)
SOURCES_LIB 		= $(filter-out main.cpp H264%.cpp RLang%.cpp measure%.cpp, $(wildcard *.cpp))
SOURCES_MEASURE 	= $(wildcard measure*.cpp)
ifeq ($(ENABLE_R), 1)
	SOURCES_MEASURE += $(wildcard RLang*.cpp)
endif

OBJS_TEST 		= $(patsubst %.cpp, %.o, $(SOURCES_TEST))
OBJS_LIB 		= $(patsubst %.cpp, %.o, $(SOURCES_LIB))
OBJS_MEASURE 		= $(patsubst %.cpp, %.o, $(SOURCES_MEASURE))

FULLOBJS_TEST 		= $(patsubst %.cpp, $(OBJPATH)/%.o, $(SOURCES_TEST))
FULLOBJS_LIB 		= $(patsubst %.cpp, $(OBJPATH)/%.o, $(SOURCES_LIB))
FULLOBJS_MEASURE 	= $(patsubst %.cpp, $(OBJPATH)/%.o, $(SOURCES_MEASURE))

INCPATH 		= -I../HPComp/include/

LIBPATH_LIB 		= -L/opt/intel/mediasdk/bin/x64/ -lpthread -lmfxhw64 -lva-drm -lva
LIBPATH_TEST 		= -L../HPComp/lib/ -lR -lnetec -ldts -lhpcomp -Wl,-rpath='/home/ricci/workdir/HPComp/lib/'

LDFLAGS_TEST 		= -w -rdynamic -msse4
LDFLAGS_LIB 		= $(LDFLAGS_TEST) -shared

CFLAGS 			= -w -c -g -fPIC -msse4 -DCONFIG_USE_MFXALLOCATOR
CFLAGS_MEASURE 		= $(CFLAGS)
ifeq ($(ENABLE_R), 1)
	LIBPATH_LIB += -lR
	CFLAGS_MEASURE += -DCONFIG_ENABLE_RLANG
endif

all:
	@echo "----------------------NOTE----------------------------"
	@echo "use \"make test\" to build test script of libhpcomp.so"
	@echo "use \"make libcomp\" to build libhpcomp.so"

test:$(TARGET_TEST)
libcomp:$(TARGET_LIB)

$(TARGET_TEST):$(OBJS_TEST)
	$(CXX) $(LDFLAGS_TEST) $(FULLOBJS_TEST) -o $(TARGET_TEST) $(LIBPATH_TEST)

$(TARGET_LIB):$(OBJS_LIB) $(OBJS_MEASURE)
	$(CXX) $(LDFLAGS_LIB) $(FULLOBJS_LIB) $(FULLOBJS_MEASURE) -o $(TARGET_LIB) $(LIBPATH_LIB)
	cp $(TARGET_LIB) ../HPComp/lib/
	cp $(HEADERS_LIB) ../HPComp/include/MSDKVideo/

$(OBJS_TEST):$(SOURCES_TEST)
	$(CXX) $(CFLAGS) $*.cpp -o $(OBJPATH)/$@ $(INCPATH)

$(OBJS_LIB):$(SOUCES_LIB)
	$(CXX) $(CFLAGS) $*.cpp -o $(OBJPATH)/$@ $(INCPATH)

$(OBJS_MEASURE):$(SOURCES_MEASURE)
	$(CXX) $(CFLAGS_MEASURE) $*.cpp -o $(OBJPATH)/$@ $(INCPATH)

clean:
	rm -f $(OBJPATH)/*.o
ifeq ($(TARGET_TEST), $(wildcard $(TARGET_TEST)))
	rm -f $(TARGET_TEST)
endif
ifeq ($(TARGET_LIB), $(wildcard $(TARGET_LIB)))
	rm -f $(TARGET_LIB)
endif
