#ifndef WHISPER_STT_HPP
#define WHISPER_STT_HPP

#include <string>
#include <vector>

struct whisper_context;

class WhisperSTT {
public:
    explicit WhisperSTT(const std::string& modelPath);
    ~WhisperSTT();

    WhisperSTT(const WhisperSTT&) = delete;
    WhisperSTT& operator=(const WhisperSTT&) = delete;

    std::string transcribe(const std::vector<float>& pcm16kMono, int threads = 4);

private:
    whisper_context* context_ = nullptr;
};

#endif
