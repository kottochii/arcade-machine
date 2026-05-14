INCDIRS = -Iinclude
CXX = skm g++ $(INCDIRS) -Ofast
src = $(wildcard src/*.cpp)
obj = $(src:.cpp=.o)

LDFLAGS = -std=c++14

ifeq ($(OS),Windows_NT)
    LDFLAGS += -lstdc++fs
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        LDFLAGS += -lstdc++fs
    endif
endif

ArcadeMachine: $(obj)
	-.scripts/generate-stats.sh
	$(CXX) -o $@ $^ $(LDFLAGS) 

clean:
	rm $(obj)
