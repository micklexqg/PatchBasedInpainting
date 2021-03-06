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

#ifndef InteractiveInpaintingGMH_HPP
#define InteractiveInpaintingGMH_HPP

// Custom
#include "Utilities/IndirectPriorityQueue.h"

// Qt
#include <QtConcurrentRun>

// Submodules
#include <Helpers/Helpers.h>

// Pixel descriptors
#include "PixelDescriptors/ImagePatchPixelDescriptor.h"

// Descriptor visitors
#include "Visitors/DescriptorVisitors/ImagePatchDescriptorVisitor.hpp"
#include "Visitors/DescriptorVisitors/CompositeDescriptorVisitor.hpp"

// Nearest neighbors visitor
#include "Visitors/NearestNeighborsDisplayVisitor.hpp"

// Nearest neighbors
#include "NearestNeighbor/LinearSearchKNNProperty.hpp"
#include "NearestNeighbor/DefaultSearchBest.hpp"
#include "NearestNeighbor/LinearSearchBest/Property.hpp"
#include "NearestNeighbor/LinearSearchBest/First.hpp"
#include "NearestNeighbor/TopPatchListOrManual.hpp"
#include "NearestNeighbor/VerifyOrManual.hpp"
#include "NearestNeighbor/FirstValidDescriptor.hpp"
#include "NearestNeighbor/SortByRGBTextureGradient.hpp"
#include "NearestNeighbor/KNNSearchAndSort.hpp"

// Pixel descriptors
#include "PixelDescriptors/ImagePatchPixelDescriptor.h"

// Acceptance visitors
#include "Visitors/AcceptanceVisitors/GMHAcceptanceVisitor.hpp"

// Information visitors
#include "Visitors/InformationVisitors/DisplayVisitor.hpp"
#include "Visitors/InformationVisitors/FinalImageWriterVisitor.hpp"

// Inpainting visitors
#include "Visitors/InpaintingVisitors/InpaintingVisitor.hpp"
#include "Visitors/ReplayVisitor.hpp"
#include "Visitors/InformationVisitors/LoggerVisitor.hpp"
#include "Visitors/InpaintingVisitors/CompositeInpaintingVisitor.hpp"
#include "Visitors/InformationVisitors/DebugVisitor.hpp"

// Nearest neighbors
#include "NearestNeighbor/LinearSearchBest/Property.hpp"

// Initializers
#include "Initializers/InitializeFromMaskImage.hpp"
#include "Initializers/InitializePriority.hpp"

// Inpainters
#include "Inpainters/CompositePatchInpainter.hpp"
#include "Inpainters/PatchInpainter.hpp"

// Difference functions
#include "DifferenceFunctions/Patch/ImagePatchDifference.hpp"
#include "DifferenceFunctions/Pixel/SumSquaredPixelDifference.hpp"

// Utilities
#include "Utilities/PatchHelpers.h"

// Inpainting
#include "Algorithms/InpaintingAlgorithmWithVerification.hpp"

// Priority
#include "Priority/PriorityCriminisi.h"
#include "Priority/PriorityConfidence.h"

// Boost
#include <boost/graph/grid_graph.hpp>
#include <boost/property_map/property_map.hpp>

// GUI
#include "Interactive/BasicViewerWidget.h"
#include "Interactive/PriorityViewerWidget.h"

