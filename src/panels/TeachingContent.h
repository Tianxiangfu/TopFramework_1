#pragma once

#include <string>
#include <vector>

namespace TopOpt {

struct TeachingParamInfo {
    std::string paramName;
    std::string displayName;
    std::string meaning;
    std::string recommended;
    std::string changeUpLabel = u8"\u8c03\u5927";
    std::string changeDownLabel = u8"\u8c03\u5c0f";
    std::string increaseEffect;
    std::string decreaseEffect;
    std::string caution;
};

struct TeachingNodeInfo {
    std::string typeName;
    std::string title;
    std::string summary;
    std::string lessonRole;
    std::string experimentHint;
    std::vector<TeachingParamInfo> params;
};

const TeachingNodeInfo* findTeachingNodeInfo(const std::string& typeName);
const TeachingParamInfo* findTeachingParamInfo(const TeachingNodeInfo& nodeInfo, const std::string& paramName);

} // namespace TopOpt
