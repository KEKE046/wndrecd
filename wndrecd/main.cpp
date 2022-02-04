#include <simppl/dispatcher.h>
#include <simppl/interface.h>
#include <simppl/skeleton.h>
#include <simppl/string.h>
#include <simppl/stub.h>

#include <chrono>
#include <ctime>  // localtime
#include <fstream>
#include <iomanip>  // put_time
#include <iostream>
#include <sstream>  // stringstream
#include <string>   // string
#include <thread>

namespace wrecd {
using namespace simppl::dbus;
INTERFACE(Daemon) {
    Method<in<std::string>> submit;
    Signal<bool> ActiveChanged;
    Daemon() : INIT(submit), INIT(ActiveChanged) {}
};
}  // namespace wrecd

namespace org {
namespace freedesktop {
using namespace simppl::dbus;
INTERFACE(ScreenSaver) {
    Method<out<bool>> GetActive;
    Method<out<uint32_t>> GetActiveTime;
    Method<out<uint32_t>> GetSessionIdleTime;
    Method<in<std::string>, in<std::string>, out<uint32_t>> Inhibit;
    Method<> Lock;
    Method<in<bool>,out<bool>> SetActive;
    Method<> SimulateUserActivity;
    Method<in<std::string>,in<std::string>,out<uint32_t>> Throttle;
    Method<in<uint32_t>> UnInhibit;
    Method<in<uint32_t>> UnThrottle;
    Signal<bool> ActiveChanged;
    ScreenSaver() : INIT(GetActive), INIT(GetActiveTime), INIT(GetSessionIdleTime), INIT(Inhibit), 
        INIT(Lock), INIT(SetActive), INIT(SimulateUserActivity), INIT(Throttle), INIT(UnInhibit), INIT(UnThrottle), INIT(ActiveChanged) {}
};
}  // namespace freedesktop
}  // namespace org

std::string getTimeStamp() {
    auto now = std::chrono::system_clock::now();
    auto tp = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&tp), "%Y-%m-%d %H:%M:%S") << "."
       << std::setfill('0') << std::setw(3)
       << (std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
               .count() %
           1000);
    return ss.str();
}

std::string createMessage(std::string msg) {
    std::stringstream ss;
    ss << getTimeStamp() << ", " << msg;
    return ss.str();
}

std::ofstream fout;
bool is_open = false;

void log(std::string msg) {
    msg = createMessage(msg);
    std::cout << msg << std::endl;
    if(is_open) {
        fout << msg << std::endl << std::flush;
    }
}

int main(int argc, char **argv) {
    if(argc > 1) {
        std::cerr << "open log file: " << argv[1] << std::endl;
        is_open = true;
        fout.open(argv[1], std::ios_base::app);
    }
    simppl::dbus::enable_threads();
    std::cerr << "start receiver" << std::endl;
    simppl::dbus::Dispatcher d("bus:session");
    simppl::dbus::Skeleton<wrecd::Daemon> skel(d, "Daemon");
    simppl::dbus::Stub<org::freedesktop::ScreenSaver> listener(d, "org.freedesktop.ScreenSaver", "wrecd.Daemon.Daemon");
    listener.connected = [&listener](simppl::dbus::ConnectionState st) {
        std::cerr << "register listener" << std::endl;
        listener.ActiveChanged.attach() >> [](bool active) {
            if(active) {
                log("scrlock, scrlock");
            }
            else {
                log("scrunlock, scrunlock");
            }
        };
    };
    bool ac = false;
    skel.submit >> [&ac, &skel](const std::string &title) {
        log("activate, " + title);
        skel.ActiveChanged.notify(ac^=1);
    };
    d.run();
    return 0;
}