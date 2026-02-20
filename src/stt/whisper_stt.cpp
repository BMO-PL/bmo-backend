#include "stt/whisper_stt.hpp"

#include <whisper.h>
#include <stdexcept>

WhisperSTT::WhisperSTT(const std::string& modelPath) {
    context_ = whisper_init_from_file(modelPath.c_str());
    if (!context_) throw std::runtime_error("whisper_init_from_file failed: " + modelPath);
}

WhisperSTT::~WhisperSTT() {
    if (context_) whisper_free(context_);
}

std::string WhisperSTT::transcribe(const std::vector<float>& pcm16kMono, int threads) {
    if (pcm16kMono.empty()) return {};

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    params.n_threads = threads;
    params.language = "en";
    params.translate = false;

    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;

    params.no_speech_thold = 0.6f;

    const int rc = whisper_full(context_, params, pcm16kMono.data(), (int)pcm16kMono.size());
    if (rc != 0) throw std::runtime_error("whisper_full failed");

    std::string out;
    const int n_segments = whisper_full_n_segments(context_);
    for (int i = 0; i < n_segments; ++i) out += whisper_full_get_segment_text(context_, i);
    return out;
}
