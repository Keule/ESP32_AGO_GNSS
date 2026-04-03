# ============================================================================
# AgSteer PC Simulation Makefile
#
# Usage:
#   cd pc_sim && make        # build
#   cd pc_sim && make run    # build + run
#   cd pc_sim && make clean  # remove binary
# ============================================================================

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall
INCLUDES := -I../lib -I../include -I.

# --- Source files ---
MAIN     := main_pc.cpp
HAL_PC   := hal_pc/hal_impl.cpp
LOGIC    := ../lib/logic/global_state.cpp \
           ../lib/logic/aog_udp_protocol.cpp \
           ../lib/logic/gnss.cpp \
           ../lib/logic/imu.cpp \
           ../lib/logic/steer_angle.cpp \
           ../lib/logic/actuator.cpp \
           ../lib/logic/control.cpp \
           ../lib/logic/net.cpp

SRCS     := $(MAIN) $(HAL_PC) $(LOGIC)
TARGET   := sim

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ -lm -lpthread

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
