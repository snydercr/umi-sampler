#include <iostream>
#include <juce_events/juce_events.h>
#include "Engine.h"
#include "SerialService.h"

int main()
{
    // Ensure a JUCE MessageManager exists (needed by some JUCE utilities).
    juce::MessageManager::getInstance();

    std::cout << "UMI Sampler skeleton starting..." << std::endl;

    // --- SERIAL: create and connect before starting audio ---
    SerialService serial;

    const juce::String dev = "/dev/ttyUSB0"; // from your dmesg/ls
    if (serial.connect(dev, 115200))
        std::cout << "Serial open @115200: " << dev << std::endl;
    else
        std::cout << "FAILED to open serial: " << dev << std::endl;

    // Print each complete line from the board ("D" or "*")
    // NOTE: In this console app we can print directly (no UI), so no callAsync needed.
    serial.onLine = [](juce::String s)
    {
        std::cout << "Serial line: " << s << std::endl;
        // If you want to react in audio later, you can push to Engine via a thread-safe flag/queue.
    };

    // --- AUDIO ENGINE ---
    Engine engine;
    if (!engine.start(48000.0, 256, 2))
    {
        std::cerr << "Failed to start audio. Exiting." << std::endl;
        serial.disconnect();                 // <--- clean up serial too
        juce::MessageManager::deleteInstance();
        return 1;
    }

    std::cout << "Audio running (silent)." << std::endl;
    std::cout << "Listening on " << dev << " — wave at the sensor to see D/*" << std::endl;
    std::cout << "Press Enter to quit." << std::endl;

    // Wait here until user presses Enter (serial thread keeps running)
    std::cin.get();

    // Clean shutdown
    engine.stop();
    serial.disconnect();                     // <--- close port & stop reader thread
    juce::MessageManager::deleteInstance();

    std::cout << "Bye!" << std::endl;
    return 0;
}
