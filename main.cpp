#include <iostream>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <cstring>


// commands
int help(int argc, char * argv[]);
int version(int argc, char * argv[]);
int cd(int argc, char * argv[]);
int wc(int argc, char * argv[]);
int tee(int argc, char * argv[]);

namespace aux {
    std::vector<std::string> split(const std::string & s, const std::string & delim) {
        std::vector<std::string> ret;

        size_t start_pos = 0;
        size_t curr_pos = s.find(delim, start_pos);
        while (curr_pos != std::string::npos) {
            ret.push_back(s.substr(start_pos, curr_pos - start_pos));
            start_pos = curr_pos + delim.length();
            curr_pos = s.find(delim, start_pos);
        }
        ret.push_back(s.substr(start_pos, curr_pos));

        return ret;
    }

    char ** to_char_vector(std::vector<std::string> v) {
        char ** cv = new char * [v.size()];
        for (int i = 0; i < v.size(); i++){
            cv[i] = new char[v[i].size() + 1];
            strcpy(cv[i], v[i].c_str());
        }
        return cv;
    }

    struct internal_command {
        std::string command_name;
        int (* command_function)(int, char * []);
        std::string supported_options;
    };

    std::vector<internal_command> internal_commands {
            {"help", &help},
            {"version", &version},
            {"cd", &cd},
            {"wc", &wc, "cwlL"},
            {"tee", &tee, "a"}
    };

    bool validate_option(const std::string & supported_options, char o) {
        size_t pos = supported_options.find(o);
        return pos != std::string::npos;
    }

    struct extracted_args {
        std::string options;
        std::vector<std::string> files;
        std::string error;
    };

    aux::extracted_args extract_args(char * c_name, const std::vector<std::string> & args) {
        std::string supported_options;
        for (const aux::internal_command & i_c: aux::internal_commands) {
            if (i_c.command_name == c_name) {
                supported_options = i_c.supported_options;
            }
        }

        aux::extracted_args new_args;
        for (const std::string & arg: args) {
            // check if option or file
            if (arg[0] == '-') {
                for (int i = 1; i < arg.size(); i++) {
                    char o = arg[i];
                    // check if valid option
                    bool valid = validate_option(supported_options, o);
                    if (!valid) {
                        new_args.error = "Invalid option (-";
                        new_args.error.push_back(o);
                        new_args.error.append(")!\n");
                        return new_args;
                    }
                    // check if the option wasn't listed already
                    if (new_args.options.find(o) == std::string::npos) {
                        new_args.options.push_back(o);
                    }
                }
            } else {
                new_args.files.push_back(arg);
            }
        }

        return new_args;
    }

    char * cwd = nullptr;

    void update_working_directory() {
        delete aux::cwd;
        aux::cwd = new char[1024];
        if (getcwd(aux::cwd, 1024 * sizeof(char)) == nullptr) {
            perror("getcwd() error");
        }
    }
}

// built-in commands v2
int cd(const std::string & options, const std::vector<std::string> & files) {
    if (files.empty()) {
        if (chdir(getenv("HOME")) < 0) {
            perror("cd");
            return -1;
        }
        aux::update_working_directory();
        return 0;
    }
    if (files.size() > 1) {
        std::cerr << "cd: Too many arguments!\n";
        return -1;
    }

    if (chdir(files[0].c_str()) < 0) {
        perror("cd");
        return -1;
    }
    aux::update_working_directory();
    return 0;
}

int wc_aux(int src, int & byte_count, int & word_count, int & line_count, int & max_line_len) {
    char c;
    bool word = false;
    int line_bytes = 0;
    while (true) {
        int r = read(src, &c, 1);
        if (r == -1) {
            std::cout << "wc: Error reading file.\n";
            return -1;
        }
        if (r == 0) {
            break;
        }

        byte_count += 1;
        line_bytes += 1;
        if (c == '\n') {
            line_count += 1;
            word = false;
            if (line_bytes > max_line_len) {
                max_line_len = line_bytes;
            }
            line_bytes = 0;
        } else {
            if (c == ' ' or c == '\t') {
                word = false;
            } else {
                if (!word) {
                    word_count += 1;
                }
                word = true;
            }
        }
    }

    if (line_bytes > max_line_len) {
        max_line_len = line_bytes;
    }
    return 0;
}

