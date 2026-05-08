#include "TutorialWorkflow.h"

#include <utility>

namespace TopOpt {

namespace {

void primeFirstPendingStep(std::vector<WorkflowStep>& steps) {
    bool pendingAssigned = false;
    for (WorkflowStep& step : steps) {
        if (step.status == WorkflowStepStatus::Completed ||
            step.status == WorkflowStepStatus::ConfigurationError) {
            continue;
        }

        if (!pendingAssigned) {
            step.status = WorkflowStepStatus::Pending;
            pendingAssigned = true;
        } else {
            step.status = WorkflowStepStatus::NotStarted;
        }
    }
}

} // namespace

const char* workflowStepStatusName(WorkflowStepStatus status) {
    switch (status) {
    case WorkflowStepStatus::NotStarted: return "Not Started";
    case WorkflowStepStatus::Pending: return "Pending";
    case WorkflowStepStatus::Completed: return "Completed";
    case WorkflowStepStatus::ConfigurationError: return "Configuration Error";
    }

    return "Unknown";
}

const char* workflowStepName(WorkflowStepId id) {
    switch (id) {
    case WorkflowStepId::GeometryDomain: return "Geometry / Domain";
    case WorkflowStepId::Material: return "Material";
    case WorkflowStepId::BoundaryConditions: return "Boundary Conditions";
    case WorkflowStepId::Solver: return "Solver";
    case WorkflowStepId::OptimizationParameters: return "Optimization Parameters";
    case WorkflowStepId::Results: return "Results";
    }

    return "Unknown";
}

WorkflowStep TutorialWorkflow::makeStep(
    WorkflowStepId id,
    const std::string& title,
    const std::string& description,
    std::vector<std::string> requiredNodeTypes) {
    WorkflowStep step;
    step.id = id;
    step.title = title;
    step.description = description;
    step.requiredNodeTypes = std::move(requiredNodeTypes);
    return step;
}

TutorialWorkflow TutorialWorkflow::makeStandardTopologyWorkflow() {
    TutorialWorkflow workflow;
    workflow.steps_ = {
        makeStep(
            WorkflowStepId::GeometryDomain,
            "Geometry / Domain",
            "Prepare the analysis domain or FE mesh that the lesson will use.",
            {"domain-box", "domain-lshape", "domain-from-mesh", "domain-import"}),
        makeStep(
            WorkflowStepId::Material,
            "Material",
            "Define the constitutive material used by the solver or optimizer.",
            {"fea-material"}),
        makeStep(
            WorkflowStepId::BoundaryConditions,
            "Boundary Conditions",
            "Provide at least one support and one load path for the lesson setup.",
            {"fea-fixed-support", "fea-displacement-bc", "fea-point-force",
             "fea-pressure-load", "fea-body-force", "fea-load-case"}),
        makeStep(
            WorkflowStepId::Solver,
            "Solver",
            "Choose the FE or topology solver backend that executes the case.",
            {"fea-solver", "topo-simp", "topo-beso"}),
        makeStep(
            WorkflowStepId::OptimizationParameters,
            "Optimization Parameters",
            "Configure the main lesson parameters such as volume fraction and penalty.",
            {"topo-simp", "topo-beso", "topo-constraint", "topo-passive-region"}),
        makeStep(
            WorkflowStepId::Results,
            "Results",
            "Review the computed density field, deformation, or exported outputs.",
            {"output-display", "output-viewer", "output-export", "post-export"})
    };
    workflow.reset();
    return workflow;
}

void TutorialWorkflow::reset() {
    for (WorkflowStep& step : steps_) {
        step.status = WorkflowStepStatus::NotStarted;
        step.issues.clear();
    }

    primeFirstPendingStep(steps_);
}

std::size_t TutorialWorkflow::stepCount() const {
    return steps_.size();
}

std::size_t TutorialWorkflow::requiredStepCount() const {
    std::size_t count = 0;
    for (const WorkflowStep& step : steps_) {
        if (step.required) {
            ++count;
        }
    }
    return count;
}

std::size_t TutorialWorkflow::completedRequiredStepCount() const {
    std::size_t count = 0;
    for (const WorkflowStep& step : steps_) {
        if (step.required && step.status == WorkflowStepStatus::Completed) {
            ++count;
        }
    }
    return count;
}

bool TutorialWorkflow::allRequiredStepsComplete() const {
    return requiredStepCount() > 0 &&
           completedRequiredStepCount() == requiredStepCount();
}

bool TutorialWorkflow::hasBlockingIssues() const {
    for (const WorkflowStep& step : steps_) {
        if (step.status == WorkflowStepStatus::ConfigurationError) {
            return true;
        }
    }
    return false;
}

WorkflowStep* TutorialWorkflow::findStep(WorkflowStepId id) {
    for (WorkflowStep& step : steps_) {
        if (step.id == id) {
            return &step;
        }
    }
    return nullptr;
}

const WorkflowStep* TutorialWorkflow::findStep(WorkflowStepId id) const {
    for (const WorkflowStep& step : steps_) {
        if (step.id == id) {
            return &step;
        }
    }
    return nullptr;
}

WorkflowStep* TutorialWorkflow::stepAt(std::size_t index) {
    if (index >= steps_.size()) {
        return nullptr;
    }
    return &steps_[index];
}

const WorkflowStep* TutorialWorkflow::stepAt(std::size_t index) const {
    if (index >= steps_.size()) {
        return nullptr;
    }
    return &steps_[index];
}

void TutorialWorkflow::setStepStatus(WorkflowStepId id, WorkflowStepStatus status) {
    WorkflowStep* step = findStep(id);
    if (!step) {
        return;
    }

    step->status = status;
    if (status != WorkflowStepStatus::ConfigurationError) {
        step->issues.clear();
    }
}

void TutorialWorkflow::clearStepIssues(WorkflowStepId id) {
    WorkflowStep* step = findStep(id);
    if (!step) {
        return;
    }

    step->issues.clear();
    if (step->status == WorkflowStepStatus::ConfigurationError) {
        step->status = WorkflowStepStatus::Pending;
    }
}

void TutorialWorkflow::setStepIssues(WorkflowStepId id, std::vector<WorkflowIssue> issues) {
    WorkflowStep* step = findStep(id);
    if (!step) {
        return;
    }

    step->issues = std::move(issues);
    step->status = step->issues.empty()
        ? WorkflowStepStatus::Pending
        : WorkflowStepStatus::ConfigurationError;
}

void TutorialWorkflow::refreshProgression() {
    primeFirstPendingStep(steps_);
}

std::vector<WorkflowStep>& TutorialWorkflow::steps() {
    return steps_;
}

const std::vector<WorkflowStep>& TutorialWorkflow::steps() const {
    return steps_;
}

} // namespace TopOpt
