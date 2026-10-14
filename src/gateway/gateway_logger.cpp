#include "autoshieldx/gateway/gateway_logger.hpp"
#include "autoshieldx/networking/aws_logger.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace autoshieldx {

static void ensureDirectoryExists(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
#ifdef _WIN32
        _mkdir(dir.c_str());
#else
        mkdir(dir.c_str(), 0755);
#endif
    }
}

std::string SecurityLogEntry::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"timestamp_iso\":\"" << timestamp_iso << "\","
        << "\"timestamp_epoch_ms\":" << timestamp_epoch_ms << ","
        << "\"can_id\":" << can_id << ","
        << "\"can_id_hex\":\"0x" << std::hex << can_id << std::dec << "\","
        << "\"event_type\":\"" << event_type << "\","
        << "\"message_status\":\"" << message_status << "\","
        << "\"attack_type\":\"" << attackTypeToString(attack_type) << "\","
        << "\"sequence_number\":" << sequence_number << ","
        << "\"hmac_result\":\"" << hmac_result << "\","
        << "\"ml_result\":\"" << ml_result << "\","
        << "\"ml_anomaly_score\":" << std::fixed << std::setprecision(3) << ml_anomaly_score << ","
        << "\"severity\":\"" << severityToString(severity) << "\","
        << "\"details\":\"" << details << "\""
        << "}";
    return oss.str();
}

GatewayLogger::GatewayLogger(const std::string& log_file_path, bool log_to_console)
    : log_file_path_(log_file_path)
    , log_to_console_(log_to_console)
{
}

GatewayLogger::~GatewayLogger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

bool GatewayLogger::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    ensureDirectoryExists(log_file_path_);

    log_file_.open(log_file_path_, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "[GatewayLogger] Error: Could not open log file: " << log_file_path_ << "\n";
        return false;
    }
    return true;
}

void GatewayLogger::setAwsLogger(std::shared_ptr<IAwsCloudLogger> aws_logger) {
    std::lock_guard<std::mutex> lock(mutex_);
    aws_logger_ = std::move(aws_logger);
}

void GatewayLogger::log(const SecurityLogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_logged_++;
    if (entry.message_status != "ACCEPTED_VALID") {
        total_rejected_++;
    }

    std::string json_line = entry.toJson();

    // 1. Write to local structured security log file
    if (log_file_.is_open()) {
        log_file_ << json_line << "\n";
        log_file_.flush();
    }

    // 2. Output to console if enabled
    if (log_to_console_) {
        std::string badge = (entry.message_status == "ACCEPTED_VALID") ? "[VALID]" : "[ALERT]";
        std::cout << badge << " [" << severityToString(entry.severity) << "] "
                  << "CAN: 0x" << std::hex << entry.can_id << std::dec
                  << " | Seq: " << entry.sequence_number
                  << " | Event: " << entry.event_type
                  << " | Status: " << entry.message_status
                  << " | Attack: " << attackTypeToString(entry.attack_type)
                  << " | Details: " << entry.details << "\n";
    }

    // 3. Forward to AWS Logger abstraction
    if (aws_logger_) {
        aws_logger_->logEvent(entry);
    }
}

} // namespace autoshieldx
