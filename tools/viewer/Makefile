CXX ?= g++
RAYLIB := build/deps/raylib-6.0/src
REPO := ../..
CXXFLAGS := -std=c++20 -O2 -I$(REPO)/src -I$(RAYLIB)
SOURCES := viewer.cpp $(REPO)/src/ui/PauseMenu.cpp $(REPO)/src/ui/PauseRadioMenu.cpp $(REPO)/src/RadioPlaylist.cpp $(REPO)/src/GameOptions.cpp build/TableScene.cpp $(REPO)/src/ui/Hud.cpp $(REPO)/src/TableGame.cpp $(REPO)/src/Deck.cpp $(REPO)/src/KoiKoiRules.cpp $(REPO)/src/KoiKoiStateMachine.cpp

all: build/viewer
build/TableScene.cpp: $(REPO)/src/TableScene.cpp prepare-table.py
	python3 prepare-table.py $< $@
build/viewer: $(SOURCES) $(RAYLIB)/libraylib.a
	$(CXX) $(CXXFLAGS) $(SOURCES) $(RAYLIB)/libraylib.a -lm -lpthread -ldl -o $@
clean:
	rm -f build/viewer
.PHONY: all clean
