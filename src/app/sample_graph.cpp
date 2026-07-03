#include "cvhub/app/sample_graph.hpp"

namespace cvhub::app {

PipelineGraph createOpenCVSampleGraph() {
  PipelineGraph graph;
  graph.nodes = {
      GraphNode{
          .instanceId = "source",
          .nodeId = "common.test_image",
          .parameters = {{"width", 640}, {"height", 360}},
      },
      GraphNode{
          .instanceId = "resize",
          .nodeId = "opencv.resize",
          .parameters = {{"dsize", 0},
                         {"fx", 0.5},
                         {"fy", 0.5},
                         {"interpolation", 1}},  // INTER_LINEAR
      },
      GraphNode{
          .instanceId = "blur",
          .nodeId = "opencv.gaussian_blur",
          .parameters = {{"ksize", 9}},
      },
  };

  graph.edges = {
      {.fromNode = "source",
       .fromPort = "image",
       .toNode = "resize",
       .toPort = "image"},
      {.fromNode = "resize",
       .fromPort = "image",
       .toNode = "blur",
       .toPort = "image"},
  };

  return graph;
}

}  // namespace cvhub::app
