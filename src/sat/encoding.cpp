
#include "sat/encoding.h"

void Encoding::addInitialClauses(Layer& layer) {
    addInitialState(layer);
    addInitialTaskNetwork(layer);
}

void Encoding::addUniversalClauses(Layer& layer) {
    addActionConstraints(layer);
    addFrameAxioms(layer);
    addQConstantsDefinition(layer);
    addPrimitivenessDefinition(layer);
}

void Encoding::addTransitionalClauses(Layer& oldLayer, Layer& newLayer) {
    addExpansions(oldLayer, newLayer);
    addActionPropagations(oldLayer, newLayer);
    addFactEquivalencies(oldLayer, newLayer);
}

void Encoding::addAssumptions(Layer& layer) {
    assertAllPrimitive(layer);
}