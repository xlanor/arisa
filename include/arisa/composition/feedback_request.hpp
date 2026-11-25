#pragma once

#include <arisa/composition/feedback_origin.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/sensory_feedback.hpp>

namespace arisa::composition {

struct FeedbackRequest {
  NodeId node{0};
  SensoryFeedback feedback;
  FeedbackOrigin origin{FeedbackOrigin::application};
};

}
