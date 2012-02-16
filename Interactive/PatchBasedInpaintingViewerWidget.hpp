#ifndef PatchBasedInpaintingViewerWidget_HPP
#define PatchBasedInpaintingViewerWidget_HPP


#include "PatchBasedInpaintingViewerWidget.h" // Appease syntax parser

// ITK
#include "itkCastImageFilter.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkMaskImageFilter.h"
#include "itkRegionOfInterestImageFilter.h"

// Qt
#include <QGraphicsPixmapItem>

// VTK
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>

// Custom
#include "Helpers/Helpers.h"
#include "Helpers/OutputHelpers.h"
#include "Helpers/ITKVTKHelpers.h"
#include "Interactive/HelpersQt.h"
#include "InteractorStyleImageWithDrag.h"
#include "ImageProcessing/Mask.h"

template <typename TImage>
PatchBasedInpaintingViewerWidget<TImage>::PatchBasedInpaintingViewerWidget(TImage* const image) : Image(image)
{
  qRegisterMetaType<itk::ImageRegion<2> >("itkImageRegion");

  this->setupUi(this);

  this->ImageDimension[0] = 0;
  this->ImageDimension[1] = 0;
  this->ImageDimension[2] = 0;

  SetupScenes();

  this->InteractorStyle = vtkSmartPointer<InteractorStyleImageWithDrag>::New();

  this->Renderer = vtkSmartPointer<vtkRenderer>::New();
  this->qvtkWidget->GetRenderWindow()->AddRenderer(this->Renderer);

  this->Renderer->AddViewProp(ImageLayer.ImageSlice);

  this->InteractorStyle->SetCurrentRenderer(this->Renderer);
  this->qvtkWidget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(this->InteractorStyle);
  this->InteractorStyle->Init();

  this->Camera = new ImageCamera(this->Renderer);
}

template <typename TImage>
void PatchBasedInpaintingViewerWidget<TImage>::SetupScenes()
{
  QBrush brush;
  brush.setStyle(Qt::SolidPattern);
  brush.setColor(this->SceneBackground);

  this->TargetPatchScene = new QGraphicsScene();
  this->TargetPatchScene->setBackgroundBrush(brush);
  this->gfxTarget->setScene(TargetPatchScene);

  this->SourcePatchScene = new QGraphicsScene();
  this->SourcePatchScene->setBackgroundBrush(brush);
  this->gfxSource->setScene(SourcePatchScene);

  this->ResultPatchScene = new QGraphicsScene();
  this->ResultPatchScene->setBackgroundBrush(brush);
  this->gfxResult->setScene(ResultPatchScene);
}

template <typename TImage>
void PatchBasedInpaintingViewerWidget<TImage>::slot_UpdateImage()
{
  std::cout << "Update image." << std::endl;
  ITKVTKHelpers::ITKImageToVTKRGBImage(this->Image, this->ImageLayer.ImageData);

  int dims[3];
  this->ImageLayer.ImageData->GetDimensions(dims);
  if(dims[0] != ImageDimension[0] || dims[1] != ImageDimension[1] || dims[2] != ImageDimension[2])
    {
    this->Renderer->ResetCamera();
    ImageDimension[0] = dims[0];
    ImageDimension[1] = dims[1];
    ImageDimension[2] = dims[2];
    }

  this->qvtkWidget->GetRenderWindow()->Render();
}

template <typename TImage>
void PatchBasedInpaintingViewerWidget<TImage>::slot_UpdateSource(const itk::ImageRegion<2>& region)
{
  std::cout << "Update source." << std::endl;

  QImage sourceImage = HelpersQt::GetQImageColor(Image, region);
  //sourceImage = HelpersQt::FitToGraphicsView(sourceImage, gfxSource);
  QGraphicsPixmapItem* item = this->SourcePatchScene->addPixmap(QPixmap::fromImage(sourceImage));
  gfxSource->fitInView(item);
}

