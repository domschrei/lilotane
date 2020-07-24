
#include "action.h"

Action::Action() : HtnOp() {}
Action::Action(const HtnOp& op) : HtnOp(op) {}
Action::Action(const Action& a) : HtnOp(a._id, a._args) {
    _preconditions = (a._preconditions);
    _effects = (a._effects); 
}
Action::Action(int nameId, const std::vector<int>& args) : HtnOp(nameId, args) {}

Action& Action::operator=(const Action& op) {
    _id = op._id;
    _args = op._args;
    _preconditions = op._preconditions;
    _effects = op._effects;
    return *this;
}