// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#include "precomp.hpp"

#include "opencv2/dnn_superres.hpp"

namespace cv
{
    namespace dnn_superres
    {

        int DnnSuperResImpl::layer_loaded = 0;

        DnnSuperResImpl::DnnSuperResImpl()
        {
            if( !this->layer_loaded )
            {
                layer_loaded = true;
                registerLayers();
            }
        }

        DnnSuperResImpl::DnnSuperResImpl(std::string algo, int scale) : alg(algo), sc(scale)
        {
            if( !this->layer_loaded )
            {
                layer_loaded = true;
                registerLayers();
            }
        }

        void DnnSuperResImpl::registerLayers()
        {
            //Register custom layer that implements pixel shuffling
            std::string name = "DepthToSpace";
            dnn::LayerParams layerParams = dnn::LayerParams();
            cv::dnn::LayerFactory::registerLayer("DepthToSpace", DepthToSpace::create);
        }

        void DnnSuperResImpl::readModel(std::string path)
        {
            if ( path.size() )
            {
                this->net = dnn::readNetFromTensorflow(path);
                std::cout << "Successfully loaded model. \n";
            }
            else
            {
                std::cout << "Could not load model. \n";
            }
        }

        void DnnSuperResImpl::readModel(std::string weights, std::string definition)
        {
            if ( weights.size() && definition.size() )
            {
                this->net = dnn::readNetFromTensorflow(weights, definition);
                std::cout << "Successfully loaded model. \n";
            }
            else
            {
                std::cout << "Could not load model. \n";
            }
        }

        void DnnSuperResImpl::setModel(std::string algo, int scale)
        {
            this->sc = scale;
            this->alg = algo;
        }

        void DnnSuperResImpl::upsample(Mat img, Mat &img_new)
        {
            if( !net.empty() )
            {
                if ( this->alg == "espcn" || this->alg == "lapsrn" || this->alg == "fsrcnn" )
                {
                    //Preprocess the image: convert to YCrCb float image and normalize
                    Mat preproc_img;
                    preprocess_YCrCb(img, preproc_img);

                    //Split the image: only the Y channel is used for inference
                    Mat ycbcr_channels[3];
                    split(preproc_img, ycbcr_channels);

                    Mat Y = ycbcr_channels[0];

                    //Create blob from image so it has size 1,1,Width,Height
                    cv::Mat blob;
                    dnn::blobFromImage(Y, blob, 1.0);

                    //Get the HR output
                    this->net.setInput(blob);

                    Mat blob_output = this->net.forward();

                    //Convert from blob
                    std::vector <Mat> model_outs;
                    dnn::imagesFromBlob(blob_output, model_outs);
                    Mat out_img = model_outs[0];

                    //Reconstruct: upscale the Cr and Cb space and merge the three layer
                    reconstruct_YCrCb(out_img, preproc_img, img_new, this->sc);
                }
                else if( this->alg == "edsr" )
                {
                    //BGR mean of the Div2K dataset
                    Scalar mean =  Scalar(103.1545782, 111.561547, 114.35629928);

                    //Convert to float
                    Mat float_img;
                    img.convertTo(float_img, CV_32F, 1.0);

                    //Create blob from image so it has size [1,3,Width,Height] and subtract dataset mean
                    cv::Mat blob;
                    dnn::blobFromImage(float_img, blob, 1.0, Size(), mean);

                    //Get the HR output
                    this->net.setInput(blob);
                    Mat blob_output = this->net.forward();

                    //Convert from blob
                    std::vector <Mat> model_outs;
                    dnn::imagesFromBlob(blob_output, model_outs);
                    img_new = model_outs[0];

                    //Post-process: add mean.
                    img_new = img_new + Scalar(103.1545782, 111.561547, 114.35629928);

                    img_new.convertTo(img_new, CV_8U);
                }
                else
                {
                    //
                }
            }
            else
            {
                std::cout << "Model not specified. Please set model via setModel(). \n";
            }
        }

