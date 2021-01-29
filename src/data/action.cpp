
#include "action.h"

Action::Action() : HtnOp() {}
Action::Action(const HtnOp& op) : HtnOp(op) {}
Action::Action(const Action& a) : HtnOp(a._id, a._args) {
    _preconditions = a._preconditions;
    _extra_preconditions = a._extra_preconditions;
    _effects = a._effects;
}
Action::Action(int nameId, const std::vector<int>& args) : HtnOp(nameId, args) {}
Action::Action(int nameId, std::vector<int>&& args) : HtnOp(nameId, std::move(args)) {}

Action& Action::operator=(const Action& op) {
    _id = op._id;
    _args = op._args;
    _preconditions = op._preconditions;
    _extra_preconditions = op._extra_preconditions;
    _effects = op._effects;
    return *this;
}