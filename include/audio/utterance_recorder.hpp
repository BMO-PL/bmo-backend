#ifndef UTTERANCE_RECORDER_HPP
#define UTTERANCE_RECORDER_HPP

#include <vector>
#include <cstdint>

class UtteranceRecorder {
public:
    struct Config {
        int sampleRate = 16000;
        int channels = 1;

        int framesPerBuffer = 160;

        float vadStartRms = 0.014f;
        float vadStopRms = 0.011f;
        int startHangMs = 80;
        int stopHangMs = 550;

        int maxUtteranceMs = 12000;
        int preRollMs = 250;
    };

    explicit UtteranceRecorder(Config config);

    bool feed(const int16_t* samples, int frames);

    bool isListening() const { return listening_; }
    bool hasUtterance() const { return finished_; }

    const std::vector<float>& utterance() const { return utterance_; }

    void reset();

private:
    Config config_;

    bool listening_ = false;
    bool finished_ = false;

    int msPerBuffer_ = 0;
    int speechMs_ = 0;
    int silenceMs_ = 0;
    int totalMs_ = 0;

    std::vector<float> preRoll_;
    std::vector<float> utterance_;

    float rms(const float* x, int n) const;
    void pushPreRoll(const float* x, int n);

};

#endif