        void DnnSuperResImpl::upsampleMultioutput(Mat img, std::vector<Mat> &imgs_new, std::vector<int> scale_factors, std::vector<String> node_names)
        {
            CV_Assert(scale_factors.size() == node_names.size());
            CV_Assert(!scale_factors.empty());
            CV_Assert(!node_names.empty());

            if ( this->alg != "lapsrn" )
            {
                std::cout << "Only LapSRN support multiscale upsampling for now!" << std::endl;
                return;
            }

            if( !net.empty() )
            {
                if ( this->alg == "lapsrn" )
                {
                    Mat orig = img;

                    //Preprocess the image: convert to YCrCb float image and normalize
                    Mat preproc_img;
                    preprocess_YCrCb(orig, preproc_img);

                    //Split the image: only the Y channel is used for inference
                    Mat ycbcr_channels[3];
                    split(preproc_img, ycbcr_channels);

                    Mat Y = ycbcr_channels[0];

                    //Create blob from image so it has size 1,1,Width,Height
                    cv::Mat blob;
                    dnn::blobFromImage(Y, blob, 1.0);

                    //Get the HR outputs
                    std::vector <Mat> outputs_blobs;
                    this->net.setInput(blob);
                    this->net.forward(outputs_blobs, node_names);

                    for(unsigned int i = 0; i < scale_factors.size(); i++)
                    {
                        std::vector <Mat> model_outs;
                        dnn::imagesFromBlob(outputs_blobs[i], model_outs);
                        Mat out_img = model_outs[0];
                        Mat reconstructed;

                        reconstruct_YCrCb(out_img, preproc_img, reconstructed, scale_factors[i]);

                        imgs_new.push_back(reconstructed);
                    }
                }
            }
            else
            {
                std::cout << "Model not specified. Please set model via setModel(). \n";
            }
        }


        void DnnSuperResImpl::upsampleVideo(String inputPath, String outputPath)
        {
            VideoCapture inputVideo(inputPath);
            int ex = static_cast<int>(inputVideo.get(CAP_PROP_FOURCC));
            Size S = Size((int) inputVideo.get(CAP_PROP_FRAME_WIDTH) * this->sc,
                            (int) inputVideo.get(CAP_PROP_FRAME_HEIGHT) * this->sc);

            VideoWriter outputVideo;
            outputVideo.open(outputPath, ex, inputVideo.get(CAP_PROP_FPS), S, true);

            if (!inputVideo.isOpened())
            {
                std::cout  << "Could not open the video." << std::endl;
                return;
            }

            for(;;)
            {
                Mat frame, outputFrame;
                inputVideo >> frame;

                if ( frame.empty() )
                    break;

                upsample(frame, outputFrame);
                outputVideo << outputFrame;
            }

            inputVideo.release();
            outputVideo.release();
        }

        int DnnSuperResImpl::getScale()
        {
            return this->sc;
        }

        std::string DnnSuperResImpl::getAlgorithm()
        {
            return this->alg;
        }

        void DnnSuperResImpl::preprocess_YCrCb(const Mat inpImg, Mat &outImg)
        {
            if ( inpImg.type() == CV_8UC1 )
            {
                Mat ycrcb;
                inpImg.convertTo(outImg, CV_32F, 1.0 / 255.0);
            }
            else if ( inpImg.type() == CV_32FC1 )
            {
                Mat ycrcb;
                inpImg.convertTo(outImg, CV_32F, 1.0 / 255.0);
            }
            else if ( inpImg.type() == CV_32FC3 )
            {
                Mat img_float;
                inpImg.convertTo(img_float, CV_32F, 1.0 / 255.0);
                cvtColor(img_float, outImg, COLOR_BGR2YCrCb);
            }
            else if ( inpImg.type() == CV_8UC3 )
            {
                Mat ycrcb;
                cvtColor(inpImg, ycrcb, COLOR_BGR2YCrCb);
                ycrcb.convertTo(outImg, CV_32F, 1.0 / 255.0);
            }
            else
            {
                std::cout << "Not supported image type!" << std::endl;
            }
        }

