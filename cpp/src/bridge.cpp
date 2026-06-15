# include "../include/bridge.h"
# include <iostream>
# include <cstring>
# include <cerrno>
# include <csignal>
# include <sys/wait.h>
using namespace std;

Bridge::Bridge(const string& command){
    // two pipes: in = Cpp -> Python , out = Python -> Cpp
    int in_pipe[2], out_pipe[2];

    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0){
        cerr<< "[BRIDGE] pipe() failed: "
            << strerror(errno) << "\n";

        return;
    }

    pid_ = fork();

    if (pid_ == 0){
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
        kill(pid_, SIGTERM);
        waitpid(pid_, nullptr, 0);
    }
}

// wait for {ready : true} from py
bool Bridge::waitForReady(){
    char buf[256];
    if (!fgets(buf, sizeof(buf), reader_)) return false;
    string line(buf);
    return line.find("ready") != string::npos;
}

bool Bridge::writeLine(const string& line){
    if (wfd_ < 0) return false;
    string data = line + "\n";
    ssize_t n = write(wfd_, data.c_str(), data.size());
    return n == static_cast<ssize_t>(data.size());
}

string Bridge::readLine(){
    if (!reader_) return "";
    char buf[1024];
    if (!fgets(buf, sizeof(buf), reader_)) return "";
    string s(buf);

    if (!s.empty() && s.back() == '\n') s.pop_back();
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

double Bridge::score(const string& featureJSON){
    if (!ready_) return FALLBACK_SCORE;

    if (!writeLine(featureJSON)){
        cerr<< "[BRIDGE] write failed - fallback\n";
        ready_ = false;
        return FALLBACK_SCORE;
    }

    string resp = readLine();
    if (resp.empty()){
        cerr<< "[BRIDGE] empty response - fallback\n";
        ready_ = false;
        return FALLBACK_SCORE;
    }

    // parse {score : 0.xxxx} - find number after score:
    auto pos = resp.find("\"score\"");
    if (pos == string::npos){
        cerr<<"[BRIDGE] no score in: " << resp << "\n";
        return FALLBACK_SCORE;
    }
    auto colon = resp.find(':', pos);
    if (colon == string::npos) return FALLBACK_SCORE;

    try {
        double s = stod(resp.substr(colon +1));
        return max(0.0, min(1.0, s));   // clamp to [0,1]
    } catch (...) {
        cerr<< "[BRIDGE] parse failed: "<< resp << "\n";
        return FALLBACK_SCORE;
    }
}