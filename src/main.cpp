#include <iostream>
#include <atomic>
#include <thread>
#include <optional>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "AudioProcessor.h"
#include "SerialService.h"
#include "OscBridge.h"

// ---- tiny CLI helper ----
static std::optional<juce::String> opt (const juce::StringArray& a, const juce::String& key)
{
    for (int i = 0; i < a.size(); ++i)
        if (a[i] == key && i + 1 < a.size())
            return a[i + 1];
    return std::nullopt;
}

int main (int argc, char** argv)
{
    juce::ignoreUnused(argc, argv);
    juce::MessageManager::getInstance();

    std::cout << "UMI Sampler starting..." << std::endl;

    // ---- CLI config ----
    juce::String macHost   = "10.0.0.166";  // --mac-host (or ip)
    int          macInPort = 9000;             // --mac-in-port
    juce::String deviceId  = "pi-01";          // --device-id
    int          listenPort = 9100;            // --listen-port
    const juce::String serialDev = "/dev/ttyUSB0";

    {
        juce::StringArray args;
        for (int i = 1; i < argc; ++i) args.add(juce::String(argv[i]));
        if (auto v = opt(args, "--mac-host"))     macHost    = *v;
        if (auto v = opt(args, "--mac-ip"))       macHost    = *v; // backward compat
        if (auto v = opt(args, "--mac-in-port"))  macInPort  = v->getIntValue();
        if (auto v = opt(args, "--device-id"))    deviceId   = *v;
        if (auto v = opt(args, "--listen-port"))  listenPort = v->getIntValue();
    }

    // ---- Serial ----
    SerialService serial;
    if (serial.connect(serialDev, 115200))
        std::cout << "Serial open @115200: " << serialDev << std::endl;
    else
        std::cout << "FAILED to open serial: " << serialDev << std::endl;

    // ---- OSC (both directions) ----
    OscBridge osc(serial, deviceId);
    if (!osc.start(listenPort, macHost, macInPort)) {
        std::cerr << "Failed to start OSC. Exiting." << std::endl;
        serial.disconnect();
        juce::MessageManager::deleteInstance();
        return 1;
    }

    // ---- Forward serial events upstream ----
    using namespace std::chrono;
    serial.onLine = [&osc, deviceId](juce::String s)
    {
        const int detected = (s == "D") ? 1 : (s == "*") ? 0 : -1;
        if (detected < 0) return;

        static std::atomic<uint32_t> seq { 0 };
        const int seqNow = (int) ++seq;
        osc.onSerialEvent(detected, seqNow, juce::Time::getMillisecondCounter());
    };

    // ---- AUDIO ENGINE (silent) ----
    AudioProcessor audio;
    if (!audio.start(48000.0, 256, 2)) {
        std::cerr << "Failed to start audio. Exiting." << std::endl;
        // ensure LED off before exit
        serial.sendLine("C0");
        osc.stop();
        serial.disconnect();
        juce::MessageManager::deleteInstance();
        return 1;
    }

    std::cout << "Audio running (silent). Press Enter to quit." << std::endl;
    std::cin.get();

    // ---- Clean shutdown ----
    serial.sendLine("C0");   // LED off on exit (safe even if not LED-driven later)
    audio.stop();
    osc.stop();
    serial.disconnect();

    juce::MessageManager::deleteInstance();
    std::cout << "Bye!" << std::endl;
    return 0;
}
