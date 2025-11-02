#pragma once
#include <atomic>
#include <thread>
#include <optional>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_osc/juce_osc.h>
#include "SerialService.h"

class OscBridge
  : private juce::OSCReceiver
  , private juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
public:
    OscBridge(SerialService& serialRef, juce::String deviceId);

    // Bind receiver and connect sender; start hello thread
    bool start(int listenPort, juce::String macHost, int macPort);

    // Stop hello thread, tear down rx/tx
    void stop();

    // Call this from your serial callback to forward events upstream
    void onSerialEvent(int detected, int seqNow, uint32_t monoMs);

private:
    // === RX side ===
    void oscMessageReceived (const juce::OSCMessage& m) override;
    void oscBundleReceived  (const juce::OSCBundle&  b) override;
    void handleMessage      (const juce::OSCMessage& m);
    static void printMessage(const juce::OSCMessage& m);
    static void walkBundle  (const juce::OSCBundle& b, OscBridge* self);

    // === TX side ===
    bool ensureConnected();
    bool send(const juce::OSCMessage& m);

    // hello loop
    void helloLoop();

private:
    SerialService& serial;
    juce::String   deviceId;

    // upstream target
    juce::String macHost;
    int          macPort = 0;

    // receiver listen port
    int listenPort = 0;

    // JUCE OSC sender
    juce::OSCSender sender;
    bool connected = false;

    // hello thread
    std::atomic<bool> running{false};
    std::thread helloThread;
};
