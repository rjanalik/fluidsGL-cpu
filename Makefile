HOST_OS   := $(shell uname -s 2>/dev/null | tr "[:upper:]" "[:lower:]")
TARGET_OS ?= $(HOST_OS)

GCC := g++
#GCC  := g++-7
#PGCC := pgcc
#GCC := pgc++

#GCCFLAGS := -fast -v -I/usr/local/Cellar/freeglut/3.0.0/include/GL -I/usr/local/Cellar/glew/2.1.0/include/GL
GCCFLAGS   := -std=c++11 -O3 
PGCCFLAGS     :=
LDFLAGS     :=

ALL_CCFLAGS += $(GCCFLAGS)

LIBRARIES :=

include ./findgllib.mk

# OpenGL specific libraries
ifeq ($(TARGET_OS),darwin)
 # Mac OSX specific libraries and paths to include
 #LIBRARIES += -L/usr/local/Cellar/freeglut/3.0.0/lib
 LIBRARIES += -L/System/Library/Frameworks/OpenGL.framework/Libraries
 LIBRARIES += -framework OpenGL -framework GLUT
 #LIBRARIES += -lGL -lGLU
 ALL_LDFLAGS += -Xlinker -framework -Xlinker GLUT
else
 LIBRARIES += $(GLLINK)
 LIBRARIES += -lGL -lGLU -lX11 -lglut
endif

# Gencode arguments
SMS ?= 30 35 37 50 52 60 61 70

################################################################################

# Target rules
all: build

build: fluidsGL


fluidsGL.o:fluidsGL.cpp
	$(EXEC) $(GCC)  $(ALL_CCFLAGS) -o $@ -c $<

fluidsGL_kernels.o:fluidsGL_kernels.cpp
	$(EXEC) $(GCC) $(ALL_CCFLAGS) -o $@ -c $<

fluidsGL: fluidsGL.o fluidsGL_kernels.o
	$(EXEC) $(GCC) $(ALL_LDFLAGS) -lfftw3f -o $@ $+ $(LIBRARIES)

run: build
	$(EXEC) ./fluidsGL

clean:
	rm -f fluidsGL fluidsGL.o fluidsGL_kernels.o

clobber: clean
