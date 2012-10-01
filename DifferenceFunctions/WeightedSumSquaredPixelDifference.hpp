/*=========================================================================
 *
 *  Copyright David Doria 2012 daviddoria@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#ifndef WeightedSumSquaredPixelDifference_hpp
#define WeightedSumSquaredPixelDifference_hpp

// STL
#include <stdexcept>

// Custom
#include <Helpers/Helpers.h>
#include <Helpers/ContainerInterface.h>
#include <ITKHelpers/ITKHelpers.h>
#include <ITKHelpers/ITKContainerInterface.h>

/**
 */
template <typename PixelType>
struct WeightedSumSquaredPixelDifference
{
  typedef std::vector<float> WeightVectorType;
  WeightVectorType Weights;
  
  WeightedSumSquaredPixelDifference(const WeightVectorType& weights) : Weights(weights) {}

  float operator()(const PixelType& a, const PixelType& b) const
  {
    assert(length(a) == length(b));
    assert(length(a) == this->Weights.size());
//    if(length(a) != this->Weights.size())
//    {
//      std::stringstream ss;
//      ss << "length(a) != Weights.size(). a is " << length(a) << " and weights is " << Weights.size();
//      throw std::runtime_error(ss.str());
//    }
    
    float pixelDifference = 0.0f;
    for(unsigned int component = 0; component < Helpers::length(a); ++component)
    {
      float componentDifference = this->Weights[component] * (Helpers::index(a,component) - Helpers::index(b,component)) *
                                                             (Helpers::index(a,component) - Helpers::index(b,component));
      pixelDifference += componentDifference;
    }

    return pixelDifference;
  }
};

#endif