#include "picosha2.h"
#include <nlohmann/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <thread>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <mutex>
#include <iostream>
#include <csignal>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <boost/log/utility/setup/console.hpp>
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
using nlohmann::json;
std::chrono::time_point<std::chrono::system_clock> begin;
std::mutex mut;
json array;
std::string file_name;
using backend_type = sinks::text_file_backend;
const char alpha_num[] = "0123456789AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz";

std::vector<unsigned char> generator_random() {
    std::vector<unsigned char> vector;
    vector.reserve(4);
    for (size_t i = 0; i < 4; i++) {
        vector.push_back(alpha_num[rand() % (sizeof(alpha_num) - 1)]);
    }
    return vector;
}

void init() {
    auto traceFile = boost::log::add_file_log(
            boost::log::keywords::file_name = "Logs/file_%5N.log",
            boost::log::keywords::rotation_size = 5 * 1024 * 1024,
            boost::log::keywords::format = "[%TimeStamp%]: %Message%");
    traceFile->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::trace);
// trace for console
    auto traceConsole = boost::log::add_console_log(
            std::cout,
            boost::log::keywords::format = "[%TimeStamp%]: %Message%");
    traceConsole->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::trace);
    auto infoConsole = boost::log::add_console_log(
            std::cout,
            boost::log::keywords::format = "[%TimeStamp%]: %Message%");
    infoConsole->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::info);
    boost::log::add_common_attributes();
}

void add_json(const std::string& hash, const std::string& r_string) {
    mut.lock();
    json j;
    auto time = std::chrono::system_clock::now();
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(time - begin).count();
    j["hash"] = hash;
    j["data"] = r_string;
    array.push_back(j);
    mut.unlock();
}

[[noreturn]] void hash() {
    for(;;) {
        std::vector<unsigned char> temp = generator_random();
        std::string r_string;
        for (const auto &elem : temp){
            r_string += elem;
        }
        std::string hash = picosha2::hash256_hex_string(temp);
        if ((hash[63] == '0') && (hash[62] == '0') && (hash[61] == '0') && (hash[60] == '0')) {
            add_json(hash, r_string);
            BOOST_LOG_TRIVIAL(trace) << "Correct value: " << r_string << "  Hash: " << hash;
        }
// else {
// BOOST_LOG_TRIVIAL(info) « "Correct value: " « r_string « ", Hash: " « hash;
// }
        r_string = "";
    }
}

void signal_handler(int signum) {
    file_name = "../hash/hashes.json";
    std::ofstream out(file_name);
    out << std::setw(4) << array << std::endl;
    out.close();
    exit(signum);
}

int main(int argc, char* argv[]) {
    init();
    unsigned int n = std::thread::hardware_concurrency();
    srand(time(nullptr));
    if (argc == 2) {
        n = boost::lexical_cast<size_t>(argv[1]);
    }
    std::signal(SIGINT, signal_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::vector<std::thread> ths;
    for (std::size_t i = 0; i < n; ++i) {
        ths.emplace_back(hash);
    }
    for (auto & th : ths) {
        th.join();
    }
}