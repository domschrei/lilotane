
#include "reduction.h"

#include "algo/topological_ordering.h"

Reduction::Reduction() : HtnOp() {}
Reduction::Reduction(HtnOp& op) : HtnOp(op) {}
Reduction::Reduction(const Reduction& r) : HtnOp(r._id, r._args), _task_name_id(r._task_name_id), _task_args(r._task_args), _subtasks(r._subtasks) {
    for (auto pre : r.getPreconditions()) addPrecondition(pre);
    for (auto pre : r.getExtraPreconditions()) addExtraPrecondition(pre);
    for (auto eff : r.getEffects()) addEffect(eff);
}
Reduction::Reduction(int nameId, const std::vector<int>& args, const USignature& task) : 
        HtnOp(nameId, args), _task_name_id(task._name_id), _task_args(task._args) {}
Reduction::Reduction(int nameId, const std::vector<int>& args, USignature&& task) : 
        HtnOp(nameId, args), _task_name_id(task._name_id), _task_args(std::move(task._args)) {}

void Reduction::orderSubtasks(const std::map<int, std::vector<int>>& orderingNodelist) {

    std::vector<int> sortedNodes = TopologicalOrdering::compute(orderingNodelist);

    // Reorder subtasks
    std::vector<USignature> newSubtasks;
    newSubtasks.resize(_subtasks.size());
    for (size_t i = 0; i < _subtasks.size(); i++) {
        newSubtasks[i] = _subtasks[sortedNodes[i]];
    }
    _subtasks = newSubtasks;
}

Reduction Reduction::substituteRed(const Substitution& s) const {
    HtnOp op = HtnOp::substitute(s);
    Reduction r(op);
    
    r._task_name_id = _task_name_id;
    
    r._task_args.resize(_task_args.size());
    for (size_t i = 0; i < _task_args.size(); i++) {
        auto it = s.find(_task_args[i]);
        if (it != s.end()) r._task_args[i] = it->second;
        else r._task_args[i] = _task_args[i];
    }
    
    r._subtasks.resize(_subtasks.size());
    for (size_t i = 0; i < _subtasks.size(); i++) {
        r._subtasks[i] = _subtasks[i].substitute(s);
    }

    return r;
}

void Reduction::addSubtask(const USignature& subtask) {
    _subtasks.push_back(subtask);
}
void Reduction::addSubtask(USignature&& subtask) {
    _subtasks.push_back(std::move(subtask));
}
void Reduction::setSubtasks(std::vector<USignature>&& subtasks) {
    _subtasks = std::move(subtasks);
}

USignature Reduction::getTaskSignature() const {
    return USignature(_task_name_id, _task_args);
}
const std::vector<int>& Reduction::getTaskArguments() const {
    return _task_args;
}
const std::vector<USignature>& Reduction::getSubtasks() const {
    return _subtasks;
}

Reduction& Reduction::operator=(const Reduction& other) {
    _id = other._id;
    _args = other._args;
    _preconditions = other._preconditions;
    _extra_preconditions = other._extra_preconditions;
    _effects = other._effects;
    _task_name_id = other._task_name_id;
    _task_args = other._task_args;
    _subtasks = other._subtasks;
    return *this;
}