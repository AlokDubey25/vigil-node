# pragma once
# include <string>
# include <utility>
# include <unistd.h>
# include <sys/types.h>
using namespace std;

class Bridge{
public:
    // command = "python3 python/bridge/scorer.py"
    // run ./vigil from root so relative path works
    explicit Bridge(const string& command, int timeoutMs = 100);
    ~Bridge();

    // true after python sends {ready : true}
    bool isReady() const { return ready_;}

    // send featureJSON (from fv.toJSON()), get fraud prob back
    // returns FALLBACK_SCORE if python is down or slow
    double score(const string& featureJSON);

    // used when python is unavailable
    static constexpr double FALLBACK_SCORE = 0.5;

    string getLastExplanation() const { return lastExplanation_; }

private:
    pid_t pid_    = -1;        // Python process ID
    int   wfd_    = -1;       // write fd -> Python stdin
    int    rfd_   = -1;      // raw read fd ← Python stdout
    FILE* reader_ = nullptr;// read FILE <- Python stdout
    bool  ready_  = false;
    int   timeoutMs_ = 100;

    bool   waitForReady();
    bool   writeLine(const string& line);
    pair<bool, string> readLineWithTimeout(int timeoutMs);

    string lastExplanation_;
};
