#pragma once

#include "autoshieldx/common.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <vector>

namespace autoshieldx {

class IAwsCloudLogger;

struct SecurityLogEntry {
    std::string timestamp_iso{};
    uint64_t timestamp_epoch_ms{0};
    uint32_t can_id{0};
    std::string event_type{};
    std::string message_status{};
    AttackType attack_type{AttackType::NONE};
    uint32_t sequence_number{0};
    std::string hmac_result{"UNCHECKED"};
    std::string ml_result{"NORMAL"};
    double ml_anomaly_score{0.0};
    Severity severity{Severity::INFO};
    std::string details{};

    std::string toJson() const;
};

/**
 * @brief Thread-Safe Structured Security Logger
 *
 * Produces structured JSON-lines records containing complete forensic metadata.
 * Suitable for local analysis, real-time dashboard forwarding, and seamless
 * ingestion into AWS CloudWatch or Amazon OpenSearch Service.
 */
class GatewayLogger {
public:
    explicit GatewayLogger(const std::string& log_file_path = "logs/autoshieldx_security.log",
                           bool log_to_console = true);
    ~GatewayLogger();

    bool init();
    void log(const SecurityLogEntry& entry);
    void setAwsLogger(std::shared_ptr<IAwsCloudLogger> aws_logger);
    void setConsoleLogging(bool enable) { log_to_console_ = enable; }

    uint64_t getTotalLogged() const { return total_logged_; }
    uint64_t getRejectedCount() const { return total_rejected_; }

private:
    std::string log_file_path_;
    bool log_to_console_;
    std::ofstream log_file_;
    std::mutex mutex_;
    std::shared_ptr<IAwsCloudLogger> aws_logger_{nullptr};

    uint64_t total_logged_{0};
    uint64_t total_rejected_{0};
};

} // namespace autoshieldx
