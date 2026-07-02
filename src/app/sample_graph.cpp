#include "cvhub/app/sample_graph.hpp"

namespace cvhub::app {

PipelineGraph createOpenCVSampleGraph()
{
    PipelineGraph graph;
    graph.nodes = {
        GraphNode{
            .instanceId = "source",
            .nodeId = "opencv.test_image",
            .parameters = {{"width", 640}, {"height", 360}},
        },
        GraphNode{
            .instanceId = "resize",
            .nodeId = "opencv.resize",
            .parameters = {{"width", 320}, {"height", 180}, {"interpolation", std::string("linear")}},
        },
        GraphNode{
            .instanceId = "gray",
            .nodeId = "opencv.grayscale",
        },
        GraphNode{
            .instanceId = "blur",
            .nodeId = "opencv.gaussian_blur",
            .parameters = {{"kernel", 9}},
        },
    };

    graph.edges = {
        {.fromNode = "source", .fromPort = "image", .toNode = "resize", .toPort = "image"},
        {.fromNode = "resize", .fromPort = "image", .toNode = "gray", .toPort = "image"},
        {.fromNode = "gray", .fromPort = "image", .toNode = "blur", .toPort = "image"},
    };

    return graph;
}

} // namespace cvhub::app