int wc(const std::string & options, const std::vector<std::string> & files) {
    bool option_c = false; // byte count
    int byte_count = 0;
    int total_byte_count = 0;

    bool option_w = false; // word count
    int word_count = 0;
    int total_word_count = 0;

    bool option_l = false; // line count
    int line_count = 0;
    int total_line_count = 0;

    bool option_L = false; // len of longest line
    int max_line_len = 0;
    int total_max_line_len = 0;

    for (char o: options) {
        if (o == 'c') {
            option_c = true;
            continue;
        }
        if (o == 'w') {
            option_w = true;
            continue;
        }
        if (o == 'l') {
            option_l = true;
            continue;
        }
        if (o == 'L') {
            option_L = true;
        }
    }

    if (files.empty()) {
        int r = wc_aux(fileno(stdin), byte_count, word_count, line_count, max_line_len);
        if (r == -1){
            return -1;
        }

        if (!option_c and !option_w and !option_l and !option_L) {
            std::cout << line_count << " " << word_count << " " << byte_count;
        } else {
            if (option_l) {
                std::cout << line_count << " ";
            }
            if (option_w) {
                std::cout << word_count << " ";
            }
            if (option_c) {
                std::cout << byte_count << " ";
            }
            if (option_L) {
                std::cout << max_line_len;
            }
        }
        std::cout << std::endl;
    } else {
        FILE * fp;
        for (const std::string & fn: files) {
            byte_count = 0;
            word_count = 0;
            line_count = 0;
            max_line_len = 0;

            fp = fopen(fn.c_str(), "r");
            if (!fp) {
                perror("fopen");
                return EXIT_FAILURE;
            }

            int r = wc_aux(fileno(fp), byte_count, word_count, line_count, max_line_len);
            if (r == -1){
                return -1;
            }

            if (!option_c and !option_w and !option_l and !option_L) {
                std::cout << line_count << " " << word_count << " " << byte_count << " ";
            } else {
                if (option_l) {
                    std::cout << line_count << " ";
                }
                if (option_w) {
                    std::cout << word_count << " ";
                }
                if (option_c) {
                    std::cout << byte_count << " ";
                }
                if (option_L) {
                    std::cout << max_line_len << " ";
                }
            }
            std::cout << fn << std::endl;
            close(fileno(fp));

            if (files.size() > 1) {
                total_line_count += line_count;
                total_word_count += word_count;
                total_byte_count += byte_count;
                if (max_line_len > total_max_line_len) {
                    total_max_line_len = max_line_len;
                }
            }
        }

        if (files.size() > 1) {
            if (!option_c and !option_w and !option_l and !option_L) {
                std::cout << total_line_count << " " << total_word_count << " " << total_byte_count << " ";
            } else {
                if (option_l) {
                    std::cout << total_line_count << " ";
                }
                if (option_w) {
                    std::cout << total_word_count << " ";
                }
                if (option_c) {
                    std::cout << total_byte_count << " ";
                }
                if (option_L) {
                    std::cout << total_max_line_len << " ";
                }
            }
            std::cout << "total" << std::endl;
        }
    }

    // l w c L
    return 0;
}

//int tee(const std::string & options, const std::vector<std::string> & files) {
//    bool option_a = false; // append
//    for (char o: options) {
//        if (o == 'a') {
//            option_a = true;
//        }
//    }
//
//    std::vector<int> dst_fd;
//    for (const std::string & fn: files) {
//        int fd;
//        if (option_a) {
//            fd = open(fn.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
//        } else {
//            fd = open(fn.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
//        }
//        dst_fd.push_back(fd);
//    }
//
//    std::string line;
//    while (true) {
//        std::getline(std::cin, line);
//
//        if (line.empty()) {
//            break;
//        }
//
//        line.push_back('\n');
//
//        std::cout << line;
//
//        for (int fd: dst_fd) {
//            int err = write(fd, line.c_str(), line.length());
//            if (err == -1) {
//                std::cout << "tee: Error writing to file.\n";
//                return -1;
//            }
//        }
//    }
//
//    // close open files
//    for (int fd: dst_fd) {
//        close(fd);
//    }
//    // todo: something seems to be blocking the input; maybe something left in buffer?
////    std::cin >> std::ws;
////    std::getline(std::cin, line);
//    return 0;
//}

