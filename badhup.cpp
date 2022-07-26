/*

  badhup: program for testing speed HDD, SDD, and RAM

  platforms:
    * Any Unix OS
    * Linux OS
    * Windows
    * MacOS
*/

#include <stdint.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace std;
using namespace chrono;

typedef uintmax_t time_x;

time_x diff = 0;

inline void sleep(time_x ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

inline time_x tick_second(void) { return (duration_cast<seconds>(steady_clock::now().time_since_epoch()).count()) - diff; }

inline time_x tick_millisec(void) {
    return (duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()) - diff;
}

inline time_x tick_microsec(void) {
    return (duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) - diff;
}

inline time_x tick_nanosec(void) { return (duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()) - diff; }

inline string expower_data(uintmax_t bytes) {
    static constexpr int iso = 1024;
    static constexpr char data_kind[5][6]{"bytes", "KiB", "MiB", "GiB", "TiB"};

    /*
    ASSIGN: bytes < 1024 = bytes
           bytes < 1024^2 = KiB
           bytes < 1024^3 = MiB
           bytes < 1024^4 = GiB
           bytes < 1024^5 = TiB
           ~ BigData
    */
    uint32_t x = 0;

    while (bytes >= iso) {
        bytes /= iso;
        ++x;
    }
    return std::to_string(bytes) + " " + data_kind[std::min(x, 5u)];
}

int main() {
    const uint32_t blockWrite = 1024 * 1024;  // Размер блока 1 MiB
    const uint32_t countWrite = 312;          // Size write

    string filename = "test-block-file";
    char* buf;
    uint32_t counter = countWrite;
    struct {
        intmax_t num, den;
    } timeSecondConverter;
    time_x (*tick)(void);
    time_x estimated;
    time_x latency = 0;
    time_x minLatency = numeric_limits<time_x>::max();
    time_x approxAverrageLatency = 0;

    setlocale(LC_ALL, nullptr);

    // Set a clock
    tick = &tick_nanosec;  // select clock dimension

    // millis to Second
    if (tick == &tick_nanosec) {
        timeSecondConverter.den = std::nano::den;
        timeSecondConverter.num = std::nano::num;
    }
    // micros to Second
    else if (tick == &tick_microsec) {
        timeSecondConverter.den = std::micro::den;
        timeSecondConverter.num = std::micro::num;
    } else if (tick == &tick_millisec) {
        timeSecondConverter.den = std::milli::den;
        timeSecondConverter.num = std::milli::num;
    } else {
        timeSecondConverter.den = 1;
        timeSecondConverter.num = 1;
    }

    // init local time
    diff = tick();

    buf = reinterpret_cast<char*>(std::malloc(std::max(128u, blockWrite)));
    if (buf == nullptr) {
        cout << "Bad alloc";
        return EXIT_FAILURE;
    }

    getcwd(buf, 256);
    strcat(buf, "/");
    filename = buf + filename;
    std::filebuf fd;
    fd.open(filename, std::ios::out | std::ios::binary | ios::trunc);
    // int fd = open(filename.c_str(), O_CLOEXEC | O_NOATIME | O_SYNC | O_TRUNC | O_CREAT | O_WRONLY);
    // if (fd == -1)
    if (!fd.is_open()) {
        cout << strerror(errno);
        return EXIT_FAILURE;
    }

    std::memset(buf, ~0, blockWrite);

    while (counter--) {
        estimated = tick();
        // start write
        std::size_t nwritten = fd.sputn(buf, blockWrite);
        fd.pubsync();
        estimated = tick() - estimated;

        approxAverrageLatency += estimated;
        latency = std::max(latency, estimated);
        minLatency = std::min(minLatency, estimated);
        cout << "\rWrited " << (100 * (countWrite - counter) / countWrite) << "% ("
             << expower_data(blockWrite * (countWrite - counter)) << "/" + expower_data(countWrite * blockWrite) << ") - "
             << "Current speed "
             << expower_data(static_cast<uintmax_t>(
                    ((blockWrite * (countWrite - counter)) / static_cast<long double>(approxAverrageLatency)) *
                    timeSecondConverter.num * timeSecondConverter.den))
             << "/second";
        cout.flush();
    }

    fd.close();  // close file
    // remove file

    std::remove(filename.c_str());

    cout << "\r\0";

    approxAverrageLatency /= countWrite;

    sprintf(buf, "%.32f", static_cast<double>(minLatency) * timeSecondConverter.num / timeSecondConverter.den);
    cout << "Min latency: " << buf << " seconds" << endl;
    sprintf(buf, "%.32f", static_cast<double>(approxAverrageLatency) * timeSecondConverter.num / timeSecondConverter.den);
    cout << "Approx latency: " << buf << " seconds" << endl;
    sprintf(buf, "%.32f", static_cast<double>(latency) * timeSecondConverter.num / timeSecondConverter.den);
    cout << "Max latency: " << buf << " seconds" << endl;
    std::free(buf);

    cout << ">>Speed "
         << expower_data(static_cast<uintmax_t>((blockWrite / static_cast<long double>(approxAverrageLatency)) *
                                                timeSecondConverter.num * timeSecondConverter.den))
         << "/seconds<<" << endl;
    cout << "Block per write: " << expower_data(blockWrite) << endl;
    cout << "Total writed: " << expower_data(static_cast<uintmax_t>(blockWrite) * countWrite);
    cout << endl;

    return 0;
}
