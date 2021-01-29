
#include "util/timer.h"
#include "util/log.h"
#include "util/params.h"

#include "data/arg_iterator.h"

void printSig(const USignature& sig) {
    Log::i("(%i", sig._name_id);
    for (int arg : sig._args) Log::log_notime(Log::V2_INFORMATION, " %i", arg);
    Log::log_notime(Log::V2_INFORMATION, ")\n"); 
}


int main(int argc, char** argv) {

    Timer::init();

    Parameters params;
    params.init(argc, argv);

    int verbosity = params.getIntParam("v");
    Log::init(verbosity, /*coloredOutput=*/params.isNonzero("co"));

    int nameId = 42;

    {
        std::vector<int> args1{1, 2, 3, 4};
        std::vector<int> args2{5};
        std::vector<int> args3{6, 7};
        std::vector<std::vector<int>> eligibleArgs{args1, args2, args3};

        size_t numInstantiations = 0;
        for (const auto& sig : ArgIterator(nameId, std::move(eligibleArgs))) {
            assert(sig._name_id == nameId);
            printSig(sig);
            numInstantiations++;
        }
        assert(numInstantiations == args1.size() * args2.size() * args3.size());
    }

    {
        std::vector<int> args{1, 2};
        std::vector<std::vector<int>> eligibleArgs{args};

        size_t numInstantiations = 0;
        for (const auto& sig : ArgIterator(nameId, std::move(eligibleArgs))) {
            assert(sig._name_id == nameId);
            printSig(sig);
            numInstantiations++;
        }
        assert(numInstantiations == args.size());
    }

    {
        std::vector<int> args1{1};
        std::vector<int> args2{2};
        std::vector<int> args3{3};
        std::vector<std::vector<int>> eligibleArgs{args1, args2, args3};

        size_t numInstantiations = 0;
        for (const auto& sig : ArgIterator(nameId, std::move(eligibleArgs))) {
            assert(sig._name_id == nameId);
            printSig(sig);
            numInstantiations++;
        }
        assert(numInstantiations == args1.size() * args2.size() * args3.size());
    }

    {
        std::vector<std::vector<int>> eligibleArgs{};

        size_t numInstantiations = 0;
        for (const auto& sig : ArgIterator(nameId, std::move(eligibleArgs))) {
            assert(sig._name_id == nameId);
            printSig(sig);
            numInstantiations++;
        }
        assert(numInstantiations == 0);
    }
    
    {
        std::vector<int> args1{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::vector<int> args2{1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<int> args3{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::vector<int> args4{1, 2, 3, 4, 5, 6, 7, 8, 9};
        std::vector<std::vector<int>> eligibleArgs{args1, args2, args3, args4};

        size_t numInstantiations = 0;
        for (const auto& sig : ArgIterator(nameId, std::move(eligibleArgs))) {
            assert(sig._name_id == nameId);
            printSig(sig);
            numInstantiations++;
        }
        assert(numInstantiations == args1.size() * args2.size() * args3.size() * args4.size());
    }
}
