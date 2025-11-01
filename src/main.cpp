#include <iostream>
#include <atomic>
#include <thread>
#include <optional>
#include <cmath>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_osc/juce_osc.h>

#include "Engine.h"
#include "SerialService.h"

// ---- tiny CLI helper ----
static std::optional<juce::String> opt (const juce::StringArray& a, const juce::String& key)
{
    for (int i = 0; i < a.size(); ++i)
        if (a[i] == key && i + 1 < a.size())
            return a[i + 1];
    return std::nullopt;
}

// ---- minimal connect-on-demand wrapper for OSCSender ----
struct OscLink {
    juce::OSCSender sender;
    juce::String host;
    int port = 0;
    bool connected = false;

    OscLink(juce::String h, int p) : host(std::move(h)), port(p) {}

    bool ensure() {
        if (connected) return true;
        connected = sender.connect(host, port);
        return connected;
    }

    bool send(const juce::OSCMessage& m) {
        if (!ensure()) return false;
        if (!sender.send(m)) {
            sender.disconnect();
            connected = false;
            return false;
        }
        return true;
    }

    void disconnect() {
        sender.disconnect();
        connected = false;
    }
};

// ---- super-simple OSC dump receiver (prints anything it gets), now with bundle support ----
struct DumpRx :
    juce::OSCReceiver,
    juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
    // pretty-print a single OSC message
    static void printMessage(const juce::OSCMessage& m)
    {
        const auto addr = m.getAddressPattern().toString();
        std::cout << "RX addr=" << addr << " argc=" << m.size();
        for (int i = 0; i < m.size(); ++i)
        {
            if      (m[i].isInt32())   std::cout << "  arg" << i << "=int:"   << m[i].getInt32();
            else if (m[i].isFloat32()) std::cout << "  arg" << i << "=float:" << m[i].getFloat32();
            else if (m[i].isString())  std::cout << "  arg" << i << "=str:"   << m[i].getString().toStdString();
            else if (m[i].isBlob())    std::cout << "  arg" << i << "=blob("  << m[i].getBlob().getSize() << "B)";
            else                       std::cout << "  arg" << i << "=?";
        }
        std::cout << std::endl;
    }

    // recursively walk a bundle and print all contained messages
    static void walkBundle(const juce::OSCBundle& b)
    {
        for (int i = 0; i < b.size(); ++i)
        {
            const auto& el = b[i];
            if (el.isMessage())
                printMessage(el.getMessage());
            else if (el.isBundle())
                walkBundle(el.getBundle());
        }
    }

    // JUCE will call this for single messages
    void oscMessageReceived (const juce::OSCMessage& m) override
    {
        printMessage(m);
    }

    // JUCE will call this for bundles (what your tcpdump shows: "#bundle")
    void oscBundleReceived (const juce::OSCBundle& b) override
    {
        walkBundle(b);
    }
};

