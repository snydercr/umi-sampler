#include <iostream>
#include <juce_events/juce_events.h>
#include "Engine.h"

int main()
{
    // Ensure a JUCE MessageManager exists (needed for AudioDeviceManager + MIDI)
    juce::MessageManager::getInstance();

    std::cout << "UMI Sampler skeleton starting..." << std::endl;

    Engine engine;
    if (!engine.start(48000.0, 256, 2))
    {
        std::cerr << "Failed to start audio. Exiting." << std::endl;
        juce::MessageManager::deleteInstance();
        return 1;
    }

    std::cout << "Audio running (silent)." << std::endl;
    std::cout << "Press Enter to quit." << std::endl;

    // Wait here until user presses Enter
    std::cin.get();

    // Clean shutdown
    engine.stop();
    juce::MessageManager::deleteInstance();
    std::cout << "Bye!" << std::endl;
    return 0;
}
