#include "audio/utterance_recorder.hpp"

#include <cmath>
#include <algorithm>

// Constructor
UtteranceRecorder::UtteranceRecorder(Config config) : config_(config) {
    msPerBuffer_ = (int)std::lround(1000.0 * config_.framesPerBuffer / config_.sampleRate);
    preRoll_.reserve((config_.preRollMs * config_.sampleRate) / 1000);
    utterance_.reserve((config_.maxUtteranceMs * config_.sampleRate) / 1000);
}

// Resets recording variables
void UtteranceRecorder::reset() {
    listening_ = false;
    finished_ = false;
    speechMs_ = 0;
    silenceMs_ = 0;
    totalMs_ = 0;
    preRoll_.clear();
    utterance_.clear();
}

// 
float UtteranceRecorder::rms(const float* x, int n) const {
    double acc = 0.0;
    for (int i = 0; i < n; ++i) acc += (double)x[i] * (double)x[i];
    acc /= std::max(1, n);
    return (float)std::sqrt(acc);
}

void UtteranceRecorder::pushPreRoll(const float* x, int n) {
    const int maxPre = (config_.preRollMs * config_.sampleRate) / 1000;
    preRoll_.insert(preRoll_.end(), x, x + n);
    if ((int)preRoll_.size() > maxPre) {
        const int extra = (int)preRoll_.size() - maxPre;
        preRoll_.erase(preRoll_.begin(), preRoll_.begin() + extra);
    }
}

bool UtteranceRecorder::feed(const int16_t* samples, int frames) {
    if (finished_) return true;

    std::vector<float> frames_float(frames);
    for (int i = 0; i < frames; ++i) frames_float[i] = (float)samples[i] / 32768.0f;

    totalMs_ += msPerBuffer_;

    const float r = rms(frames_float.data(), frames);

    if (!listening_) {
        pushPreRoll(frames_float.data(), frames);
        if (r >= config_.vadStartRms) {
            speechMs_ += msPerBuffer_;
            if (speechMs_ >= config_.startHangMs) {
                listening_ = true;
                utterance_.insert(utterance_.end(), preRoll_.begin(), preRoll_.end());
                preRoll_.clear();

                utterance_.insert(utterance_.end(), frames_float.begin(), frames_float.end());
                silenceMs_ = 0;
            }
        } else {
            speechMs_ = 0;
        }
    } else {
        utterance_.insert(utterance_.end(), frames_float.begin(), frames_float.end());

        if (r <= config_.vadStopRms) {
            silenceMs_ += msPerBuffer_;
            if (silenceMs_ >= config_.stopHangMs) {
                finished_ = true;
                listening_ = false;
                return true;
            }
        } else {
            silenceMs_ = 0;
        }

        if (totalMs_ >= config_.maxUtteranceMs) {
            finished_ = true;
            listening_ = false;
            return true;
        }
    }

    return false;
}
