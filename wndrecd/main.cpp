#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip> // put_time
#include <string>  // string

#include <simppl/interface.h>
#include <simppl/stub.h>
#include <simppl/skeleton.h>
#include <simppl/dispatcher.h>
#include <simppl/string.h>

namespace wrecd {
    using namespace simppl::dbus;
    INTERFACE(Daemon) {
        Method<in<std::string>> submit;
        Daemon() : INIT(submit) {}
    };
}

std::string getTimeStamp() {
    auto now = std::chrono::system_clock::now();
    auto tp = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&tp), "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3)
        << (std::chrono::duration_cast<std::chrono::milliseconds>(
           now.time_since_epoch()).count() % 1000);
    return ss.str();
}

class Daemon: public simppl::dbus::Skeleton<wrecd::Daemon> {
    std::ofstream * fout;
public:
    Daemon(simppl::dbus::Dispatcher& disp, std::ofstream * _fout=nullptr)
     : simppl::dbus::Skeleton<wrecd::Daemon>(disp, "Daemon"), fout(_fout) {
        submit >> [this](const std::string& title) {
            std::stringstream ss;
            ss << getTimeStamp() << ", " << title << std::endl;
            auto s = ss.str();
            std::cout << s;
            if(fout) *fout << s << std::flush;
        };
    }
};

int main(int argc, char **argv) {
    std::ofstream fout;
    if(argc > 1) {
        fout.open(argv[1], std::ios_base::app);
    }
    simppl::dbus::Dispatcher disp("bus:session");
    Daemon service(disp, &fout);
    disp.run();
    return 0;
}