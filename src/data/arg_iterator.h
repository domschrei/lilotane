
#ifndef DOMPASCH_TREE_REXX_ARG_ITERATOR_H
#define DOMPASCH_TREE_REXX_ARG_ITERATOR_H

#include <vector>

#include <data/hashmap.h>
#include "data/signature.h"
#include "util/log.h"

class HtnInstance;

class ArgIterator {

private:
    struct It {
        int _sig_id;
        const std::vector<std::vector<int>>& _eligible_args;
        std::vector<size_t> _counter;
        USignature _usig;

        It(int sigId, const std::vector<std::vector<int>>& eligibleArgs) 
                : _sig_id(sigId), _eligible_args(eligibleArgs), _counter(eligibleArgs.size(), 0), 
                    _usig(_sig_id, std::vector<int>(_counter.size())) {
            
            for (size_t i = 0; i < _usig._args.size(); i++) {
                _usig._args[i] = _eligible_args[i].front();
            }
        }

        const USignature& operator*() {
            return _usig;
        }

        const USignature& operator++() {
            for (size_t i = 0; i < _counter.size(); i++) {
                if (_counter[i]+1 == _eligible_args[i].size()) {
                    // max value reached
                    _counter[i] = 0;
                    _usig._args[i] = _eligible_args[i].front();
                } else {
                    // increment
                    _counter[i]++;
                    _usig._args[i] = _eligible_args[i].at(_counter[i]);
                    break;
                }
            }
            return _usig;
        }

        bool operator==(const It& other) const {
            return _counter == other._counter;
        }
        bool operator!=(const It& other) const {
            return !(*this == other);
        }

    } _begin, _end;

public:

    ArgIterator(int sigId, const std::vector<std::vector<int>>& eligibleArgs) : 
            _begin(sigId, eligibleArgs),
            _end(sigId, eligibleArgs) {
        
        for (size_t i = 0; i < _begin._usig._args.size(); i++) {
            _begin._usig._args[i] = _begin._eligible_args[i].front();
            _end._usig._args[i] = _end._eligible_args[i].back();
            _end._counter[i] = _end._eligible_args[i].size()-1;
        }
    }

    It begin() const {
        return _begin;
    }

    It end() const {
        return _end;
    }

    static std::vector<Signature> getFullInstantiation(const Signature& sig, HtnInstance& _htn);
    static std::vector<USignature> getFullInstantiation(const USignature& sig, HtnInstance& _htn);
    static std::vector<USignature> instantiate(const USignature& sig, const std::vector<std::vector<int>>& eligibleArgs);
};

#endif