#include "autoshieldx/networking/aws_logger.hpp"
#include "autoshieldx/gateway/gateway_logger.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace autoshieldx {

LocalMockAwsLogger::LocalMockAwsLogger(const std::string& mock_file)
    : mock_file_(mock_file)
{
}

LocalMockAwsLogger::~LocalMockAwsLogger() {
    flush();
}

bool LocalMockAwsLogger::init(const std::string& config_file_path) {
    if (config_file_path.empty()) {
        return true;
    }

    std::ifstream cfg(config_file_path);
    if (!cfg.is_open()) {
        return true;
    }

    std::string line;
    while (std::getline(cfg, line)) {
        if (line.find("\"s3_bucket\"") != std::string::npos) {
            auto first = line.find('\"', line.find(':') + 1);
            auto last = line.rfind('\"');
            if (first != std::string::npos && last != std::string::npos && last > first) {
                s3_bucket_ = line.substr(first + 1, last - first - 1);
            }
        } else if (line.find("\"cloudwatch_log_group\"") != std::string::npos) {
            auto first = line.find('\"', line.find(':') + 1);
            auto last = line.rfind('\"');
            if (first != std::string::npos && last != std::string::npos && last > first) {
                cloudwatch_log_group_ = line.substr(first + 1, last - first - 1);
            }
        }
    }
    return true;
}

void LocalMockAwsLogger::logEvent(const SecurityLogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.push_back(entry.toJson());

    if (buffer_.size() >= batch_size_) {
        // Internal flush without re-locking
        std::ofstream out(mock_file_, std::ios::app);
        if (out.is_open()) {
            out << "{\n"
                << "  \"aws_destination\": \"s3://" << s3_bucket_ << "\",\n"
                << "  \"cloudwatch_group\": \"" << cloudwatch_log_group_ << "\",\n"
                << "  \"batch_timestamp_ms\": " << getCurrentTimestampMillis() << ",\n"
                << "  \"records_count\": " << buffer_.size() << ",\n"
                << "  \"records\": [\n";
            for (size_t i = 0; i < buffer_.size(); ++i) {
                out << "    " << buffer_[i] << (i + 1 < buffer_.size() ? ",\n" : "\n");
            }
            out << "  ]\n}\n";
        }
        buffer_.clear();
    }
}

void LocalMockAwsLogger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_.empty()) return;

    std::ofstream out(mock_file_, std::ios::app);
    if (out.is_open()) {
        out << "{\n"
            << "  \"aws_destination\": \"s3://" << s3_bucket_ << "\",\n"
            << "  \"cloudwatch_group\": \"" << cloudwatch_log_group_ << "\",\n"
            << "  \"batch_timestamp_ms\": " << getCurrentTimestampMillis() << ",\n"
            << "  \"records_count\": " << buffer_.size() << ",\n"
            << "  \"records\": [\n";
        for (size_t i = 0; i < buffer_.size(); ++i) {
            out << "    " << buffer_[i] << (i + 1 < buffer_.size() ? ",\n" : "\n");
        }
        out << "  ]\n}\n";
    }
    buffer_.clear();
}

} // namespace autoshieldx
