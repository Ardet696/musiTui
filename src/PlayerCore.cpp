#include "PlayerCore.h"

#include <string>

PlayerCore::PlayerCore()
    : controller_(bus_, cmdQueue_)
    , service_(library_, controller_, bus_)
    , asyncService_(service_, cmdQueue_) {
    std::string savedRoot = config_.getRootPath();
    if (!savedRoot.empty()) {
        std::string error;
        service_.setRootPath(savedRoot, error);
    }
}
