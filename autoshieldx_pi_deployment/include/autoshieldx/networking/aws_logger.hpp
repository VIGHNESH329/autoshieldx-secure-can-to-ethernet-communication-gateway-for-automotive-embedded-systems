#pragma once

#include "autoshieldx/common.hpp"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace autoshieldx {

struct SecurityLogEntry; // Forward declaration

/**
 * @brief Abstract AWS Cloud Logging & Storage Interface
 *
 * Provides a clean interface for persisting security telemetry to AWS S3
 * and AWS CloudWatch Logs. Local software runs completely offline using
 * LocalMockAwsLogger without requiring AWS credentials, IAM keys, or internet.
 */
class IAwsCloudLogger {
public:
    virtual ~IAwsCloudLogger() = default;

    virtual bool init(const std::string& config_file_path) = 0;
    virtual void logEvent(const SecurityLogEntry& entry) = 0;
    virtual void flush() = 0;
    virtual std::string getBackendName() const = 0;
};

/**
 * @brief Offline Mock AWS Cloud Logger
 *
 * Buffers event records locally and writes structured CloudWatch-format
 * JSON batches to `logs/aws_mock_telemetry_batch.json`.
 * Zero AWS credentials required.
 */
class LocalMockAwsLogger : public IAwsCloudLogger {
public:
    explicit LocalMockAwsLogger(const std::string& mock_file = "logs/aws_mock_telemetry_batch.json");
    ~LocalMockAwsLogger() override;

    bool init(const std::string& config_file_path) override;
    void logEvent(const SecurityLogEntry& entry) override;
    void flush() override;
    std::string getBackendName() const override { return "Local_Mock_AWS_Logger (Offline)"; }

private:
    std::string mock_file_;
    std::string s3_bucket_{"autoshieldx-telemetry-logs"};
    std::string cloudwatch_log_group_{"/autoshieldx/gateway_events"};
    std::vector<std::string> buffer_;
    size_t batch_size_{20};
    std::mutex mutex_;
};

} // namespace autoshieldx
