
#ifndef DOMPASCH_TREE_REXX_SAMPLE_ARG_ITERATOR_H
#define DOMPASCH_TREE_REXX_SAMPLE_ARG_ITERATOR_H

#include <vector>

#include "util/hashmap.h"
#include "data/signature.h"
#include "util/log.h"
#include "util/random.h"

class HtnInstance;

class SampleArgIterator {

private:

    std::vector<std::vector<int>> _eligible_args;
    size_t _num_samples;

    struct It {
        int _sig_id;
        const std::vector<std::vector<int>>& _eligible_args;
        size_t _num_samples;
        USignature _usig;

        It(int sigId, const std::vector<std::vector<int>>& eligibleArgs, size_t numSamples) 
                : _sig_id(sigId), _eligible_args(eligibleArgs), _num_samples(numSamples), 
                    _usig(_sig_id, std::vector<int>(eligibleArgs.size())) {
            
            setRandom();
        }

        const USignature& operator*() {
            return _usig;
        }

        const USignature& operator++() {
            setRandom();
            _num_samples--;
            return _usig;
        }

        bool operator==(const It& other) const {
            return _num_samples == other._num_samples;
        }
        bool operator!=(const It& other) const {
            return !(*this == other);
        }

        private:
        void setRandom() {
            size_t totalSize = 1;
            for (const auto& args : _eligible_args) totalSize *= args.size();
            //Log::d("total size %i\n", totalSize);
            size_t randomIdx = (int) (totalSize * Random::rand());
            //Log::d("random idx %i\n", randomIdx);
            
            size_t factor = totalSize;
            for (size_t i = 0; i < _eligible_args.size(); i++) {
                size_t size = _eligible_args[i].size();
                factor /= size;
                size_t idx = randomIdx / factor;
                //Log::d("i=%i size=%i random_idx=%i factor=%i idx=%i\n", i, size, randomIdx, factor, idx);
                assert(idx < size);
                _usig._args[i] = _eligible_args[i][idx];
                randomIdx -= idx * factor;
            }
            assert(factor == 1);
        }

    } _begin, _end;

public:

    SampleArgIterator(int sigId, std::vector<std::vector<int>>&& eligibleArgs, size_t numSamples) : 
            _eligible_args(std::move(eligibleArgs)), _num_samples(numSamples),
            _begin(sigId, _eligible_args, _num_samples),
            _end(sigId, _eligible_args, _num_samples) {
        
        size_t numChoices = _eligible_args.empty() ? 0 : 1;
        for (const auto& args : _eligible_args) numChoices *= args.size();
        _end._num_samples = 0;
    }

    It begin() const {
        return _begin;
    }

    It end() const {
        return _end;
    }
};

#endif