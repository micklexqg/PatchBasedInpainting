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

#ifndef WeightedHSVSSDFull_hpp
#define WeightedHSVSSDFull_hpp

// STL
#include <stdexcept>

// Submodules
#include <Helpers/Helpers.h>
#include <Helpers/ContainerInterface.h>
#include <ITKHelpers/ITKHelpers.h>
#include <ITKHelpers/ITKContainerInterface.h>

/**
  * This class is designed to compute the difference between an HSV* image. That is, the
  * first channel is H, and there can be as many additional channels that are all to be compared
  * in a standard SSD fashion. The H channel is compared using a cyclic difference to handle the
  * wrapping nature of the values. This class is different from WeightedHSVSSD in that it considers
  * any additional channels after H, S, and V as well.
  */
template <typename PixelType>
struct WeightedHSVSSDFull
{
  typedef std::vector<float> WeightVectorType;
  WeightVectorType Weights;
  
  WeightedHSVSSDFull(const WeightVectorType& weights) : Weights(weights) {}

  Helpers::HSV_H_Difference HDifference;

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

    pixelDifference += this->Weights[0] * HDifference(Helpers::index(a,0), Helpers::index(b,0)) *
                       HDifference(Helpers::index(a,0), Helpers::index(b,0));

    for(unsigned int component = 1; component < Helpers::length(a); ++component) // start at component 1, because we have handled 0 (H) separately)
    {
      float componentDifference = this->Weights[component] * (Helpers::index(a,component) - Helpers::index(b,component)) *
                                                             (Helpers::index(a,component) - Helpers::index(b,component));
      pixelDifference += componentDifference;
    }

    return pixelDifference;
  }
};

/**
  * This specialization handles pixels of type CovariantVector<T,N>.
  */
template <>
template <typename T, unsigned int N>
class WeightedHSVSSDFull<itk::CovariantVector<T, N> >
{
public:
  typedef itk::CovariantVector<T, N> PixelType;

  typedef std::vector<float> WeightVectorType;
  WeightVectorType Weights;

  Helpers::HSV_H_Difference HDifference;

  WeightedHSVSSDFull(const WeightVectorType& weights) : Weights(weights) {}

  // Using ITK function. This function uses casts to double internally, so it is not ideal for integer types
  // with values that you know will not cause overflow (unnecessarily slow)
//  float operator()(const PixelType& a, const PixelType& b) const
//  {
////    std::cout << "SumSquaredPixelDifference <itk::CovariantVector<T, N> >::operator()" << std::endl;
//    return (a-b).GetSquaredNorm();
//  }

  // Manual
  float operator()(const PixelType& a, const PixelType& b) const
  {
//    std::cout << "SumSquaredPixelDifference <itk::CovariantVector<T, N> >::operator()" << std::endl;
//    T sum = std::numeric_limits<T>::zero;
    T sum = static_cast<T>(0);

    sum += this->Weights[0] * HDifference(a[0], b[0]) * HDifference(a[0], b[0]);

    for(unsigned int component = 1; component < N; component++) // start at component 1, because we have handled 0 (H) separately)
    {
      sum += this->Weights[component] * (a[component] - b[component]) * (a[component] - b[component]);
    }
    return sum;
  }
};

#endif