//// second implementation
int tee(const std::string & options, const std::vector<std::string> & files) {
    bool option_a = false; // append
    for (char o: options) {
        if (o == 'a') {
            option_a = true;
        }
    }

    int src_fd, err, n;
    unsigned char buffer[4096];

    src_fd = fileno(stdin);
    std::vector<int> dst_fd;
    for (const std::string & fn: files) {
        int fd;
        if (option_a) {
            fd = open(fn.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        } else {
            fd = open(fn.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        }
        dst_fd.push_back(fd);
    }

    while (true) {
        err = read(src_fd, buffer, 4096);
        if (err == -1) {
            std::cout << "tee: Error reading file.\n";
            return -1;
        }

        n = err;
        if (n == 0) {
            break;
        }

        for (int i = 0; i < n; i++) {
            std::cout << buffer[i];
        }

        for (int fd: dst_fd) {
            err = write(fd, buffer, n);
            if (err == -1) {
                std::cout << "tee: Error writing to file.\n";
                return -1;
            }
        }
    }

    // close open files
    for (int fd: dst_fd) {
        close(fd);
    }

    return 0;
}


// built-in commands
int help(int argc, char * argv[]) {
    std::cout << "Available internal commands:\n";
    for (int i = 0; i < aux::internal_commands.size(); i++) {
        aux::internal_command c = aux::internal_commands[i];
        std::cout << i + 1 << ". " << c.command_name;
        if (!c.supported_options.empty()){
            std::cout << ", with arguments: -" << c.supported_options;
        }
        std::cout << std::endl;
    }
    return 0;
}

int version(int argc, char * argv[]) {
    std::cout << "Shello v0.2 (© R Ciama • robert.ciama00@e-uvt.ro)\n";
    std::cout << "[This is a free piece of software which falls under GNU GPL.]\n";
    return 0;
}

int cd(int argc, char * argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    args.erase(args.begin());
    aux::extracted_args ex_args = aux::extract_args(argv[0], args);
    if (!ex_args.error.empty()) {
        std::cerr << "cd: " << ex_args.error;
        return EXIT_FAILURE;
    }
    return cd(ex_args.options, ex_args.files);
}

int wc(int argc, char * argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    args.erase(args.begin());
    aux::extracted_args ex_args = aux::extract_args(argv[0], args);
    if (!ex_args.error.empty()) {
        std::cerr << "wc: " << ex_args.error;
        return EXIT_FAILURE;
    }
    return wc(ex_args.options, ex_args.files);
}

int tee(int argc, char * argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    args.erase(args.begin());
    aux::extracted_args ex_args = aux::extract_args(argv[0], args);
    if (!ex_args.error.empty()) {
        std::cerr << "tee: " << ex_args.error;
        return EXIT_FAILURE;
    }
    return tee(ex_args.options, ex_args.files);
}


struct command {
    std::string name;
    std::vector<std::string> args;
//    std::string in_redir;
    std::string out_redir;
};

std::string get_input(){
    if (aux::cwd == nullptr) {
        aux::update_working_directory();
    }

    std::cout << "\n[" << aux::cwd << "] ";

    char * input = readline(">>> ");
    if (input == nullptr) {
        return "";
    }
    std::string line(input);
    if (!line.empty()) {
        add_history(input);
    }
    free(input);
    return line;
}

command get_command(std::string s) {
    command c;

    // separate redirection
    std::vector<std::string> out_redir_split = aux::split(s, " > ");
    if (out_redir_split.size() == 2) {
        c.out_redir = out_redir_split[1];
        s = out_redir_split[0];
    }

    // separate name by arguments
    std::vector<std::string> vector = aux::split(s, " ");
    c.name = vector[0];
    c.args = vector;

    return c;
}

int run_external_command(int argc, char * argv[]) {
    int pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        // sub

        int er = execvp(argv[0], argv);
        if (er < 0) {
            perror("execvp");
        }
        exit(EXIT_FAILURE);
    }

    int sub_status;
    waitpid(pid, & sub_status, 0);
    return sub_status;
}

int run_command(const command & c) {
    char ** c_args = aux::to_char_vector(c.args);

    // check if command is in builtins, else call external command
    for (const aux::internal_command & i_c: aux::internal_commands){
        if (i_c.command_name == c.name) {
            return i_c.command_function(c.args.size(), c_args);
        }
    }
    return  run_external_command(c.args.size(), c_args);
}

// todo: implement this recursively?
int run_piped_commands(std::vector<std::string> & comms) {
    int temp_in = dup(0);
    int temp_out = dup(1);

    int fd_in = dup(temp_in);
    int fd_out;
    int pid;
    while (!comms.empty()){
        command c = get_command(comms[0]);

        // redirect input
        dup2(fd_in, 0);
        close(fd_in);

        // last command
        if (comms.size() == 1) {
            if (c.out_redir.empty()) {
                fd_out = dup(temp_out);
            } else {
                fd_out = open(c.out_redir.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            }
        } else {
            int fd_pipe[2];
            if (pipe(fd_pipe) < 0) {
                perror("pipe");
                return -1;
            }
            fd_in = fd_pipe[0];
            fd_out = fd_pipe[1];
        }

        dup2(fd_out, 1);
        close(fd_out);

        // execute command in sub
        pid = fork();
        if (pid == 0) {
            int r = run_command(c);
            exit(EXIT_FAILURE);
        }

        // remove command from list
        comms.erase(comms.begin());
    }

    dup2(temp_in, 0);
    close(temp_in);
    dup2(temp_out, 1);
    close(temp_out);

    // wait for last command
    int sub_status;
    waitpid(pid, & sub_status, 0);
    return sub_status;
}

void interpret_input(const std::string & line) {
    if (!line.empty()){
        add_history(line.c_str());
    }
    std::vector<std::string> comms = aux::split(line, " | ");
    if (comms.size() == 1) {
        if (comms[0].length() == 0){
            return;
        }
        if (comms[0] == "exit") {
            std::exit(EXIT_SUCCESS);
        }

        command c = get_command(comms[0]);

        int temp_out, fd_out;

        if (!c.out_redir.empty()) {
            temp_out = dup(fileno(stdout));
            fd_out = open(c.out_redir.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            dup2(fd_out, fileno(stdout));
            close(fd_out);
        }

        int r = run_command(c);

        if (!c.out_redir.empty()) {
            dup2(temp_out, fileno(stdout));
//            close(temp_out);
        }

    } else {
        int r = run_piped_commands(comms);
    }

}

int main() {
    while (true) {
        std::string line = get_input();
        interpret_input(line);
    }
}
