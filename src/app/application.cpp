#include "cvhub/app/application.hpp"

#include "cvhub/app/sample_graph.hpp"

#ifdef CVHUB_HAS_OPENCV_PLUGIN
#include "cvhub/plugins/opencv/opencv_plugin.hpp"
#endif

#include <iostream>

namespace cvhub::app {

std::shared_ptr<ServiceContainer> createApplication(const ApplicationOptions& options)
{
    auto services = std::make_shared<ServiceContainer>();
    services->setLogger(createSpdlogFileLogger(options.logFile));

#ifdef CVHUB_HAS_OPENCV_PLUGIN
    plugins::opencv::registerOpenCVPlugin(*services);
#endif

    services->buildRuntime(options.workerCount);
    return services;
}

namespace {

void printUsage()
{
    std::cout << "Usage: cvhub [--list-nodes] [--run-sample] [--log-file <path>] [--workers <count>]\n";
}

} // namespace

int runCli(int argc, char** argv)
{
    ApplicationOptions options;
    bool listNodes = false;
    bool runSample = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
        if (arg == "--list-nodes") {
            listNodes = true;
            continue;
        }
        if (arg == "--run-sample") {
            runSample = true;
            continue;
        }
        if (arg == "--log-file" && i + 1 < argc) {
            options.logFile = argv[++i];
            continue;
        }
        if (arg == "--workers" && i + 1 < argc) {
            options.workerCount = std::stoi(argv[++i]);
            continue;
        }
        std::cerr << "Unknown argument: " << arg << "\n";
        printUsage();
        return 2;
    }

    auto services = createApplication(options);

    if (listNodes) {
        for (const auto& node : services->nodeCatalog()->list()) {
            std::cout << node.id << " | " << node.displayName << " | " << node.category << "\n";
        }
    }

    if (runSample || (!listNodes && !runSample)) {
        const auto result = services->pipelineExecutor()->run(createOpenCVSampleGraph());
        std::cout << result.message << "\n";
        if (!result.ok) {
            return 1;
        }
        if (const auto lastNode = result.outputs.find("blur"); lastNode != result.outputs.end()) {
            if (const auto imageOutput = lastNode->second.find("image"); imageOutput != lastNode->second.end()) {
                const auto image = valueCast<ImageValue>(imageOutput->second);
                if (image) {
                    std::cout << "final image: " << image->width() << "x" << image->height() << " channels=" << image->channels() << "\n";
                }
            }
        }
    }

    return 0;
}

} // namespace cvhub::app
