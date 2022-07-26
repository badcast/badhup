/*

  badhup: program for testing speed HDD, SDD, and RAM

  platforms:
    * Any Unix OS
    * Linux OS
    * Windows
    * MacOS
*/

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <sys/unistd.h>

#if __unix__
#include <sys/stat.h>
#endif

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
    const uint32_t blockSize = 4096*1024*20;  // Размер блока
    const uint32_t countWrite = 312;   // Size write
    int x;
    int fd;
    char* buf;
    std::size_t bufsize;
    uint32_t counter = countWrite;
    struct {
        intmax_t num, den;
    } timeSecondConverter;
    time_x (*tick)(void);
    time_x estimated;
    time_x latency = 0;
    time_x minLatency = numeric_limits<time_x>::max();
    time_x approxAverrageLatency = 0;
    string filename = "badhup.";

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

    buf = reinterpret_cast<char*>(std::malloc(bufsize = std::max(256u, blockSize)));
    if (buf == nullptr) {
        throw std::bad_alloc();
    }

    getcwd(buf, bufsize);
    if (strlen(buf) == bufsize) {
        buf = reinterpret_cast<char*>(std::realloc(buf, bufsize *= 2));
        if (buf == nullptr) {
            throw std::bad_alloc();
        }
        getcwd(buf, bufsize);
    }
    strcat(buf, "/");
    filename = buf + filename;
    filename.resize(filename.size() + 7, ' ');
    srand(time(nullptr));
    struct stat st;
    do {
        for (x = 0; x < 8; ++x) {
            filename[filename.size() - x] = static_cast<char>(rand() % 26 + 'a');
        }
        x=stat(filename.c_str(), &st);
    } while (!x);

    //  std::fstream fd;
    // fd.open(filename, std::ios::out | std::ios::binary | ios::trunc);

    fd = open(filename.c_str(), O_NOATIME | O_SYNC | O_TRUNC | O_CREAT | O_WRONLY);

    if (fd == -1) {
        cout << strerror(errno);
        return EXIT_FAILURE;
    }

    std::memset(buf, ~0, blockSize);

    while (counter--) {
        estimated = tick();
        // start write
        write(fd, buf, blockSize);

        estimated = tick() - estimated;

        approxAverrageLatency += estimated;
        latency = std::max(latency, estimated);
        minLatency = std::min(minLatency, estimated);
        cout << "\rWrited " << (100 * (countWrite - counter) / countWrite) << "% ("
             << expower_data(blockSize * (countWrite - counter)) << "/" + expower_data(countWrite * blockSize) << ") - "
             << "Current speed "
             << expower_data(static_cast<uintmax_t>(
                    ((blockSize * (countWrite - counter)) / static_cast<long double>(approxAverrageLatency)) *
                    timeSecondConverter.num * timeSecondConverter.den))
             << "/second";
        cout.flush();
    }

    close(fd);  // close file
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
         << expower_data(static_cast<uintmax_t>((blockSize / static_cast<long double>(approxAverrageLatency)) *
                                                timeSecondConverter.num * timeSecondConverter.den))
         << "/seconds<<" << endl;
    cout << "Block per write: " << expower_data(blockSize) << endl;
    cout << "Total writed: " << expower_data(static_cast<uintmax_t>(blockSize) * countWrite);
    cout << endl;

    return 0;
}