template <typename TImage>
void PatchBasedInpaintingViewerWidget<TImage>::slot_UpdateTarget(const itk::ImageRegion<2>& region)
{
  std::cout << "Update target." << std::endl;
  // We use the previous image and previous mask, but the current PotentialPairSets,
  // as these are the sets that were used to get to this state.

  // Target
  QImage targetImage = HelpersQt::GetQImageColor(Image, region);

  //targetImage = HelpersQt::FitToGraphicsView(targetImage, gfxTarget);
  QGraphicsPixmapItem* item = this->TargetPatchScene->addPixmap(QPixmap::fromImage(targetImage));
  gfxTarget->fitInView(item);
}

// template <typename TImage>
// void PatchBasedInpaintingViewerWidget<TImage>::DisplayResultPatch()
// {
  //QImage qimage(regionSize[0], regionSize[1], QImage::Format_RGB888);

  // TODO: Fix this
//   itk::ImageRegionIterator<FloatVectorImageType> sourceIterator(currentImage, this->SourcePatchToDisplay.GetRegion());
//   itk::ImageRegionIterator<FloatVectorImageType> targetIterator(currentImage, this->TargetPatchToDisplay.GetRegion());
//   itk::ImageRegionIterator<Mask> maskIterator(currentMask, this->TargetPatchToDisplay.GetRegion());
// 
//   FloatVectorImageType::Pointer resultPatch = FloatVectorImageType::New();
//   resultPatch->SetNumberOfComponentsPerPixel(currentImage->GetNumberOfComponentsPerPixel());
//   itk::Size<2> patchSize = ITKHelpers::SizeFromRadius(this->Settings.PatchRadius);
//   itk::ImageRegion<2> region;
//   region.SetSize(patchSize);
//   resultPatch->SetRegions(region);
//   resultPatch->Allocate();
// 
//   while(!maskIterator.IsAtEnd())
//     {
//     FloatVectorImageType::PixelType pixel;
// 
//     if(currentMask->IsHole(maskIterator.GetIndex()))
//       {
//       pixel = sourceIterator.Get();
//       }
//     else
//       {
//       pixel = targetIterator.Get();
//       }

//     itk::Offset<2> offset = sourceIterator.GetIndex() - this->SourcePatchToDisplay.GetRegion().GetIndex();
//     itk::Index<2> offsetIndex;
//     offsetIndex[0] = offset[0];
//     offsetIndex[1] = offset[1];
//     resultPatch->SetPixel(offsetIndex, pixel);

//     ++sourceIterator;
//     ++targetIterator;
//     ++maskIterator;
//     }

  // Color the center pixel
  //qimage.setPixel(regionSize[0]/2, regionSize[1]/2, this->CenterPixelColor.rgb());

//   qimage = HelpersQt::GetQImage<FloatVectorImageType>(resultPatch, resultPatch->GetLargestPossibleRegion(), this->ImageDisplayStyle);
// 
//   //qimage = HelpersQt::FitToGraphicsView(qimage, gfxResult);
//   this->ResultPatchScene->clear();
//   QGraphicsPixmapItem* item = this->ResultPatchScene->addPixmap(QPixmap::fromImage(qimage));
//   gfxResult->fitInView(item);
//   //this->ResultPatchScene->addPixmap(QPixmap());
//   //std::cout << "Set result patch." << std::endl;

//}

/*
void PatchBasedInpaintingViewerWidget::IterationComplete(const PatchPair& usedPatchPair)
{
  // TODO: The interfaces used in this function have changed.

  InpaintingIterationRecord iterationRecord;
  for(unsigned int i = 0; i < this->Inpainting->GetImagesToUpdate().size(); ++i)
    {
    std::cout << "Updating image " << i << std::endl;

    ITKHelpers::OutputImageType(this->Inpainting->GetImagesToUpdate()[i]);
    itk::ImageBase<2>::Pointer newImage = ITKHelpers::CreateImageWithSameType(this->Inpainting->GetImagesToUpdate()[i]);
    ITKHelpers::OutputImageType(newImage);
    // TODO: Make this deep copy work
    //ITKHelpers::DeepCopy(this->Inpainting->GetImagesToUpdate()[i], newImage);
    }
  std::cout << "Finished creating record images." << std::endl;

}*/


#endif