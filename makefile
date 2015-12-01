
#
# target
#
TARGET = a.out

#
# source file
#
SRCCPP = main.cpp memcheck.cpp
SRCC =
MAKEFILE = makefile

SRCS = $(SRCCPP) $(SRCC)
OBJS = $(SRCCPP:.cpp=.o) $(SRCC:.c=.o)
DEPENDFILES = $(SRCCPP:.cpp=.d) $(SRCC:.c=.d)

#
# compile parameter
#
CC = g++
CFLAGS = -g -O2 -Wall -std=c++11 -Wno-deprecated-declarations
LDFLAGS =
#CFLAGS = -g -O2 -Wall -fprofile-arcs -ftest-coverage
INCDIR =
LIBDIR =
LIBS =
#LIBS = -lgcov -lpthread

#
# shell comannd
#
RM = rm -rf
#RM = del

# definition
.SUFFIXES: .cxx .cpp .c .o .h .d
.PHONY: clean all echo

#
# Rules
#
all: $(TARGET)

echo:
	@echo 'TARGET:$(TARGET)'
	@echo 'SRCS:$(SRCS)'
	@echo 'OBJS:$(OBJS)'

$(TARGET): $(OBJS)
	@echo 'Making $(TARGET)...'
	-@ $(CC) $(LDFLAGS) -o $@ $^ $(LIBDIR) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -MMD -c $< $(INCDIR) -o $@

.cpp.o:
	$(CC) $(CFLAGS) -MMD -c $< $(INCDIR) -o $@

clean:
	-@ $(RM) $(TARGET) $(DEPENDFILES) $(OBJS) *.d *.o *.obj *~ *.~* *.BAK

# source and header file dependent
-include $(DEPENDFILES)
