# include "../include/bridge.h"
#include "../vendor/nlohmann/json.hpp"
# include <iostream>
# include <cstring>
# include <cerrno>
# include <csignal>
# include <sys/wait.h>
# include <sys/select.h>
# include <algorithm>
using namespace std;
using json = nlohmann::json;

Bridge::Bridge(const string& command, int timeoutMs)
    : timeoutMs_(timeoutMs > 0 ? timeoutMs : 100)
{
    // two pipes: in = Cpp -> Python , out = Python -> Cpp
    int in_pipe[2], out_pipe[2];

    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0){
        cerr<< "[BRIDGE] pipe() failed: "
            << strerror(errno) << "\n";

        return;
    }

    pid_ = fork();

    if (pid_ == 0){
        setpgid(0, 0);

        // child: becomes the py process
        dup2(in_pipe[0],  STDIN_FILENO);    // read from parent's write end
        dup2(out_pipe[1], STDOUT_FILENO);  // write to parent's read end

        close(in_pipe[1]); close(out_pipe[0]);
        close(in_pipe[0]); close(out_pipe[1]);

        // launch py via shell so PATH is resolved
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);

        // only reached if excel fails
        cerr<< "[BRIDGE] exec failed: "
            << strerror(errno) << "\n";

        _exit(1);
    }

    if (pid_ < 0){
        cerr<< "[BRIDGE] fork() failed\n";
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);

        return;
    }

    // parent: close ends we don't use
    close(in_pipe[0]);  // child reads this end
    close(out_pipe[1]);// choild writes this end

    wfd_ = in_pipe[1];                 // we write here -> python stdin
    rfd_ = out_pipe[0];               // raw fd — no fdopen, no FILE* buffer
    
    ready_ = waitForReady();
    if (ready_)
        cout<< "[BRIDGE] python scorer is ready\n";
    else
        cerr<< "[BRIDGE] pyhton did not send ready signal\n";
}

Bridge::~Bridge(){
    if (wfd_ >= 0) close(wfd_);
    if (rfd_ >= 0) close(rfd_);
    if (pid_ > 0){
        kill(-pid_, SIGTERM);
        for (int i = 0; i < 20; i++) {
            if (waitpid(pid_, nullptr, WNOHANG) == pid_) return;
            usleep(10000);
        }
        kill(-pid_, SIGKILL);
        waitpid(pid_, nullptr, 0);
    }
}

// wait for {ready : true} from py
// generous timeout: the scorer imports heavy libs (shap, xgboost, numba)
// and loads pickled models on startup, which can take tens of seconds on
// slow filesystems (e.g. a WSL /mnt/c mount). readLineWithTimeout returns
// as soon as the ready line arrives, so this only affects the worst case.
bool Bridge::waitForReady(){
    auto [ok, line] = readLineWithTimeout(READY_TIMEOUT_MS);
    if (!ok) return false;
    return line.find("ready") != string::npos;
}

bool Bridge::writeLine(const string& line){
    if (wfd_ < 0) return false;
    string data = line + "\n";
    ssize_t n = write(wfd_, data.c_str(), data.size());
    return n == static_cast<ssize_t>(data.size());
}

pair<bool, string> Bridge::readLineWithTimeout(int timeoutMs) {
    while (pending_.find('\n') == string::npos) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(rfd_, &set);
        timeval tv;
        tv.tv_sec  = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        int rc = select(rfd_ + 1, &set, nullptr, nullptr, &tv);
        if (rc <= 0) return {false, ""};   // timeout or error
        char buf[1024];
        ssize_t n = read(rfd_, buf, sizeof(buf));
        if (n <= 0) return {false, ""};    // EOF or read error
        pending_.append(buf, n);
    }
    auto nl = pending_.find('\n');
    string line = pending_.substr(0, nl);
    pending_.erase(0, nl + 1);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return {true, line};
}

double Bridge::score(const string& featureJSON) {
    if (!ready_) return FALLBACK_SCORE;
    lastExplanation_.clear();

    if (!writeLine(featureJSON)) {
        cerr << "[BRIDGE] write failed\n";
        ready_ = false; return FALLBACK_SCORE;
    }

    auto [ok, resp] = readLineWithTimeout(timeoutMs_);
    if (!ok || resp.empty()) {
        cerr << "[BRIDGE] empty response or timeout after " << timeoutMs_ << "ms\n";
        // Keep the bridge available for the next order; one slow call is not fatal.
        return FALLBACK_SCORE;
    }

    try {
        json j = json::parse(resp);
        if (j.contains("error")) {
            cerr << "[BRIDGE] python error: " << j.value("error","?") << "\n";
            return FALLBACK_SCORE;
        }
        if (!j.contains("score")) { return FALLBACK_SCORE; }
        if (j.contains("explanation"))
            lastExplanation_ = j["explanation"].get<string>();
        double s = j["score"].get<double>();
        return max(0.0, min(1.0, s));
    } catch (...) {
        cerr << "[BRIDGE] JSON parse failed: " << resp << "\n";
        return FALLBACK_SCORE;
    }
}
