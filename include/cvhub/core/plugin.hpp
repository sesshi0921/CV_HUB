#pragma once

namespace cvhub {

class ServiceContainer;

class IPluginModule {
  public:
    virtual ~IPluginModule() = default;
    virtual void registerServices(ServiceContainer& services) = 0;
};

} // namespace cvhub
