#include "Engine.h"
#include "SamplerEngine.h"

Engine::Engine() {}
Engine::~Engine() { stop(); }

bool Engine::start(double requestedSampleRate, int requestedBlock, int outChannels)
{
    // Initialise default device: 0 inputs, N outputs
    auto err = deviceManager.initialise(/*numInputChannels*/ 0,
        /*numOutputChannels*/ juce::jmax(1, outChannels),
        /*savedState*/ nullptr,
        /*selectDefaultDeviceOnFailure*/ true);
    if (err.isNotEmpty())
    {
        juce::Logger::writeToLog("Audio init error: " + err);
        return false;
    }

    // Apply requested SR / block if possible
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    setup.sampleRate = requestedSampleRate;
    setup.bufferSize = requestedBlock;
    deviceManager.setAudioDeviceSetup(setup, /*latency*/ true);

    // Query final device parameters
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        sampleRate = dev->getCurrentSampleRate();
        blockSize = dev->getCurrentBufferSizeSamples();
        numOutputs = dev->getActiveOutputChannels().countNumberOfSetBits();
    }
    else
    {
        sampleRate = requestedSampleRate;
        blockSize = requestedBlock;
        numOutputs = outChannels;
    }

    // Prepare our AudioSource chain
    prepare(sampleRate, blockSize, numOutputs);

    // The player becomes the device callback
    deviceManager.addAudioCallback(&player);
    return true;
}

void Engine::stop()
{
    deviceManager.removeAudioCallback(&player);
    deviceManager.closeAudioDevice();
    release();
}

void Engine::prepare(double sr, int block, int outs)
{
    sampler = std::make_unique<SamplerEngine>();
    //sampler->prepare(block, sr); // AudioSource API

    //player.setSource(sampler.get());   // route sampler to device
}

void Engine::release()
{
    player.setSource(nullptr); // detach before destroying sampler
    if (sampler)
       // sampler->releaseResources();
    sampler.reset();
}
