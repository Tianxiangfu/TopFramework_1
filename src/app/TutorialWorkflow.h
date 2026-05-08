#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace TopOpt {

enum class WorkflowStepStatus {
    NotStarted,
    Pending,
    Completed,
    ConfigurationError
};

enum class WorkflowStepId {
    GeometryDomain,
    Material,
    BoundaryConditions,
    Solver,
    OptimizationParameters,
    Results
};

struct WorkflowIssue {
    std::string code;
    std::string message;
};

struct WorkflowStep {
    WorkflowStepId id = WorkflowStepId::GeometryDomain;
    std::string title;
    std::string description;
    WorkflowStepStatus status = WorkflowStepStatus::NotStarted;
    bool required = true;
    std::vector<std::string> requiredNodeTypes;
    std::vector<WorkflowIssue> issues;
};

const char* workflowStepStatusName(WorkflowStepStatus status);
const char* workflowStepName(WorkflowStepId id);

class TutorialWorkflow {
public:
    static TutorialWorkflow makeStandardTopologyWorkflow();

    void reset();

    std::size_t stepCount() const;
    std::size_t requiredStepCount() const;
    std::size_t completedRequiredStepCount() const;
    bool allRequiredStepsComplete() const;
    bool hasBlockingIssues() const;

    WorkflowStep* findStep(WorkflowStepId id);
    const WorkflowStep* findStep(WorkflowStepId id) const;
    WorkflowStep* stepAt(std::size_t index);
    const WorkflowStep* stepAt(std::size_t index) const;

    void setStepStatus(WorkflowStepId id, WorkflowStepStatus status);
    void clearStepIssues(WorkflowStepId id);
    void setStepIssues(WorkflowStepId id, std::vector<WorkflowIssue> issues);
    void refreshProgression();

    std::vector<WorkflowStep>& steps();
    const std::vector<WorkflowStep>& steps() const;

private:
    static WorkflowStep makeStep(
        WorkflowStepId id,
        const std::string& title,
        const std::string& description,
        std::vector<std::string> requiredNodeTypes);

    std::vector<WorkflowStep> steps_;
};

} // namespace TopOpt
