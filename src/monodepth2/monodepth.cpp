#include "monodepth.h"

//headers for pytorch(c++)
#include <torch/script.h>
#include <torch/torch.h>

#include <torch/serialize/tensor.h>
#include <torch/serialize.h>

// headers for opencv
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <iostream>
#include <assert.h>

using namespace std;
using namespace cv; // opencv
using namespace at; //pytorch c++ api

namespace dso
{
    //read torchscript
    MonoDepth::MonoDepth(const std::string &model_file, int use_gpu) : model_file_(model_file), use_gpu_(use_gpu)
    {

        model = torch::jit::load(model_file_);
        if (use_gpu_ ==true)
        {
            model.to(at::kCUDA);
        }
        else
        {
            model.to(at::kCPU);
        }
    }


    // Destructor (interface)   
    MonoDepth::~MonoDepth() = default;

    void MonoDepth::inference(cv::Mat &image, cv::Mat &invdepth){
        // For PackNet01_HR_velsup_CStoK_jit.pt, height==384 and width==1280
        // We were able to also run this successfully with 192x640. Probably worth a quick investigation
        // to figure out what's going on there.
        // This code may need to be adjusted if, for some reason, images fed into DSO are not the same
        // size as the images the net was trained on.
        const int height = image.rows;
        const int width = image.cols;
        invdepth = MonoDepth::inference(image, height, width); // Packnet outputs inverse-depth, not depth
    }

    cv::Mat MonoDepth::inference(cv::Mat &image,const int &height, const int &width)
    {
        //images_to_tensors
        assert(!image.empty());
        cv::Mat input_mat;
        cv::resize(image, input_mat, cv::Size(width, height));

        // All tests of DSO ran with this, but on closer inspection I'm not sure it's needed and may even be negatively impacting results
        // Should be tested side by side, both with and without this conversion
        //[0, 255]
        input_mat.convertTo(input_mat,CV_32FC1, 1./255.);
        //[0, 1]

        //!transform a cv::Mat into a tensor   [B, H, W, C]
        torch::Tensor tensor_image = torch::from_blob(input_mat.data, {1,input_mat.rows, input_mat.cols,3}, torch::kF32);
        tensor_image = tensor_image.permute({0,3,1,2});//[B, C, H, W]

        if (use_gpu_ == 1)
        {
            tensor_image = tensor_image.to(at::kCUDA);
        }
        else
        {
            tensor_image = tensor_image.to(at::kCPU);
        }
        //[0,1]
        vector<torch::IValue> batch;
        batch.push_back(tensor_image);
        //! get the result
        auto result = model.forward(batch);
        torch::Tensor disp_tensor = result.toTensor().squeeze();
        // std::cout << "inference size: " << disp_tensor.sizes() << "\n";
       
        disp_tensor = disp_tensor.to(at::kCPU);

        cv::Mat disp = cv::Mat::eye(height, width, CV_32FC1);
        std::memcpy((void *) disp.data, disp_tensor.data_ptr(), sizeof(float)*disp_tensor.numel());

        
        // This was left behind from the guy we forked this from.
        // Might be worth investigating to confirm whether this is needed or not.
        // All tests were done with this commented out.
        // //linear transform
        // double minVal;
        // double maxVal;
        // cv::minMaxLoc(disp, &minVal, &maxVal);
        // disp /= maxVal;
        // cv::resize(disp, disp, cv::Size(image.cols, image.rows));
        // disp*= 255;

        // disp.convertTo(disp, CV_8UC1);
        // cv::cvtColor(disp, disp, cv::COLOR_GRAY2BGR);
        
        return disp;
    }

    void MonoDepth::disp2Depth(cv::Mat &dispMap, cv::Mat &depthMap)
    {
        int type = dispMap.type();
        if (type == CV_8UC1){
            uchar* dispData = (uchar*)dispMap.data;
            ushort* depthData = (ushort*)depthMap.data;
            for (int i = 0; i < dispMap.rows; i++)
            {
                for (int j = 0; j < dispMap.cols; j++)
                {
                    int id = i*dispMap.cols + j;
                    if (!dispData[id])  continue;  //防止0除
                    depthData[id] = ushort( (float)1.f / ((float)dispData[id]) );
                }
            }
            cout<<depthMap<<"done"<<endl;
        }
        else if(type == CV_32FC1){
            float* dispData = (float*)dispMap.data;
            ushort* depthData = (ushort*)depthMap.data;
            for(int i = 0; i < dispMap.rows; i++){
                for(int j = 0; j < dispMap.cols; j++){
                    int id = (int) i*dispMap.cols + j;
                    if (!dispData[id])  continue;  //防止0除
                    depthData[id] = ushort( (float)1.f / ((float)dispData[id]));
                }
            }

        }
        else
        {
            cout << "please confirm dispImg's type!" << endl;
            cv::waitKey(0);
        }
    }
}
// namespace monodepth
