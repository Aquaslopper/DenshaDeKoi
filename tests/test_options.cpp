#include "GameOptions.hpp"
#include "ui/PauseMenu.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

void TestInput() {
    GameOptions o;
    Ui::PauseMenu menu;
    constexpr Resolution desktop{1920, 1080};
    const auto input = [&](float x, float y, bool pressed, bool down) {
        return menu.Update(o, {{Ui::PausePanel.x + x * Ui::PauseArtScale, Ui::PausePanel.y + y * Ui::PauseArtScale},
                               true, pressed, down, 0, 0, false}, o.resolution, desktop);
    };
    const auto click = [&](float x, float y) {
        const auto resume = input(x, y, true, true);
        input(x, y, false, false);
        return resume;
    };
    const auto key = [&](int v, int h, bool enter = false) {
        return menu.Update(o, {{-1, -1}, false, false, false, v, h, enter}, o.resolution, desktop);
    };
    Check(!input(616, 240, true, true) && o.masterVolume == 50, "Slider hit must track the actual meter");
    input(900, 318, false, true);
    Check(o.masterVolume == 100 && o.sfxVolume == 70, "Dragging out must clamp, not switch rows");
    input(400, 318, false, true);
    Check(o.masterVolume == 0, "Volume must reach zero");
    input(400, 318, false, false);
    input(700, 318, false, true);
    Check(o.masterVolume == 0 && o.sfxVolume == 70, "Release must end the drag");
    click(550, 318);
    Check(o.sfxVolume == 25, "SFX has an independent slider");
    key(0, -1);
    Check(o.sfxVolume == 20, "Arrow key must adjust focused level");
    click(730, 380); Check(!o.cardHints, "Hints OFF");
    click(485, 380); Check(o.cardHints, "Hints ON");
    click(815, 450); Check(o.resolution == Resolution{1600, 900}, "Next display size");
    click(430, 450); Check(o.resolution == Resolution{1360, 768}, "Previous display size");
    click(620, 521); Check(o.screenMode == ScreenMode::Borderless, "Borderless button");
    click(815, 450); Check(o.resolution == Resolution{1360, 768}, "Borderless must retain the window size");
    click(756, 521); Check(o.screenMode == ScreenMode::Fullscreen, "Fullscreen button");
    click(470, 521); Check(o.screenMode == ScreenMode::Windowed, "Windowed button");
    click(620, 590); Check(o.aaSamples == 3, "Middle AA choice");
    click(756, 590); Check(o.aaSamples == 4, "Last AA choice");
    click(470, 590); Check(o.aaSamples == 2, "First AA choice");
    const auto before = o;
    Check(!click(990, 590) && !click(120, 710) && o == before, "Train and unused footer must be inert");
    Check(click(604, 705), "Baked Resume button must be live");
    menu = {};
    key(-1, 0);
    Check(menu.Focus() == Ui::OptionRow::Resume && key(0, 0, true), "Keyboard Resume and focus wrap");
    key(1, 0); key(0, 1);
    Check(menu.Focus() == Ui::OptionRow::Master && o.masterVolume == 5, "Keyboard-only navigation");
}

void TestStorage() {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / ("ddk-options-test-" + std::to_string(std::random_device{}()));
    Check(fs::create_directory(dir), "Could not create fresh settings test directory");
    const auto file = dir / "options.txt";
    Check(LoadOptions(file) == GameOptions{}, "Missing file should use safe defaults");
    GameOptions custom{0, 83, false, {1536, 864}, ScreenMode::Fullscreen, 4};
    SaveOptions(file, custom);
    Check(LoadOptions(file) == custom, "All six settings should survive a restart");
    for (const char* bad : {"truncated", "DDK-OPTIONS-1 80 70 1 1920 1080 9 2", "DDK-OPTIONS-1 80 70 0 -30 64 0 3",
                            "DDK-OPTIONS-1 80 70 1 1360 768 0 999"}) {
        std::ofstream(file) << bad;
        Check(LoadOptions(file) == GameOptions{}, "Invalid settings should use safe defaults");
    }
    fs::remove(file); fs::remove(dir);
}
} // namespace

void TestOptions() {
    TestInput(); TestStorage();
    Check(StepResolution({1500, 800}, {1920, 1080}, 1) == Resolution{1600, 900}, "Step from custom size");
    Check(StepResolution({960, 540}, {1440, 900}, -1) == Resolution{1440, 900}, "Include a non-preset desktop size");
    Check(StepResolution({1280, 720}, {1280, 720}, 1) == Resolution{960, 540}, "Omit sizes too large for the display");
}
