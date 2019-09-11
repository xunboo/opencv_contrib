// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#ifndef _OPENCV_DNN_SUPERRES_DNNSUPERRESIMPL_HPP_
#define _OPENCV_DNN_SUPERRES_DNNSUPERRESIMPL_HPP_

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "opencv2/dnn.hpp"

/** @defgroup dnn_superres DNN used for super resolution

This module contains functionality for upscaling an image via convolutional neural networks.
The following four models are implemented:

- EDSR
- ESPCN
- FSRCNN
- LapSRN

There is also functionality for simply upscaling by bilinear or bicubic interpolation.

*/

namespace cv
{
namespace dnn
{
namespace dnn_superres
{
    //! @addtogroup dnn_superres
    //! @{

    /** @brief A class to upscale images via convolutional neural networks.
    The following four models are implemented:

    - edsr
    - espcn
    - fsrcnn
    - lapsrn
     */
    class CV_EXPORTS DnnSuperResImpl
    {
        private:
            /** @brief Net which holds the desired neural network
             */
            Net net;

        public:
            /** @brief Empty constructor
             */
            DnnSuperResImpl();

            /** @brief Constructor which immediately sets the desired model
            @param algo String containing one of the desired models:
                - __edsr__
                - __espcn__
                - __fsrcnn__
                - __lapsrn__
            @param scale Integer specifying the upscale factor
             */
            DnnSuperResImpl(std::string algo, int scale);

            /** @brief Set desired model
            @param algo String containing one of the desired models:
                - __edsr__
                - __espcn__
                - __fsrcnn__
                - __lapsrn__
            @param scale Integer specifying the upscale factor
             */
            void setModel(std::string algo, int scale);

            /** @brief Upsample via neural network
            @param img Image to upscale
            @param img_new Destination upscaled image
             */
            void upsample(Mat img, Mat img_new);
    };
    //! @}
}
}
}
#endif