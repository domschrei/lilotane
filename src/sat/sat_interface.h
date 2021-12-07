
#ifndef DOMPASCH_LILOTANE_SAT_INTERFACE_H
#define DOMPASCH_LILOTANE_SAT_INTERFACE_H

#include <initializer_list>
#include <fstream>
#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <mutex>

#include "util/params.h"
#include "util/log.h"
#include "sat/variable_domain.h"
#include "sat/encoding_statistics.h"

extern "C" {
    #include "sat/ipasir.h"
}

int callbackShouldTerminate(void* data);
void callbackFinished(int result, void* solver, void* data);

class SatInterface {

public:
    struct BranchData {
        int branchIdx;
        SatInterface* interface;
        void* solver;
        int result = -1;
    };
    
private:
    Parameters& _params;
    void* _solver;
    std::ofstream _out;
    EncodingStatistics& _stats;

    const bool _print_formula;    
    bool _began_line = false;

    std::vector<int> _last_assumptions;
    std::vector<int> _no_decision_variables;

    std::mutex _branch_mutex;
    std::vector<BranchData*> _branches;
    int _max_branch_interrupt_index = -1;
    int _num_active_branches = 0;
    int _solved_layer = -1;

public:
    SatInterface(Parameters& params, EncodingStatistics& stats) : 
                _params(params), _stats(stats), _print_formula(params.isNonzero("wf")) {
        
        bool incremental = _params.getIntParam("branch") == 0;
        _solver = mallob_ipasir_init(/*incremental=*/incremental);
        ipasir_set_seed(_solver, params.getIntParam("s"));
        if (_print_formula) _out.open("formula.cnf");

        if (incremental && _params.isNonzero("presubmit"))
            mallob_ipasir_presubmit(_solver);
    }
    
    inline void addClause(int lit) {
        assert(lit != 0);
        ipasir_add(_solver, lit); ipasir_add(_solver, 0);
        if (_print_formula) _out << lit << " 0\n";
        _stats._num_lits++; _stats._num_cls++;
    }
    inline void addClause(int lit1, int lit2) {
        assert(lit1 != 0);
        assert(lit2 != 0);
        ipasir_add(_solver, lit1); ipasir_add(_solver, lit2); ipasir_add(_solver, 0);
        if (_print_formula) _out << lit1 << " " << lit2 << " 0\n";
        _stats._num_lits += 2; _stats._num_cls++;
    }
    inline void addClause(int lit1, int lit2, int lit3) {
        assert(lit1 != 0);
        assert(lit2 != 0);
        assert(lit3 != 0);
        ipasir_add(_solver, lit1); ipasir_add(_solver, lit2); ipasir_add(_solver, lit3); ipasir_add(_solver, 0);
        if (_print_formula) _out << lit1 << " " << lit2 << " " << lit3 << " 0\n";
        _stats._num_lits += 3; _stats._num_cls++;
    }
    inline void addClause(const std::initializer_list<int>& lits) {
        for (int lit : lits) {
            assert(lit != 0);
            ipasir_add(_solver, lit);
            if (_print_formula) _out << lit << " ";
        } 
        ipasir_add(_solver, 0);
        if (_print_formula) _out << "0\n";
        _stats._num_cls++;
        _stats._num_lits += lits.size();
    }
    inline void addClause(const std::vector<int>& cls) {
        for (int lit : cls) {
            assert(lit != 0);
            ipasir_add(_solver, lit);
            if (_print_formula) _out << lit << " ";
        } 
        ipasir_add(_solver, 0);
        if (_print_formula) _out << "0\n";
        _stats._num_cls++;
        _stats._num_lits += cls.size();
    }
    inline void appendClause(int lit) {
        _began_line = true;
        assert(lit != 0);
        ipasir_add(_solver, lit);
        if (_print_formula) _out << lit << " ";
        _stats._num_lits++;
    }
    inline void appendClause(int lit1, int lit2) {
        _began_line = true;
        assert(lit1 != 0);
        assert(lit2 != 0);
        ipasir_add(_solver, lit1); ipasir_add(_solver, lit2);
        if (_print_formula) _out << lit1 << " " << lit2 << " ";
        _stats._num_lits += 2;
    }
    inline void appendClause(const std::initializer_list<int>& lits) {
        _began_line = true;
        for (int lit : lits) {
            assert(lit != 0);
            ipasir_add(_solver, lit);
            if (_print_formula) _out << lit << " ";
            //log("%i ", lit);
        } 

        _stats._num_lits += lits.size();
    }
    inline void endClause() {
        assert(_began_line);
        ipasir_add(_solver, 0);
        if (_print_formula) _out << "0\n";
        //log("0\n");
        _began_line = false;

        _stats._num_cls++;
    }
    inline void assume(int lit) {
        if (_stats._num_asmpts == 0) _last_assumptions.clear();
        ipasir_assume(_solver, lit);
        //log("CNF !%i\n", lit);
        _last_assumptions.push_back(lit);
        _stats._num_asmpts++;
    }