int main (int argc, char** argv)
{
    juce::ignoreUnused(argc, argv);

    // Ensure a MessageManager exists (fixes AsyncUpdater/MIDI asserts)
    juce::MessageManager::getInstance();

    std::cout << "UMI Sampler skeleton starting..." << std::endl;

    // ---- SERIAL: create and connect first ----
    SerialService serial;
    const juce::String dev = "/dev/ttyUSB0";
    if (serial.connect(dev, 115200))
        std::cout << "Serial open @115200: " << dev << std::endl;
    else
        std::cout << "FAILED to open serial: " << dev << std::endl;

    // ---- OSC config (Pi -> Mac), parse CLI ----
    juce::String macIp     = "192.168.1.100";  // override via --mac-ip
    int          macInPort = 9000;             // override via --mac-in-port
    juce::String deviceId  = "pi-01";          // override via --device-id
    int          listenPort = 9100;            // NEW: override via --listen-port

    {
        juce::StringArray args;
        for (int i = 1; i < argc; ++i) args.add(juce::String(argv[i]));
        if (auto v = opt(args, "--mac-ip"))       macIp      = *v;
        if (auto v = opt(args, "--mac-in-port"))  macInPort  = v->getIntValue();
        if (auto v = opt(args, "--device-id"))    deviceId   = *v;
        if (auto v = opt(args, "--listen-port"))  listenPort = v->getIntValue();
    }

    std::cout << "OSC upstream target: " << macIp << ":" << macInPort
              << " deviceId=" << deviceId << std::endl;

    // ---- bring up the simple OSC receiver ----
    DumpRx rx;
    if (!rx.connect(listenPort)) {
        std::cerr << "ERROR: couldn't bind OSC receiver on UDP " << listenPort << std::endl;
    } else {
        rx.addListener(&rx); // listen to ALL addresses
        std::cout << "OSC receiver listening on UDP " << listenPort << " (printing all messages)" << std::endl;
    }

    OscLink toMac(macIp, macInPort);
    if (toMac.ensure())
        std::cout << "Connected to Mac at " << macIp << ":" << macInPort << std::endl;
    else
        std::cout << "WARN: initial connect(" << macIp << ":" << macInPort << ") failed" << std::endl;

    // ---- periodic self-registration (“hello”) ----
    std::atomic<bool> running { true };
    std::thread helloThread([&]{
        uint32_t helloSeq = 0;
        while (running) {
            juce::OSCMessage hello("/umi/hello");
            hello.addString(deviceId);
            hello.addInt32(listenPort);           // advertise our listen port
            hello.addInt32((int) ++helloSeq);     // sequence for visibility

            if (!toMac.send(hello)) {
                std::cout << "WARN: send(/umi/hello) failed" << std::endl;
            } else {
                std::cout << "TX /umi/hello id=" << deviceId
                          << " seq=" << helloSeq << std::endl;
            }
            juce::Thread::sleep(5000); // every 5s
        }
    });

    using namespace std::chrono_literals;

    // ---- serial line handler: keep tiny LED blink + add OSC on D/* ----
    serial.onLine = [&serial, &toMac, deviceId](juce::String s)
    {
        // quick local visual on 'D' so you see activity even without the Mac
        if (s == "D") {
            static std::atomic<bool> busy { false };
            bool expected = false;
            if (busy.compare_exchange_strong(expected, true)) {
                serial.sendLine("C5"); // quick blue
                std::thread([&serial]{
                    std::this_thread::sleep_for(250ms);
                    serial.sendLine("C0"); // off
                    std::this_thread::sleep_for(150ms);
                    busy.store(false);
                }).detach();
            }
        }

        // Send /umi/prox upstream on both 'D' (1) and '*' (0)
        const int detected = (s == "D") ? 1 : (s == "*") ? 0 : -1;
        if (detected >= 0) {
            static std::atomic<uint32_t> seq { 0 };
            const int seqNow = (int) ++seq;

            juce::OSCMessage prox("/umi/prox");
            prox.addString(deviceId);
            prox.addInt32(seqNow);
            prox.addInt32(detected);
            prox.addInt32((int) juce::Time::getMillisecondCounter()); // 32-bit monotonic ms

            if (!toMac.send(prox)) {
                std::cout << "WARN: send(/umi/prox) failed (seq=" << seqNow << ")" << std::endl;
            } else {
                std::cout << "TX /umi/prox id=" << deviceId
                          << " det=" << detected
                          << " seq=" << seqNow
                          << " ts=" << juce::Time::getMillisecondCounter()
                          << std::endl;
            }
        }
    };

    // ---- AUDIO ENGINE (silent) ----
    Engine engine;
    if (!engine.start(48000.0, 256, 2)) {
        std::cerr << "Failed to start audio. Exiting." << std::endl;
        running = false;
        if (helloThread.joinable()) helloThread.join();
        rx.removeListener(&rx);
        rx.disconnect();
        serial.disconnect();
        toMac.disconnect();
        juce::MessageManager::deleteInstance();
        return 1;
    }

    std::cout << "Audio running (silent)." << std::endl;
    std::cout << "Listening on " << dev << " — wave at the sensor to see D/*" << std::endl;
    std::cout << "Press Enter to quit." << std::endl;

    // Block until user quits (serial + hello threads keep running)
    std::cin.get();

    // ---- Clean shutdown ----
    engine.stop();
    running = false;
    if (helloThread.joinable()) helloThread.join();
    rx.removeListener(&rx);
    rx.disconnect();
    serial.disconnect();
    toMac.disconnect();

    juce::MessageManager::deleteInstance();
    std::cout << "Bye!" << std::endl;
    return 0;
}
