#include "BoundaryEnergy.h" // Appease syntax parser

#include "ITKHelpers/ITKHelpers.h"
#include "Mask/MaskOperations.h"
#include "ImageProcessing/PixelFilterFunctors.hpp"

template<typename TImage>
BoundaryEnergy<TImage>::BoundaryEnergy(const TImage* const image, const Mask* const mask) : Image(image), MaskImage(mask)
{

}

template<typename TImage>
float BoundaryEnergy<TImage>::operator()(const itk::ImageRegion<2>& region)
{
  // Get the boundary of the valid region
  Mask::BoundaryImageType::Pointer boundaryImage = Mask::BoundaryImageType::New();
  this->MaskImage->CreateBoundaryImageInRegion(region, boundaryImage.GetPointer(), Mask::VALID);

  // We are unsure about the values of the boundary pixels, but they are definitely greater than .5
  // (we'd hope non-boundary=0 and boundary = 1 or 255)
  GreaterThanOrEqualFunctor<Mask::BoundaryImageType::PixelType> greaterThanOrEqualFunctor(1);
  std::vector<itk::Index<2> > pixelsSatisfyingFunctor = PixelsSatisfyingFunctor(boundaryImage.GetPointer(),
                                                        region, greaterThanOrEqualFunctor);

  if(pixelsSatisfyingFunctor.size() == 0)
  {
    std::stringstream ss;
    ss << "Cannot compute boundary energy - there are no boundary pixels in the specified region: " << region;
    throw std::runtime_error(ss.str());
  }

  float totalDifference = 0.0f;
  for(unsigned int i = 0; i < pixelsSatisfyingFunctor.size(); ++i)
  {
    typename TImage::PixelType averageMaskedNeighborValue =
      MaskOperations::AverageHoleNeighborValue(Image, MaskImage, pixelsSatisfyingFunctor[i]);

    typename TImage::PixelType averageValidNeighborValue =
      MaskOperations::AverageValidNeighborValue(Image, MaskImage, pixelsSatisfyingFunctor[i]);

    totalDifference += Difference(averageMaskedNeighborValue, averageValidNeighborValue);
  }

  float averageDifference = totalDifference / static_cast<float>(pixelsSatisfyingFunctor.size());
  return averageDifference;
}

template<typename TImage>
float BoundaryEnergy<TImage>::operator()(const itk::ImageRegion<2>& sourceRegion, const itk::ImageRegion<2>& targetRegion)
{
  // Get the boundary of the valid region
  Mask::BoundaryImageType::Pointer boundaryImage = Mask::BoundaryImageType::New();
  this->MaskImage->CreateBoundaryImageInRegion(targetRegion, boundaryImage.GetPointer(), Mask::VALID);

  // We are unsure about the values of the boundary pixels, but they are definitely greater than .5
  // (we'd hope non-boundary=0 and boundary = 1 or 255)
  GreaterThanOrEqualFunctor<Mask::BoundaryImageType::PixelType> greaterThanOrEqualFunctor(1);
  std::vector<itk::Index<2> > boundaryPixels = PixelsSatisfyingFunctor(boundaryImage.GetPointer(),
                                                        targetRegion, greaterThanOrEqualFunctor);

  if(boundaryPixels.size() == 0)
  {
    std::stringstream ss;
    ss << "Cannot compute boundary energy - there are no boundary pixels in the specified target region: " << targetRegion;
    ITKHelpers::SetRegionToConstant(MaskImage, targetRegion, 122);
    ITKHelpers::WriteImage(MaskImage, "BoundaryError.png");
    throw std::runtime_error(ss.str());
  }

  float totalDifference = 0.0f;
  for(unsigned int i = 0; i < boundaryPixels.size(); ++i)
  {
    // Compute the average of the target pixels in the target region
    typename TImage::PixelType averageMaskedNeighborValue =
      MaskOperations::AverageHoleNeighborValue(Image, MaskImage, boundaryPixels[i]);

    // Compute the average of the source pixels in the source region
    itk::Offset<2> boundaryPixelOffsetFromTargetCorner = boundaryPixels[i] - targetRegion.GetIndex();

    std::vector<itk::Offset<2> > validPixelOffsets = MaskImage->GetValidNeighborOffsets(boundaryPixels[i]);
    //std::cout << "There are " << validPixelOffsets.size() << " offsets." << std::endl;

    std::vector<itk::Index<2> > sourceRegionValidPixelIndices =
       ITKHelpers::OffsetsToIndices(validPixelOffsets, sourceRegion.GetIndex() + boundaryPixelOffsetFromTargetCorner);

    //std::cout << "There are " << sourceRegionValidPixelIndices.size() << " indices." << std::endl;

    typename TImage::PixelType averageValidNeighborValue = ITKHelpers::AverageOfPixelsAtIndices(Image,
                                                                                                sourceRegionValidPixelIndices);

    //std::cout << "averageValidNeighborValue: " << averageValidNeighborValue << std::endl;

    totalDifference += Difference(averageMaskedNeighborValue, averageValidNeighborValue);
  }

  float averageDifference = totalDifference / static_cast<float>(boundaryPixels.size());
  return averageDifference;
}

template<typename TImage>
template <typename T>
float BoundaryEnergy<TImage>::Difference(const T& a, const T& b)
{
  return a - b;
}

template<typename TImage>
float BoundaryEnergy<TImage>::Difference(const VectorPixelType& a, const VectorPixelType& b)
{
  return (a-b).GetNorm();
}
