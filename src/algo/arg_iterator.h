
#ifndef DOMPASCH_TREE_REXX_ARG_ITERATOR_H
#define DOMPASCH_TREE_REXX_ARG_ITERATOR_H

#include <vector>

#include "util/hashmap.h"
#include "data/signature.h"
#include "util/log.h"

class HtnInstance;

class ArgIterator {

private:

    std::vector<std::vector<int>> _eligible_args;

    struct It {
        int _sig_id;
        const std::vector<std::vector<int>>& _eligible_args;
        std::vector<size_t> _counter;
        size_t _counter_number;
        USignature _usig;

        It(int sigId, const std::vector<std::vector<int>>& eligibleArgs) 
                : _sig_id(sigId), _eligible_args(eligibleArgs), _counter(eligibleArgs.size(), 0), 
                    _counter_number(0), _usig(_sig_id, std::vector<int>(_counter.size())) {
            
            for (size_t i = 0; i < _usig._args.size(); i++) {
                assert(i < _eligible_args.size());
                assert(!_eligible_args[i].empty());
                _usig._args[i] = _eligible_args[i].front();
            }
        }

        const USignature& operator*() {
            return _usig;
        }

        const USignature& operator++() {
            for (size_t i = 0; i < _counter.size(); i++) {
                if (_counter[i]+1 == _eligible_args[i].size()) {
                    // reached max value of some position
                    _counter[i] = 0;
                    _usig._args[i] = _eligible_args[i].front();
                } else {
                    // increment and done
                    _counter[i]++;
                    _usig._args[i] = _eligible_args[i].at(_counter[i]);
                    break;
                }
            }
            _counter_number++;
            return _usig;
        }

        bool operator==(const It& other) const {
            return _counter_number == other._counter_number;
        }
        bool operator!=(const It& other) const {
            return !(*this == other);
        }

    } _begin, _end;

public:

    ArgIterator(int sigId, std::vector<std::vector<int>>&& eligibleArgs) : 
            _eligible_args(std::move(eligibleArgs)),
            _begin(sigId, _eligible_args),
            _end(sigId, _eligible_args) {
        
        size_t numChoices = _eligible_args.empty() ? 0 : 1;
        for (const auto& args : _eligible_args) numChoices *= args.size();
        _end._counter_number = numChoices;
    }

    It begin() const {
        return _begin;
    }

    It end() const {
        return _end;
    }

    static ArgIterator getFullInstantiation(const USignature& sig, HtnInstance& _htn);
};

#endif