template <typename TImage>
void InteractiveInpaintingGMH(typename itk::SmartPointer<TImage> originalImage,
                              Mask::Pointer mask, const unsigned int patchHalfWidth,
                              const unsigned int knn,
                              const float maxAllowedDifference, const std::string& outputFileName)
{
  // Get the region so that we can reference it without referring to a particular image
  itk::ImageRegion<2> fullRegion = originalImage->GetLargestPossibleRegion();

  // Blur the image enough so that the gradients are useful for the priority computation.
  typedef TImage BlurredImageType; // Usually the blurred image is the same type as the original image.
  typename BlurredImageType::Pointer blurredImage = BlurredImageType::New();
  float blurVariance = 3.0f;
  MaskOperations::MaskedBlur(originalImage.GetPointer(), mask, blurVariance, blurredImage.GetPointer());

  // Blur the image a little bit so that the SSD comparisons are less wild.
  typedef TImage BlurredImageType; // Usually the blurred image is the same type as the original image.
  typename BlurredImageType::Pointer slightlyBlurredImage = BlurredImageType::New();
  float slightBlurVariance = 1.2f;
  MaskOperations::MaskedBlur(originalImage.GetPointer(), mask, slightBlurVariance, slightlyBlurredImage.GetPointer());

  typedef ImagePatchPixelDescriptor<TImage> ImagePatchPixelDescriptorType;

  // Create the graph
  typedef boost::grid_graph<2> VertexListGraphType;
  boost::array<std::size_t, 2> graphSideLengths = { { fullRegion.GetSize()[0],
                                                      fullRegion.GetSize()[1] } };
  std::shared_ptr<VertexListGraphType> graph(new VertexListGraphType(graphSideLengths));
  typedef boost::graph_traits<VertexListGraphType>::vertex_descriptor VertexDescriptorType;

  // Queue
  typedef IndirectPriorityQueue<VertexListGraphType> BoundaryNodeQueueType;
  std::shared_ptr<BoundaryNodeQueueType> boundaryNodeQueue(new BoundaryNodeQueueType(*graph));

  // Create the descriptor map. This is where the data for each pixel is stored.
  typedef boost::vector_property_map<ImagePatchPixelDescriptorType,
      BoundaryNodeQueueType::IndexMapType> ImagePatchDescriptorMapType;
  std::shared_ptr<ImagePatchDescriptorMapType> imagePatchDescriptorMap(
        new ImagePatchDescriptorMapType(num_vertices(*graph), *(boundaryNodeQueue->GetIndexMap())));

  // Create the patch inpainters.
  typedef PatchInpainter<TImage> OriginalImageInpainterType;
  std::shared_ptr<OriginalImageInpainterType> originalImageInpainter(
      new OriginalImageInpainterType(patchHalfWidth, originalImage, mask));

  typedef PatchInpainter<BlurredImageType> BlurredImageInpainterType;
  std::shared_ptr<BlurredImageInpainterType> blurredImageInpainter(
      new BlurredImageInpainterType(patchHalfWidth, blurredImage, mask));

  std::shared_ptr<BlurredImageInpainterType> slightlyBlurredImageInpainter(
      new BlurredImageInpainterType(patchHalfWidth, slightlyBlurredImage, mask));

  // Create a composite inpainter.
  /** We only have to store the composite inpainter in the class, as it stores shared_ptrs
    * to all of the individual inpainters. If the composite inpainter says in scope, the
    * individual inpainters do as well.
    */
  std::shared_ptr<CompositePatchInpainter> compositeInpainter(new CompositePatchInpainter);
  compositeInpainter->AddInpainter(originalImageInpainter);
  compositeInpainter->AddInpainter(blurredImageInpainter);
  compositeInpainter->AddInpainter(slightlyBlurredImageInpainter);

  // Create the priority function
  typedef PriorityCriminisi<TImage> PriorityType;
  std::shared_ptr<PriorityType> priorityFunction(
        new PriorityType(blurredImage, mask, patchHalfWidth));
//  typedef PriorityConfidence PriorityType;
//  std::shared_ptr<PriorityType> priorityFunction(
//        new PriorityType(mask, patchHalfWidth));


  // Create the descriptor visitor
  typedef ImagePatchDescriptorVisitor<VertexListGraphType, TImage, ImagePatchDescriptorMapType>
          ImagePatchDescriptorVisitorType;

  // Use the slightly blurred image here, as this is where the patch objects get created, and later these patch objects
  // are passed to the SSD function.
  std::shared_ptr<ImagePatchDescriptorVisitorType> imagePatchDescriptorVisitor(
        new ImagePatchDescriptorVisitorType(slightlyBlurredImage.GetPointer(), mask,
                                            imagePatchDescriptorMap, patchHalfWidth));

  // Acceptance visitor. Use the slightly blurred image here, as this the gradients will be less noisy.
  unsigned int numberOfBinsPerChannel = 40;

  typedef GMHAcceptanceVisitor<VertexListGraphType, TImage> GMHAcceptanceVisitorType;
  std::shared_ptr<GMHAcceptanceVisitorType> gmhAcceptanceVisitor(
        new GMHAcceptanceVisitorType(slightlyBlurredImage.GetPointer(), mask, patchHalfWidth,
                                   maxAllowedDifference, numberOfBinsPerChannel));

  // Create the inpainting visitor
//  typedef InpaintingVisitor<VertexListGraphType, BoundaryNodeQueueType,
//                            ImagePatchDescriptorVisitorType, CompositeAcceptanceVisitorType,
//                            PriorityType, TImage>
//                            InpaintingVisitorType;
//  std::shared_ptr<InpaintingVisitorType> inpaintingVisitor(
//        new InpaintingVisitorType(mask, boundaryNodeQueue,
//                                  imagePatchDescriptorVisitor, compositeAcceptanceVisitor,
//                                  priorityFunction, patchHalfWidth,
//                                  "InpaintingVisitor", originalImage.GetPointer()));

  typedef InpaintingVisitor<VertexListGraphType, BoundaryNodeQueueType,
                            ImagePatchDescriptorVisitorType, GMHAcceptanceVisitorType,
                            PriorityType>
                            InpaintingVisitorType;
  std::shared_ptr<InpaintingVisitorType> inpaintingVisitor(
        new InpaintingVisitorType(mask, boundaryNodeQueue,
                                  imagePatchDescriptorVisitor, gmhAcceptanceVisitor,
                                  priorityFunction, patchHalfWidth,
                                  "InpaintingVisitor"));

  typedef DisplayVisitor<VertexListGraphType, TImage> DisplayVisitorType;
  std::shared_ptr<DisplayVisitorType> displayVisitor(
        new DisplayVisitorType(originalImage, mask, patchHalfWidth));

  typedef FinalImageWriterVisitor<VertexListGraphType, TImage> FinalImageWriterVisitorType;
  std::shared_ptr<FinalImageWriterVisitorType> finalImageWriterVisitor(
        new FinalImageWriterVisitorType(originalImage, outputFileName));


  typedef CompositeInpaintingVisitor<VertexListGraphType> CompositeInpaintingVisitorType;
  std::shared_ptr<CompositeInpaintingVisitorType> compositeInpaintingVisitor(new CompositeInpaintingVisitorType);
  compositeInpaintingVisitor->AddVisitor(inpaintingVisitor);
  compositeInpaintingVisitor->AddVisitor(displayVisitor);
  compositeInpaintingVisitor->AddVisitor(finalImageWriterVisitor);

  InitializePriority(mask, boundaryNodeQueue.get(), priorityFunction.get());

  // Initialize the boundary node queue from the user provided mask image.
  InitializeFromMaskImage<CompositeInpaintingVisitorType, VertexDescriptorType>(
        mask, compositeInpaintingVisitor.get());
  std::cout << "InteractiveInpaintingWithVerification: There are " << boundaryNodeQueue->size()
            << " nodes in the boundaryNodeQueue" << std::endl;

  typedef SumSquaredPixelDifference<typename TImage::PixelType> PixelDifferenceType;
  typedef ImagePatchDifference<ImagePatchPixelDescriptorType, PixelDifferenceType >
            ImagePatchDifferenceType;

  // Create the nearest neighbor finders
  typedef LinearSearchKNNProperty<ImagePatchDescriptorMapType,
                                  ImagePatchDifferenceType > KNNSearchType;

  std::shared_ptr<KNNSearchType> knnSearch(new KNNSearchType(imagePatchDescriptorMap, knn));

  // Since we are using a KNNSearchAndSort, we just have to return the top patch after the sort,
  // so we use this trival Best searcher.
  typedef LinearSearchBestFirst BestSearchType;
  std::shared_ptr<BestSearchType> bestSearch;

  // Use the slightlyBlurredImage here because we want the gradients to be less noisy
  typedef SortByRGBTextureGradient<ImagePatchDescriptorMapType,
                                   TImage > NeighborSortType;
  std::shared_ptr<NeighborSortType> neighborSortType(
        new NeighborSortType(*imagePatchDescriptorMap, slightlyBlurredImage.GetPointer(), mask, numberOfBinsPerChannel));

  typedef KNNSearchAndSort<KNNSearchType, NeighborSortType, TImage> SearchAndSortType;
  std::shared_ptr<SearchAndSortType> searchAndSort(
        new SearchAndSortType(knnSearch, neighborSortType, originalImage));

  typedef BasicViewerWidget<TImage> BasicViewerWidgetType;
//  std::shared_ptr<BasicViewerWidgetType> basicViewer(new BasicViewerWidgetType(originalImage, mask)); // This shared_ptr will go out of scope when this function ends, so the window will immediately close
  BasicViewerWidgetType* basicViewer =
      new BasicViewerWidgetType(originalImage, mask);
//  std::cout << "basicViewer pointer: " << basicViewer << std::endl;
  basicViewer->ConnectVisitor(displayVisitor.get());

  // If the acceptance tests fail, prompt the user to select a patch. Pass the basicViewer as the parent so that we can position the top pathces dialog properly.
//  typedef TopPatchListOrManual<TImage> ManualSearchType;
//  std::shared_ptr<ManualSearchType> manualSearchBest(new ManualSearchType(originalImage, mask,
//                                                                          patchHalfWidth, basicViewer));

  typedef VerifyOrManual<TImage> ManualSearchType;
  std::shared_ptr<ManualSearchType> manualSearchBest(
        new ManualSearchType(originalImage, mask, patchHalfWidth, basicViewer));

  // Connect the viewer to the top patches selection widget
  basicViewer->ConnectWidget(manualSearchBest->GetTopPatchesDialog());
  basicViewer->show();

  // View the priority of boundary pixels at each iteration
//  typedef PriorityViewerWidget<PriorityType, BoundaryNodeQueueType::BoundaryStatusMapType> PriorityViewerType;
//  PriorityViewerType* priorityViewer = new PriorityViewerType(priorityFunction.get(), fullRegion.GetSize(),
//                                                              boundaryNodeQueue->GetBoundaryStatusMap());
//  QObject::connect(displayVisitor.get(), SIGNAL(signal_RefreshImage()),
//                   priorityViewer, SLOT(slot_UpdateImage()),
//                   Qt::BlockingQueuedConnection);
//  priorityViewer->show();

  // Run the remaining inpainting with interaction
  std::cout << "Running inpainting..." << std::endl;

  QtConcurrent::run(boost::bind(InpaintingAlgorithmWithVerification<
                                VertexListGraphType, CompositeInpaintingVisitorType,
                                BoundaryNodeQueueType, SearchAndSortType, BestSearchType,
                                ManualSearchType, CompositePatchInpainter>,
                                graph, compositeInpaintingVisitor, boundaryNodeQueue, searchAndSort,
                                bestSearch, manualSearchBest, compositeInpainter));

}

#endif
