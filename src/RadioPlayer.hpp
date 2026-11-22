#pragma once

#include "RadioPlaylist.hpp"
#include "RadioStreamWorker.hpp"

#include <string>
#include <string_view>

struct RadioSnapshot {
    RadioState state = RadioState::OFF;
    bool enabled = false;
    int stationIndex = 0; // -1 when playing a custom URL
    std::string stationName;
    std::string nowPlaying;
    std::string statusText;
    std::string customUrl;
    float volume = 0.65f;
    bool muted = false;
};

class RadioPlayer {
public:
    RadioPlayer();
    ~RadioPlayer();
    RadioPlayer(const RadioPlayer&) = delete;
    RadioPlayer& operator=(const RadioPlayer&) = delete;

    void SelectStation(int index);
    void PlayCustomUrl(std::string_view url);
    void ToggleEnabled();
    void Stop();
    void SetVolume(float volume);
    void ToggleMute();
    void Update();

    [[nodiscard]] RadioSnapshot Snapshot() const;

private:
    void StartCurrentSelection();
    void ResetAudioStream();

    RadioStreamWorker worker_;
    bool enabled_ = false;
    int stationIndex_ = 0;
    std::string customUrl_;
    float volume_ = 0.65f;
    bool muted_ = false;

    bool ownsAudioDevice_ = false;
    bool audioDeviceReady_ = false;
    bool streamActive_ = false;
    int streamSampleRate_ = 0;
    int streamChannels_ = 0;
    struct AudioImpl;
    AudioImpl* audio_ = nullptr;
};
