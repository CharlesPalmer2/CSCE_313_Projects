#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <time.h> // for date and time
#include <fcntl.h>  // For file redirection

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {

    // Save original stdin and stdout
    int in = dup(0);
    int out = dup(1);
    
    // save of previous directory
    string prev_dir;
    // Vector of pids
    vector<pid_t> pids;

    for (;;) {

        // implement iteration over vector of big pid (vector also declared outside loop)
        // waitpid() - using flag for non-blocking
        for (auto i = pids.begin(); i != pids.end(); i++){
            int status = 0;
            status = waitpid(*i, NULL, WNOHANG);
            if (status == *i){
                pids.erase(i);
                i--;    // To make iteration not read nonexistent data after erase is called
            }
        }

        // implement date/time
        time_t rawtime;
        time(&rawtime);
        string date = ctime(&rawtime);
        date = date.substr(4,20);
        // implement useranme
        char* username = getenv("USER");
        // implement current directiry
        char curdir[256];
        getcwd(curdir, sizeof(curdir));
        // display date/time, username, and absolute path to current dir
        cout << YELLOW << date << " " << username << ":" << curdir << "$" << NC << " ";
        
        // get user inputted command
        string input;
        getline(cin >> ws, input);  // >> ws, makes sure cin is empty

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        // for (auto cmd : tknr.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }

        // chdir()
        // if dir (cd <dir>) is "-", then go to previous working directory
        if (tknr.commands[0]->args[0] == "cd"){
            string curr_dir = curdir;
            if (tknr.commands[0]->args[1] == "-"){
                int ch = chdir(prev_dir.c_str());
                if (ch < 0) {  // error check
                perror("change directory");
                exit(2);
                }
            }
            else {
                int ch = chdir(tknr.commands[0]->args[1].c_str());
                if (ch < 0) {  // error check
                perror("change directory");
                exit(2);
                }
            }
            prev_dir = curr_dir;
        }
        else {
            
            for (int i=0; (long unsigned int)i < tknr.commands.size(); i++){

                // Create pipe
                int fds[2];
                pipe(fds);

                // fork to create child
                pid_t pid = fork();
                if (pid < 0) {  // error check
                    perror("fork");
                    exit(2);
                }

                // add check for bg process - add pid to vector if bg don't waipid() in parent
                if(tknr.commands[i]->isBackground()){
                    pids.push_back(pid);
                }

                if (pid == 0) {  // if child, exec to run command
                    
                    if((long unsigned int)i < tknr.commands.size()-1){
                        // In child, redirect output to write end of pipe
                        dup2(fds[1], 1);
                    }
                    // Close the read end of the pipe on the child side.
                    close(fds[0]);
                    // In child, execute the command
                    char** args = new char*[tknr.commands[i]->args.size() + 1];
                    for (long unsigned int j=0; j < tknr.commands[i]->args.size(); j++){
                        args[j] = (char*) tknr.commands[i]->args.at(j).c_str();
                    }
                    args[tknr.commands[i]->args.size()] = nullptr;

                    // if current command is redirected, then open file and dup2 std(in/out) that's being redirected
                    // implement it safely for both at same time
                    if(tknr.commands[i]->hasInput()){
                        int file_in = open(tknr.commands[i]->in_file.c_str(), O_RDONLY, 0666);
                        if (file_in < 0){
                            perror("file_in");
                            exit(2);
                        }
                        dup2(file_in, 0);
                    }
                    if (tknr.commands[i]->hasOutput()){
                        int file_out = open(tknr.commands[i]->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        if (file_out < 0){
                            perror("file_out");
                            exit(2);
                        }
                        dup2(file_out, 1);
                    }

                    if (execvp(args[0], args) < 0) {  // error check
                        perror("execvp");
                        exit(2);
                    }
                }
                else {  // if parent, wait for child to finish
                    
                    // Redirect the SHELL(PARENT)'s input to the read end of the pipe.
                    dup2(fds[0], 0);
                    // Close the write end of the pipe
                    close(fds[1]);

                    if(!(tknr.commands[i]->isBackground())){
                        int status = 0;
                        waitpid(pid, &status, 0);
                        if (status > 1) {  // exit if child didn't exec properly
                            exit(status);
                        }
                    }
                }
            }
        }
        
        // Reset the input and output file descriptors of the parent.
        // Overrite in/out with what was saved.
        dup2(in, 0);
        dup2(out, 1);
    }
}
