#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <utility>

#include "cvhub/core/logging.hpp"

namespace cvhub {

namespace {

class SpdlogFileLogger final : public ILogger {
  public:
    explicit SpdlogFileLogger(const std::string& logFile) {
        logger_ = spdlog::get("cvhub");
        if (!logger_) {
            logger_ = spdlog::basic_logger_mt("cvhub", logFile, true);
            logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [%v]");
            logger_->flush_on(spdlog::level::info);
        }
    }

    void info(std::string_view scope, std::string_view message) override {
        logger_->info("[{}] {}", scope, message);
    }

    void warn(std::string_view scope, std::string_view message) override {
        logger_->warn("[{}] {}", scope, message);
    }

    void error(std::string_view scope, std::string_view message) override {
        logger_->error("[{}] {}", scope, message);
    }

  private:
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace

ScopedLogger::ScopedLogger(std::shared_ptr<ILogger> logger, std::string scope)
    : logger_(std::move(logger)), scope_(std::move(scope)) {}

void ScopedLogger::info(std::string_view message) const {
    logger_->info(scope_, message);
}

void ScopedLogger::warn(std::string_view message) const {
    logger_->warn(scope_, message);
}

void ScopedLogger::error(std::string_view message) const {
    logger_->error(scope_, message);
}

std::shared_ptr<ILogger> createSpdlogFileLogger(const std::string& logFile) {
    return std::make_shared<SpdlogFileLogger>(logFile);
}

} // namespace cvhub
