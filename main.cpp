#include <simppl/dispatcher.h>
#include <simppl/interface.h>
#include <simppl/skeleton.h>
#include <simppl/string.h>
#include <simppl/stub.h>

#include <signal.h>

#include <chrono>
#include <ctime>  // localtime
#include <fstream>
#include <iomanip>  // put_time
#include <iostream>
#include <sstream>  // stringstream
#include <string>   // string
#include <thread>
#include <optional>
#include <filesystem>

using namespace simppl::dbus;
namespace wndrecd {
    INTERFACE(WndRecd) {
        Method<in<std::string>> submit;
        Method<> scriptOnline;
        WndRecd() : INIT(submit), INIT(scriptOnline) {}
    };
}  // namespace wndrecd

namespace org::freedesktop {
    INTERFACE(ScreenSaver) {
        Signal<bool> ActiveChanged;
        ScreenSaver() : INIT(ActiveChanged) {}
    };
}  // namespace org::freedesktop

namespace org::kde::kwin {
    INTERFACE(Scripting) {
        Method<in<std::string>,out<bool>> isScriptLoaded;
        Method<in<std::string>,in<std::string>,out<int32_t>> loadDeclarativeScript;
        Method<in<std::string>,in<std::string>,out<int32_t>> loadScript;
        Method<in<std::string>,out<bool>> unloadScript;
        Scripting(): INIT(isScriptLoaded), INIT(loadDeclarativeScript), INIT(loadScript), INIT(unloadScript) {}
    };
    INTERFACE(Script) {
        Method<> run;
        Method<> stop;
        Script(): INIT(run), INIT(stop) {}
    };
}  // namespace org::kde

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
    for(auto & c: msg) c = c == ',' ? '.' : c;
    ss << getTimeStamp() << ", " << msg;
    return ss.str();
}

std::ofstream fout;
bool is_open = false;

void log(std::string msg) {
    msg = createMessage(msg);
    std::cout << msg << std::endl;
    if (is_open) {
        fout << msg << std::endl << std::flush;
    }
}

std::string scriptcode = "\n\
callDBus('wndrecd.WndRecd', '/WndRecd', 'wndrecd.WndRecd', 'scriptOnline');\n\
var nonce = 0;\n\
function actived(client){\n\
    if(client && client.caption) {\n\
        callDBus('wndrecd.WndRecd', '/WndRecd', 'wndrecd.WndRecd', 'submit', client.caption);\n\
        nonce += 1;\n\
        const savedNonce = nonce;\n\
        function captionChanged() {\n\
            if(nonce != savedNonce) {\n\
                client.captionChanged.disconnect(captionChanged);\n\
                return;\n\
            }\n\
            callDBus('wndrecd.WndRecd', '/WndRecd', 'wndrecd.WndRecd', 'submit', client.caption);\n\
        }\n\
        client.captionChanged.connect(captionChanged);\n\
    }\n\
}\n\
workspace.clientActivated.connect(actived);";

std::string emitScript() {
    std::string name("/tmp/wndrecd.script.js");
    std::ofstream fout(name);
    fout << scriptcode << std::flush;
    fout.close();
    return name;
}

Dispatcher * disp;

int main(int argc, char **argv) {
    enable_threads();

    if (argc > 1) {
        std::cerr << "open log file: " << argv[1] << std::endl;
        is_open = true;
        fout.open(argv[1], std::ios_base::app);
    }

    Dispatcher d("bus:session");
    disp = &d;
    
    signal(SIGTERM, [](int signal){disp->stop();});

    Stub<org::freedesktop::ScreenSaver> listener(d, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver");
    listener.connected = [&listener](ConnectionState st) {
        std::cerr << "register listener" << std::endl;
        listener.ActiveChanged.attach() >> [](bool active) {
            if (active) {
                log("scrlock");
            } else {
                log("scrunlock");
            }
        };
    };

    std::cerr << "start receiver" << std::endl;
    Skeleton<wndrecd::WndRecd> skel(d, "wndrecd.WndRecd", "/WndRecd");
    skel.submit >> [&](const std::string &title) {log(title);};
    skel.scriptOnline >> [&](){std::cerr << "script online" << std::endl;};

    auto scriptfile = emitScript();

    Stub<org::kde::kwin::Scripting> scripter(d, "org.kde.KWin", "/Scripting");
    scripter.connected = [&](ConnectionState st) {
        std::cerr << "register kwin script" << std::endl;
        if(scripter.isScriptLoaded("WndRecd")) {
            scripter.unloadScript("WndRecd");
        }
        auto idx = scripter.loadScript(scriptfile.c_str(), "WndRecd");
        if(idx == -1) {
            std::cerr << "unable to load kwin script" << std::endl;
            d.stop();
            return;
        }
        std::cerr << "script index: " << idx << std::endl;
        Dispatcher runner_dispatcher("bus:session");
        Stub<org::kde::kwin::Script> runner(runner_dispatcher, "org.kde.KWin", ("/" + std::to_string(idx)).c_str());
        runner.connected = [&](ConnectionState st) {
            std::cerr << "run script" << std::endl;
            runner.run();
            runner_dispatcher.stop();
        };
        runner_dispatcher.run();
    };

    d.run();
    log("stopped");
    return 0;
}