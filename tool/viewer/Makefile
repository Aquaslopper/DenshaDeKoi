# ============================================================================
#  Headless viewer build: the game's own sources, linked against raylib's
#  software renderer (PLATFORM_MEMORY). Separate from the CMake build. All
#  output goes to build/ in this folder, which git ignores.
#
#    ./setup.sh        once: fetch, patch and build raylib
#    make              build build/viewer
#    ./run.sh SCRIPT   render
#
#  REPO is the checkout to look at: by default the one this folder sits in.
# ============================================================================
REPO    ?= $(abspath ../..)
B       := build
DEPS    := $(B)/deps
RAYLIB  := $(DEPS)/raylib-6.0/src

CXX      ?= g++
# -w: this build is for looking, not for compiler diagnostics.
CXXFLAGS := -std=c++20 -O1 -g0 -w -MMD -MP \
  -I$(REPO)/src -I$(RAYLIB)

# The game side is read out of the repository's CMakeLists.txt, so the viewer
# compiles what the client compiles: the hanafuda_core list plus the menu and
# table presentation units. Left out on purpose: src/TableMain.cpp (the viewer
# is the main loop here), src/RadioEngine.cpp, src/SimpleRadio.cpp and
# src/SfxPlayer.cpp (no network and no sound device).
CORE_SRC := $(shell sed -n '/^add_library(hanafuda_core/,/)/p' $(REPO)/CMakeLists.txt \
              | grep -o 'src/[A-Za-z0-9_/]*\.cpp')
GAME_SRC := $(CORE_SRC) src/GameOptions.cpp src/TableScene.cpp src/ui/Hud.cpp \
            src/ui/PauseMenu.cpp src/ui/PauseRadioMenu.cpp
GAME_OBJS := $(patsubst src/%.cpp,$(B)/obj/%.o,$(GAME_SRC))
OBJS := $(GAME_OBJS) $(B)/obj/viewer.o

all: $(B)/viewer

$(B)/obj/%.o: $(REPO)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/obj/viewer.o: viewer.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# The --wrap flags let viewer.cpp record every string the game paints, script
# the clock, stub the magnification shader and tag the fonts - without any game
# file being copied or edited.
WRAPS := DrawTextEx GetFrameTime GetApplicationDirectory LoadShaderFromMemory \
         GetShaderLocation SetShaderValue BeginShaderMode EndShaderMode UnloadShader \
         LoadFontEx
WRAPFLAGS := $(foreach f,$(WRAPS),-Wl,--wrap=$(f))

$(B)/viewer: $(OBJS) $(RAYLIB)/libraylib.a
	$(CXX) -o $@ $(OBJS) -L$(RAYLIB) -lraylib -lm -lpthread -ldl $(WRAPFLAGS)

sources:
	@printf '%s\n' $(GAME_SRC)

clean:
	rm -rf $(B)/obj $(B)/viewer

.PHONY: all sources clean
-include $(OBJS:.o=.d)
