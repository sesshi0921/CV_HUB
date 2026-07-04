#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace cvhub {

class ILogger {
  public:
    virtual ~ILogger() = default;
    virtual void info(std::string_view scope, std::string_view message) = 0;
    virtual void warn(std::string_view scope, std::string_view message) = 0;
    virtual void error(std::string_view scope, std::string_view message) = 0;
};

class ScopedLogger {
  public:
    ScopedLogger(std::shared_ptr<ILogger> logger, std::string scope);

    void info(std::string_view message) const;
    void warn(std::string_view message) const;
    void error(std::string_view message) const;

  private:
    std::shared_ptr<ILogger> logger_;
    std::string scope_;
};

std::shared_ptr<ILogger> createSpdlogFileLogger(const std::string& logFile);

} // namespace cvhub
