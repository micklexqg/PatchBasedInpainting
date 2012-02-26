#ifndef InpaintingPrecomputedAlgorithm_hpp
#define InpaintingPrecomputedAlgorithm_hpp

// Boost
#include <boost/graph/properties.hpp>

// STL
#include <stdexcept>

// Custom
#include "Helpers/BoostHelpers.h"

template <typename TNodePairQueue, typename TVisitor, typename TPatchInpainter>
inline
void InpaintingPrecomputedAlgorithm(TNodePairQueue& nodeQueue, TVisitor vis,
                        TPatchInpainter inpaint_patch)
{
  typedef typename TNodePairQueue::value_type NodePairType;
  while(!nodeQueue.empty())
  {
    NodePairType nodePair = nodeQueue.front();
    typename NodePairType::first_type targetNode = nodePair.first;
    typename NodePairType::second_type sourceNode = nodePair.second;

    inpaint_patch(targetNode, sourceNode, vis);

    nodeQueue.pop();
  } // end main iteration loop

  vis.InpaintingComplete();

};

#endif