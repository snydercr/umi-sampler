#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class SamplerEngine; // forward

/** Headless audio engine using AudioSourcePlayer (no callback signature hassles). */
class Engine
{
public:
    Engine();
    ~Engine();

    // Start/stop device; safe default is 48k / 256 on Pi/USB
    bool start(double requestedSampleRate = 48000.0, int requestedBlock = 256, int outChannels = 2);
    void stop();

    double getSampleRate() const noexcept { return sampleRate; }
    int    getBlockSize()  const noexcept { return blockSize; }

private:
    void prepare(double sr, int block, int outs);
    void release();

    juce::AudioDeviceManager deviceManager;   // owns the device
    juce::AudioSourcePlayer  player;          // becomes the device's callback
    std::unique_ptr<SamplerEngine> sampler;   // your future sampler core

    double sampleRate = 0.0;
    int    blockSize = 0;
    int    numOutputs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Engine)
};
