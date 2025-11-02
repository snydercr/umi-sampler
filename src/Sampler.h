#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

/** Stub sampler core with a plugin-like API you’ll fill in later. */
class Sampler
{
public:
    void prepare(double sampleRate, int blockSize, int numOutputs)
    {
        sr = sampleRate; block = blockSize; outs = numOutputs;
        // TODO: allocate voices/buffers once you implement the real sampler
    }

    void release()
    {
        // TODO: free resources if needed
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        // For initial commit, stay silent. This guarantees safe startup.
        buffer.clear();
    }

private:
    double sr = 0.0;
    int block = 0;
    int outs  = 0;
};
