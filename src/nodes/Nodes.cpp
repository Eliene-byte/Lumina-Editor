#include "Nodes.h"

#include "core/NodeRegistry.h"
#include "gpu/NodeUniforms.h"

namespace lmn::nodes {

void registerAllNodes() {
    registerSourceNodes();
    registerMediaNodes();
    registerMergeNodes();
    registerColorNodes();
    registerFilterNodes();
    registerTimeNodes();
}

int registeredNodeTypeCount() {
    return static_cast<int>(NodeRegistry::instance().typeCount());
}

}  // namespace lmn::nodes
