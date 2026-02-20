#include "stt/whisper_stt.hpp"
#include "audio/utterance_recorder.hpp"

#include <portaudio.h>
#include <iostream>
#include <stdexcept>

static void pa_check(PaError e, const char* msg) {
    if (e != paNoError) {
        throw std::runtime_error(std::string(msg) + " (" + std::to_string((int)e) + "): " + Pa_GetErrorText(e));
    }
}

int run_live_stt(WhisperSTT& stt) {
    pa_check(Pa_Initialize(), "Pa_Initialize");

    UtteranceRecorder::Config config;

    UtteranceRecorder recorder(config);

    PaStreamParameters inParams{};
    inParams.device = Pa_GetDefaultInputDevice();
    if (inParams.device == paNoDevice) {
        throw std::runtime_error("No default input device");
    }

    const PaDeviceInfo* info = Pa_GetDeviceInfo(inParams.device);
    std::cout << "Input device: " << (info ? info->name : "(unknown)") << "\n";

    inParams.channelCount = 1;
    inParams.sampleFormat = paInt16;
    inParams.suggestedLatency = info ? info->defaultLowInputLatency : 0.05;
    inParams.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream = nullptr;
    pa_check(
        Pa_OpenStream(&stream, &inParams, nullptr,
                      config.sampleRate, config.framesPerBuffer,
                      paNoFlag, nullptr, nullptr),
        "Pa_OpenStream"
    );

    pa_check(Pa_StartStream(stream), "Pa_StartStream");

    std::cout << "Listening... (speak, then pause)\n";

    std::vector<int16_t> buff(config.framesPerBuffer);

    while (true) {
        PaError e = Pa_ReadStream(stream, buff.data(), config.framesPerBuffer);
        if (e == paInputOverflowed) {
            continue;
        }
        pa_check(e, "Pa_ReadStream");

        const bool done = recorder.feed(buff.data(), config.framesPerBuffer);

        if (done && recorder.hasUtterance()) {
            std::cout << "Transcribing...\n";
            const auto& pcm = recorder.utterance();

            std::string text = stt.transcribe(pcm, 4);

            std::cout << "STT: " << text << "\n\n";

            // TODO: Pass text onto llm
            
            recorder.reset();
            break;
        }
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}
