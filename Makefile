
PROGS = caltrain_test party_test
PATH_TO_FILE = destruct.cc
ifneq ("$(wildcard $(PATH_TO_FILE))","")
    PROGS += destruct
endif
OBJS = caltrain.o caltrain_test.o party.o party_test.o
HEADERS = caltrain.hh party.hh

CXX = clang++-10 -std=c++20
CXXFLAGS = -ggdb -O -Wall -Werror $(DEPS)

all: $(PROGS)

test: $(PROGS)
	./run_tests

%_test: %_test.o %.o
	$(CXX) $(CXXFLAGS) $^ -pthread -L/usr/class/cs110/local/lib/ -lthreads -o $@

destruct: destruct.cc
	$(CXX) $(CXXFLAGS) destruct.cc -o destruct

$(OBJS): $(HEADERS)

clean::
	rm -f $(PROGS) $(OBJS) *~ .*~

.PHONY: all clean

