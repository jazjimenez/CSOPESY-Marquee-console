#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <queue>

// --- Shared State and Thread Control ---
std::atomic<bool> is_running{true};
std::atomic<bool> marquee_running{false};
std::string marquee_window = "";
std::atomic<int> marquee_speed{200}; // default 200 ms

/**
 * We will use three threads:
 * 1. Keyboard Handler Thread (reads user input)
 * 2. Marquee Logic Thread (updates marquee position)
 * 3. Display Thread (renders marquee and prompt)
 */

// The command interpreter and display thread share this variable.
std::string prompt_display_buffer = "";
std::mutex prompt_mutex;

// Shared state for the keyboard handler and command interpreter.
std::queue<std::string> command_queue;
std::mutex command_queue_mutex;

// The marquee logic thread and display thread share this variable.
std::string marquee_display_buffer = "This is a scrolling marquee text! ";
std::mutex marquee_to_display_mutex;

// Display parameters
int display_width = 40; //change niyo yung width kung naliliitan kayo
int marquee_pos = 0;

// --- Utility Function ---
void gotoxy(int x, int y) {
    std::cout << "\033[" << y << ";" << x << "H";
}

// --- Thread Functions ---
void keyboard_handler_thread_func() {
    std::string command_line;
    while (is_running) {
        std::getline(std::cin, command_line);
        if (!command_line.empty()) {
            std::unique_lock<std::mutex> lock(command_queue_mutex);
            command_queue.push(command_line);
        }
    }
}

void marquee_logic_thread_func() {
    while (is_running) {
        if (marquee_running) {
            std::string text;
            {
                std::unique_lock<std::mutex> lock(marquee_to_display_mutex);
                text = marquee_display_buffer;
            }

            if (!text.empty()) {
                // String from right to left
                char first = text[0];
                text.erase(0, 1);
                text.push_back(first);

                {
                    std::unique_lock<std::mutex> lock(marquee_to_display_mutex);
                    marquee_display_buffer = text; // update stored text
                }

                // Truncates or pad
                std::string window = text;
                if ((int)window.size() < display_width) {
                    window += std::string(display_width - window.size(), ' ');
                } else if ((int)window.size() > display_width) {
                    window = window.substr(0, display_width);
                }

                {
                    std::unique_lock<std::mutex> lock(prompt_mutex);
                    prompt_display_buffer = window;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(marquee_speed));
    }
}


void display_thread_func() {
    const int refresh_rate_ms = 50;
    while (is_running) {
        {
            std::unique_lock<std::mutex> lock(prompt_mutex);
            gotoxy(0, 0);
            std::cout << std::setw(display_width) << std::left << prompt_display_buffer << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_rate_ms));
    }
}

void command_process(const std::string& command_line) {
    std::istringstream iss(command_line);
    std::string cmd;
    iss >> cmd;

    if(cmd == "help"){
        std::cout << "Available commands:\n"
                  << "  help          - show this help message\n"
                  << "  start_marquee - start marquee animation\n"
                  << "  stop_marquee  - stop marquee animation\n"
                  << "  set_text      - set marquee text\n"
                  << "  set_speed     - set marquee animation speed (ms)\n"
                  << "  exit          - exit emulator\n";
    } else if(cmd == "start_marquee"){
        marquee_running = true;
    } else if(cmd == "stop_marquee"){
        marquee_running = false;
    } else if (cmd == "set_text") {
        std::string new_text;
        std::getline(iss, new_text);
        if (!new_text.empty() && new_text[0] == ' ') new_text.erase(0, 1);
        if (!new_text.empty()) {
            std::unique_lock<std::mutex> lock(marquee_to_display_mutex);
            marquee_display_buffer = new_text;
            display_width = static_cast<int>(new_text.size() + 5);
            gotoxy(1, 5);
            // marquee_pos = 0;
        }
    } else if (cmd == "set_speed") {
        int speed;
        if (iss >> speed && speed > 0) {
            marquee_speed = speed;
        } else {
            std::cout << "Invalid speed.\n";
        }
    } else if (cmd == "exit") {
        is_running = false;
    } else {
        std::cout << "Unknown command. Type 'help' for a list of commands.\n";
    }
}

int main() {
    std::thread marquee_logic_thread(marquee_logic_thread_func);
    std::thread display_thread(display_thread_func);
    std::thread keyboard_handler_thread(keyboard_handler_thread_func);

    //COMMAND INTERPRETER
    while(is_running) {
        std::string command_line;
        {
            std::unique_lock<std::mutex> lock(command_queue_mutex);
            if (!command_queue.empty()) {
                command_line = command_queue.front();
                command_queue.pop();
            }
        }

        if (!command_line.empty()) {
            command_process(command_line);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Join threads
    if (marquee_logic_thread.joinable()) marquee_logic_thread.join();
    if (display_thread.joinable()) display_thread.join();
    if (keyboard_handler_thread.joinable()) keyboard_handler_thread.join();


    std::cout << "\n\nGoodbye!\n";
    return 0;
}