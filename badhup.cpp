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
#include <sys/unistd.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#if __unix__
#include <sys/stat.h>
#endif

using namespace std;
using namespace chrono;

typedef uintmax_t time_x;

struct {
    intmax_t num, den;
} timeSecondConverter;

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
    const uint32_t blockSize = 64 * 1024;  // Размер блока
    const uint32_t countWrite = 312;       // Size write
    int x;
    int fd;
    char* buf;
    std::size_t bufsize;
    uint32_t counter = countWrite;
    string filename = "badhup.";
    time_x (*tick)(void);
    time_x estimated, latency = 0, minLatency = numeric_limits<time_x>::max(), approxAverrageLatency = 0;

    setlocale(LC_ALL, nullptr);

    // Set a clock
    tick = &tick_nanosec;  // select clock dimension

    // nanos to Second
    if (tick == &tick_nanosec) {
        timeSecondConverter.den = std::nano::den;
        timeSecondConverter.num = std::nano::num;
    }
    // micros to Second
    else if (tick == &tick_microsec) {
        timeSecondConverter.den = std::micro::den;
        timeSecondConverter.num = std::micro::num;
    }
    // millis to Second
    else if (tick == &tick_millisec) {
        timeSecondConverter.den = std::milli::den;
        timeSecondConverter.num = std::milli::num;
    }
    // Second base
    else {
        timeSecondConverter.den = 1;
        timeSecondConverter.num = 1;
    }

    // init local time
    diff = tick();

    buf = static_cast<char*>(std::malloc(bufsize = std::max(256u, blockSize)));
    if (buf == nullptr) {
        throw std::bad_alloc();
    }

    getcwd(buf, bufsize);
    if (strlen(buf) == bufsize) {
        if ((buf = static_cast<char*>(std::realloc(buf, bufsize *= 2))) == nullptr) {
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
        x = stat(filename.c_str(), &st);
    } while (!x);

    fd = open(filename.c_str(), O_NOATIME | O_TRUNC | O_CREAT | O_WRONLY);

    if (fd == -1) {
        cout << strerror(errno);
        return EXIT_FAILURE;
    }

    std::memset(buf, ~0, blockSize);

    std::cout << "Timing data..." << std::endl;

    while (counter--) {
        estimated = tick();
        // start write
        write(fd, buf, blockSize);

        estimated = tick() - estimated;

        approxAverrageLatency += estimated;
        latency = std::max(latency, estimated);
        minLatency = std::min(minLatency, estimated);
        /*cout << "\rWrited " << (100 * (countWrite - counter) / countWrite) << "% ("
             << expower_data(blockSize * (countWrite - counter)) << "/" + expower_data(countWrite * blockSize) << ") - "
             << "Current speed "
             << expower_data(static_cast<uintmax_t>(
                    ((blockSize * (countWrite - counter)) / static_cast<long double>(approxAverrageLatency)) *
                    timeSecondConverter.num * timeSecondConverter.den))
             << "/second";
        cout.flush();*/
    }

    estimated = tick();
    x = fdatasync(fd);
    estimated = tick() - estimated; // get wait time
    close(fd);  // close file

    if(x){
        cout << strerror(errno);
        return EXIT_FAILURE;
    }


    // remove file
    std::remove(filename.c_str());

    approxAverrageLatency /= countWrite;

    sprintf(buf, "%f", static_cast<double>(minLatency) * timeSecondConverter.num / timeSecondConverter.den);
    cout << "Min latency: " << buf << " seconds" << endl;
    sprintf(buf, "%f", static_cast<double>(approxAverrageLatency) * timeSecondConverter.num / timeSecondConverter.den);
    cout << "Approx latency: " << buf << " seconds" << endl;
    sprintf(buf, "%f", static_cast<double>(latency) * timeSecondConverter.num / timeSecondConverter.den);
    cout << "Max latency: " << buf << " seconds" << endl;

    std::free(buf);

    cout << ">> Speed "
         << expower_data(static_cast<uintmax_t>((blockSize / static_cast<long double>(estimated)) *
                                                timeSecondConverter.num / timeSecondConverter.den))
         << "/s <<" << endl;
    cout << "Block per write: " << expower_data(blockSize) << endl;
    cout << "Total writed: " << expower_data(static_cast<uintmax_t>(blockSize) * countWrite);
    cout << endl;

    return 0;
}
