# include "../include/bridge.h"
#include "../vendor/nlohmann/json.hpp"
# include <iostream>
# include <cstring>
# include <cerrno>
# include <csignal>
# include <sys/wait.h>
# include <sys/select.h>
# include <algorithm>
# include <unistd.h>
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

    wfd_    = in_pipe[1];                // we write here -> python stdin
    reader_ = fdopen(out_pipe[0], "r"); // we read here <- python stdout

    if (!reader_){
        cerr<<  "[BRIDGE] fdopen failed\n";
        close(wfd_);
        return;
    }

    ready_ = waitForReady();
    if (ready_)
        cout<< "[BRIDGE] python scorer is ready\n";
    else
        cerr<< "[BRIDGE] pyhton did not send ready signal\n";
}

Bridge::~Bridge(){
    if (wfd_ >= 0) close(wfd_);
    if (reader_)   fclose(reader_);
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
bool Bridge::waitForReady(){
    auto [ok, line] = readLineWithTimeout(5000);
    if (!ok) return false;
    return line.find("ready") != string::npos;
}

bool Bridge::writeLine(const string& line){
    if (wfd_ < 0) return false;
    string data = line + "\n";
    ssize_t n = write(wfd_, data.c_str(), data.size());
    return n == static_cast<ssize_t>(data.size());
}

pair<bool, string> Bridge::readLineWithTimeout(int timeoutMs){
    if (!reader_) return {false, ""};

    int fd = fileno(reader_);
    if (fd < 0) return {false, ""};

    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);

    timeval timeout{};
    timeout.tv_sec  = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    int rc = select(fd + 1, &set, nullptr, nullptr, &timeout);
    if (rc <= 0) return {false, ""};

    char buf[1024];
    if (!fgets(buf, sizeof(buf), reader_)) return {false, ""};
    string s(buf);

    if (!s.empty() && s.back() == '\n') s.pop_back();
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return {true, s};
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
