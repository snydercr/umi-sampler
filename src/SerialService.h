#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#if JUCE_LINUX
#include "SerialLinux.h"
#endif

class SerialService
{
public:
    // UI/host can subscribe here; gets complete lines ("D", "*", etc.)
    std::function<void(juce::String)> onLine;

    bool connect(const juce::String& devicePath, int baud = 115200)
    {
       #if JUCE_LINUX
        buffer.clear();
        const bool ok = port.open(devicePath.toStdString(), baud);
        if (ok)
        {
            // called from the SerialLinux reader thread whenever bytes arrive
            port.setOnBytes([this](const char* d, int n){ onBytes(d, n); });
        }
        return ok;
       #else
        juce::ignoreUnused(devicePath, baud);
        return false;
       #endif
    }

    void disconnect()
    {
       #if JUCE_LINUX
        port.close();
       #endif
    }

    // Optional: send a text command line (adds '\n')
    void sendLine(const juce::String& s)
    {
        juce::String msg = s;
        msg << "\n";
       #if JUCE_LINUX
        port.writeBytes(msg.toRawUTF8(), (int) msg.getNumBytesAsUTF8());
       #endif
    }

private:
    // accumulate bytes; split on CR/LF; emit complete lines
    void onBytes(const char* data, int len)
    {
        for (int i = 0; i < len; ++i)
        {
            char c = data[i];
            if (c == '\r' || c == '\n')
            {
                if (buffer.isNotEmpty())
                {
                    const juce::String line = buffer.trim();
                    buffer.clear();

                    // Call subscriber (in your console app it’s safe to print here)
                    if (onLine && line.isNotEmpty())
                        onLine(line);
                }
            }
            else
            {
                buffer += juce::String::charToString(c);
            }
        }
    }

   #if JUCE_LINUX
    SerialLinux port;
   #endif
    juce::String buffer;
};