        void DnnSuperResImpl::reconstruct_YCrCb(const Mat inpImg, const Mat origImg, Mat &outImg, int scale)
        {
            if ( origImg.type() == CV_32FC3 )
            {
                Mat orig_channels[3];
                split(origImg, orig_channels);

                Mat Cr, Cb;
                cv::resize(orig_channels[1], Cr, cv::Size(), scale, scale);
                cv::resize(orig_channels[2], Cb, cv::Size(), scale, scale);

                std::vector <Mat> channels;
                channels.push_back(inpImg);
                channels.push_back(Cr);
                channels.push_back(Cb);

                Mat merged_img;
                merge(channels, merged_img);

                Mat merged_8u_img;
                merged_img.convertTo(merged_8u_img, CV_8U, 255.0);

                cvtColor(merged_8u_img, outImg, COLOR_YCrCb2BGR);
            }
            else if ( origImg.type() == CV_32FC1 )
            {
                inpImg.convertTo(outImg, CV_8U, 255.0);
            }
            else
            {
                std::cout << "Not supported image type!" << std::endl;
            }
        }

        DnnSuperResImpl::DepthToSpace::DepthToSpace(const cv::dnn::LayerParams &params) : Layer(params)
        {

        }

        cv::Ptr<cv::dnn::Layer> DnnSuperResImpl::DepthToSpace::create(cv::dnn::LayerParams &params)
        {
            return cv::Ptr<cv::dnn::Layer>(new DepthToSpace(params));
        }

        bool DnnSuperResImpl::DepthToSpace::getMemoryShapes(const std::vector <std::vector<int>> &inputs,
                                                            const int,
                                                            std::vector <std::vector<int>> &outputs,
                                                            std::vector <std::vector<int>> &) const
        {
            std::vector<int> outShape(4);

            int scale;
            if( inputs[0][1] == 4 || inputs[0][1] == 9 || inputs[0][1] == 16 ) //Only one image channel
            {
                scale = static_cast<int>(sqrt(inputs[0][1]));
            }
            else // Three image channels
            {
                scale = static_cast<int>(sqrt(inputs[0][1]/3));
            }

            outShape[0] = inputs[0][0];
            outShape[1] = static_cast<int>(inputs[0][1] / pow(scale,2));
            outShape[2] = static_cast<int>(scale * inputs[0][2]);
            outShape[3] = static_cast<int>(scale * inputs[0][3]);

            outputs.assign(4, outShape);

            return false;
        }

        void DnnSuperResImpl::DepthToSpace::forward(cv::InputArrayOfArrays inputs_arr,
                                                    cv::OutputArrayOfArrays outputs_arr,
                                                    cv::OutputArrayOfArrays)
        {
            std::vector <cv::Mat> inputs, outputs;
            inputs_arr.getMatVector(inputs);
            outputs_arr.getMatVector(outputs);
            cv::Mat &inp = inputs[0];
            cv::Mat &out = outputs[0];
            const float *inpData = (float *) inp.data;
            float *outData = (float *) out.data;

            const int inpHeight = inp.size[2];
            const int inpWidth = inp.size[3];

            const int numChannels = out.size[1];
            const int outHeight = out.size[2];
            const int outWidth = out.size[3];

            int scale = int(outHeight / inpHeight);
            int count = 0;

            for (int ch = 0; ch < numChannels; ch++)
            {
                for (int y = 0; y < outHeight; y++)
                {
                    for (int x = 0; x < outWidth; x++)
                    {
                        int x_coord = static_cast<int>(floor((y / scale)));
                        int y_coord = static_cast<int>(floor((x / scale)));
                        int c_coord = numChannels * scale * (y % scale) + numChannels * (x % scale) + ch;

                        int index = (((c_coord * inpHeight) + x_coord) * inpWidth) + y_coord;

                        outData[count++] = inpData[index];
                    }
                }
            }
        }
    }
}