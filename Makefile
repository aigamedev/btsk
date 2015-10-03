btsk_TARGET := libbtsk.so
btsk_SOURCE := \
	BehaviorTree.cpp \
	BehaviorTreeEvent.cpp \
	BehaviorTreeOptimized.cpp \
	BehaviorTreeShared.cpp
btsk_OBJS := $(btsk_SOURCE:%.cpp=%.fpic.o)
btsk_LDFLAGS := -fPIC -shared

test_TARGET := btsk-test
test_SOURCE := Test.cpp
test_OBJS := $(test_SOURCE:%.cpp=%.o)
test_LDFLAGS := -L.
test_LIBS += -lbtsk

CXXFLAGS_local += -std=c++11
CXXFLAGS_shared += -DPIC -fPIC

ifeq ($(strip $(V)),)
  SILENT=@
  P=@ echo
else
  SILENT=
  P=@ true
endif

.PHONY: all
all: $(btsk_TARGET) $(test_TARGET)

.PHONY: clean
clean: 
	$(P) "  RM              $(btsk_TARGET)"
	$(SILENT) rm -f $(btsk_TARGET)
	$(P) "  RM              $(btsk_OBJS)"
	$(SILENT) rm -f $(btsk_OBJS)
	$(P) "  RM              $(test_TARGET)"
	$(SILENT) rm -f $(test_TARGET)
	$(P) "  RM              $(test_OBJS)"
	$(SILENT) rm -f $(test_OBJS)

.PHONY: check
check: $(test_TARGET)
	$(SILENT) LD_LIBRARY_PATH=$(PWD) ./$(test_TARGET)

.PHONY: install
install: 
	@ echo "really, thou shall not do that"

$(btsk_TARGET): $(btsk_OBJS)
	$(P) "* LD.C++.shared   $<"
	$(SILENT) $(CXX) $(LDFLAGS) $(LDFLAGS_local) $(btsk_LDFLAGS) -o $@ $^ $(LIBS) $(LIBS_local) $(btsk_LIBS)

$(test_TARGET): $(btsk_TARGET)
$(test_TARGET): $(test_OBJS)
	$(P) "* LD.C++          $<"
	$(SILENT) $(CXX) $(LDFLAGS) $(LDFLAGS_local) $(test_LDFLAGS) -o $@ $^ $(LIBS) $(LIBS_local) $(test_LIBS)

%.fpic.o: %.cpp
	$(P) "  C++.shared      $<"
	$(SILENT) $(CXX) $(CXXFLAGS) $(CXXFLAGS_local) $(CXXFLAGS_shared) -o $@ -c $<

%.o: %.cpp
	$(P) "  C++             $<"
	$(SILENT) $(CXX) $(CXXFLAGS) $(CXXFLAGS_local) -o $@ -c $<

