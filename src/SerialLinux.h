#pragma once

// Header-only Linux serial wrapper using termios.
// Minimal, blocking reads with VMIN=1 / VTIME=0 for low latency.

#include <functional>
#include <atomic>
#include <thread>
#include <string>

#if defined(__linux__) && !defined(__ANDROID__)
  #include <fcntl.h>
  #include <termios.h>
  #include <unistd.h>
  #include <errno.h>
  #include <cstring>

class SerialLinux {
public:
    using OnBytes = std::function<void(const char*, int)>;

    SerialLinux() = default;
    ~SerialLinux() { close(); }

    bool open(const std::string& path, int baud = 115200)
    {
        close();

        // open in blocking mode; no O_NONBLOCK so read() waits for at least 1 byte
        fd = ::open(path.c_str(), O_RDWR | O_NOCTTY);
        if (fd < 0) return false;

        if (!configurePortBlocking8N1(fd, baud)) {
            ::close(fd);
            fd = -1;
            return false;
        }

        running = true;
        reader = std::thread([this]{ readerLoop(); });
        return true;
    }

    void close()
    {
        running = false;
        if (reader.joinable()) reader.join();
        if (fd >= 0) { ::close(fd); fd = -1; }
    }

    bool writeBytes(const void* data, int len)
    {
        if (fd < 0) return false;
        const char* p = static_cast<const char*>(data);
        int total = 0;
        while (total < len) {
            ssize_t n = ::write(fd, p + total, len - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            total += (int) n;
        }
        return true;
    }

    void setOnBytes(OnBytes cb) { onBytes = std::move(cb); }

private:
    static bool configurePortBlocking8N1(int fd, int baud)
    {
        termios tio{};
        if (tcgetattr(fd, &tio) != 0) return false;

        // Baud
        speed_t speed = B115200;
        switch (baud) {
            case 9600:   speed = B9600; break;
            case 19200:  speed = B19200; break;
            case 38400:  speed = B38400; break;
            case 57600:  speed = B57600; break;
            case 115200: default: speed = B115200; break;
        }
        cfsetispeed(&tio, speed);
        cfsetospeed(&tio, speed);

        // 8N1, raw, no flow control
        tio.c_cflag &= ~PARENB;
        tio.c_cflag &= ~CSTOPB;
        tio.c_cflag &= ~CSIZE;  tio.c_cflag |= CS8;
        tio.c_cflag |= CLOCAL | CREAD;
        tio.c_cflag &= ~CRTSCTS;

        tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
        tio.c_iflag &= ~(INLCR | ICRNL | IGNCR);
        tio.c_oflag &= ~OPOST;

        // Block until at least 1 byte; no inter-byte timeout
        tio.c_cc[VMIN]  = 1;
        tio.c_cc[VTIME] = 0;

        return tcsetattr(fd, TCSANOW, &tio) == 0;
    }

    void readerLoop()
    {
        char buf[256];
        while (running) {
            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0 && onBytes) onBytes(buf, (int)n);
            else if (n < 0 && errno == EINTR) continue;
        }
    }

    int fd = -1;
    std::atomic<bool> running{false};
    std::thread reader;
    OnBytes onBytes;
};
#else
// Non-Linux stub so code still compiles on other platforms if included accidentally.
class SerialLinux {
public:
    using OnBytes = std::function<void(const char*, int)>;
    bool open(const std::string&, int = 115200) { return false; }
    void close() {}
    bool writeBytes(const void*, int) { return false; }
    void setOnBytes(OnBytes) {}
};
#endif
