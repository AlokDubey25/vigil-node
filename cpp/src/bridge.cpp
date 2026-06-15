# include "../include/bridge.h"
# include <iostream>
# include <cstring>
# include <cerrno>
# include <csignal>
# include <sys/wait.h>

Bridge::Bridge(const string& command){
    // two pipes: in = Cpp -> Python , out = Python -> Cpp
    int in_pipe[2], out_pipe[2];

    if (pipe(in_pipe) != 0 || pipe(out_pipe) = 0){
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
        execl("/bin/sh", "sh". "-c", command.c_str(), nullptr);

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

    wfd_    = in_pipe[1],                // we write here -> python stdin
    reader_ = fdopen(out_pipe[0], "r"), // we read here <- python stdout

    if (!reader_){
        
    }
}