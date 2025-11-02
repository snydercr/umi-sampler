#include "OscBridge.h"
#include <cmath>
using namespace std::chrono_literals;

OscBridge::OscBridge(SerialService& serialRef, juce::String id)
: serial(serialRef), deviceId(std::move(id))
{
    // Ensure JUCE message manager exists (safe if already created)
    juce::MessageManager::getInstance();
}

bool OscBridge::start(int listenPortIn, juce::String macHostIn, int macPortIn)
{
    listenPort = listenPortIn;
    macHost    = std::move(macHostIn);
    macPort    = macPortIn;

    if (! connect(listenPort)) {
        std::cerr << "ERROR: couldn't bind OSC receiver on UDP " << listenPort << std::endl;
        return false;
    }
    addListener(this); // all addresses
    std::cout << "OSC RX listening on UDP " << listenPort << std::endl;

    connected = sender.connect(macHost, macPort);
    if (connected)
        std::cout << "OSC TX connected to " << macHost << ":" << macPort << std::endl;
    else
        std::cout << "WARN: initial connect(" << macHost << ":" << macPort << ") failed" << std::endl;

    running = true;
    helloThread = std::thread(&OscBridge::helloLoop, this);
    return true;
}

void OscBridge::stop()
{
    running = false;
    if (helloThread.joinable()) helloThread.join();

    removeListener(this);
    disconnect();

    sender.disconnect();
    connected = false;
}

void OscBridge::helloLoop()
{
    uint32_t helloSeq = 0;
    while (running)
    {
        juce::OSCMessage hello("/umi/hello");
        hello.addString(deviceId);
        hello.addInt32(listenPort);
        hello.addInt32((int) ++helloSeq);

        if (!send(hello))
            std::cout << "WARN: send(/umi/hello) failed" << std::endl;
        else
            std::cout << "TX /umi/hello id=" << deviceId << " seq=" << helloSeq << std::endl;

        juce::Thread::sleep(5000);
    }
}

bool OscBridge::ensureConnected()
{
    if (connected) return true;
    connected = sender.connect(macHost, macPort);
    return connected;
}

bool OscBridge::send(const juce::OSCMessage& m)
{
    if (!ensureConnected()) return false;
    if (!sender.send(m)) {
        sender.disconnect();
        connected = false;
        // one quick retry after reconnect
        if (!ensureConnected()) return false;
        return sender.send(m);
    }
    return true;
}

void OscBridge::onSerialEvent(int detected, int seqNow, uint32_t monoMs)
{
    juce::OSCMessage event("/umi/pcell");
    event.addString(deviceId);
    event.addInt32(seqNow);
    event.addInt32(detected);
    event.addInt32((int) monoMs);

    if (!send(event))
        std::cout << "WARN: send(/umi/pcell) failed (seq=" << seqNow << ")" << std::endl;
    else
        std::cout << "TX /umi/pcell id=" << deviceId
                  << " det=" << detected
                  << " seq=" << seqNow
                  << " ts="  << monoMs
                  << std::endl;
}

// ===== RX side =====
void OscBridge::printMessage(const juce::OSCMessage& m)
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

void OscBridge::handleMessage(const juce::OSCMessage& m)
{
    printMessage(m);

    // For now: react to /umi/led <int 0|1>
    if (m.getAddressPattern().toString() == "/umi/led" && m.size() >= 1)
    {
        int onOff = 0;
        if      (m[0].isInt32())   onOff = m[0].getInt32();
        else if (m[0].isFloat32()) onOff = (int) std::lround(m[0].getFloat32());

        // Persisted state: 1 = on (C5), 0 = off (C0)
        serial.sendLine(onOff != 0 ? "C5" : "C0");
    }

    // Later: route more addresses to your sampler, e.g. /umi/note, /umi/state, etc.
}

void OscBridge::walkBundle(const juce::OSCBundle& b, OscBridge* self)
{
    for (int i = 0; i < b.size(); ++i) {
        const auto& el = b[i];
        if (el.isMessage()) self->handleMessage(el.getMessage());
        else if (el.isBundle()) walkBundle(el.getBundle(), self);
    }
}

void OscBridge::oscMessageReceived(const juce::OSCMessage& m) { handleMessage(m); }
void OscBridge::oscBundleReceived (const juce::OSCBundle&  b) { walkBundle(b, this); }