    inline bool holds(int lit) {
        return ipasir_val(_solver, lit) > 0;
    }

    inline bool didAssumptionFail(int lit) {
        return ipasir_failed(_solver, lit);
    }

    bool hasLastAssumptions() {
        return !_last_assumptions.empty();
    }

    void setTerminateCallback(void * state, int (*terminate)(void * state)) {
        ipasir_set_terminate(_solver, state, terminate);
    }

    void setLearnCallback(int maxLength, void* state, void (*learn)(void * state, int * clause)) {
        ipasir_set_learn(_solver, state, maxLength, learn);
    }

    int solve() {
        int result = ipasir_solve(_solver);
        if (_stats._num_asmpts == 0) _last_assumptions.clear();
        _stats._num_asmpts = 0;
        _solved_layer++;
        if (result != 10 && _params.getIntParam("branch") == 0 && _params.isNonzero("presubmit")) 
            mallob_ipasir_presubmit(_solver);
        return result;
    }





    

    bool branchedSolve() {
        if (_num_active_branches >= _params.getIntParam("branch")) return false;

        _branch_mutex.lock();

        int branchIdx = _branches.size();
        Log::i("Branching off #%i\n", branchIdx);
        BranchData* data = new BranchData();
        data->branchIdx = branchIdx;
        data->interface = this;
        _branches.push_back(data);

        mallob_ipasir_branched_solve(_solver, data, callbackShouldTerminate, callbackFinished);
        
        _branch_mutex.unlock();

        _num_active_branches++;
        return true;
    }

    bool shouldBranchTerminate(const BranchData& data) {
        _branch_mutex.lock();
        bool terminate = data.branchIdx <= _max_branch_interrupt_index;
        _branch_mutex.unlock();
        return terminate;
    }

    void branchDone(int result, void* solver, BranchData& data) {
        int idx = data.branchIdx;
        _branch_mutex.lock();
        data.solver = solver;
        data.result = result;
        _branch_mutex.unlock();
    }

    bool checkBranches() {
        bool done = false;
        _branch_mutex.lock();
        for (size_t i = 0; i < _branches.size(); i++) {
            if (!_branches[i]) continue;
            if (_branches[i]->result >= 0) {
                // This branch finished
                BranchData& data = *_branches[i];
                Log::i("Branch #%i done, result %s\n", i, 
                    data.result == 0 ? "UNKNOWN" : (data.result == 10 ? "SAT" : "UNSAT"));

                if (data.result == 10) {
                    // SAT: done! 
                    // Replace internal solver with "winning" branch
                    _solver = data.solver;
                    _solved_layer = i;
                    // Interrupt ALL branches
                    _max_branch_interrupt_index = INT32_MAX;
                    done = true;
                }
                if (data.result == 20) {
                    // UNSAT: Interrupt all smaller branches
                    _max_branch_interrupt_index = i;
                }

                // Clean up branch
                delete _branches[i];
                _branches[i] = nullptr;
                _num_active_branches--;

                if (done) break;
            }
        }
        _branch_mutex.unlock();
        return done;
    }

    int getSolvedLayerIdx() const {
        return _solved_layer;
    }






    ~SatInterface() {
        
        if (_params.isNonzero("wf")) {

            for (int asmpt : _last_assumptions) {
                _out << asmpt << " 0\n";
            }
            _out.flush();
            _out.close();

            // Create final formula file
            std::ofstream ffile;
            ffile.open("f.cnf");
            
            // Append header to formula file
            ffile << "p cnf " << VariableDomain::getMaxVar() << " " << (_stats._num_cls+_last_assumptions.size()) << "\n";

            // Append main content to formula file (reading from "old" file)
            std::ifstream oldfile;
            oldfile.open("formula.cnf");
            std::string line;
            while (std::getline(oldfile, line)) {
                line = line + "\n";
                ffile.write(line.c_str(), line.size());
            }
            oldfile.close();
            remove("formula.cnf");

            // Finish
            ffile.flush();
            ffile.close();
        }

        // Release SAT solver
        ipasir_release(_solver);
    }
};

#endif
