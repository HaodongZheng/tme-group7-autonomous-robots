/*
 * Copyright (C) 2018  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/dnn/dnn.hpp>

int32_t main(int32_t argc, char **argv) {
  int32_t retCode{1};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ( (0 == commandlineArguments.count("cid")) ||
       (0 == commandlineArguments.count("name")) ||
       (0 == commandlineArguments.count("width")) ||
       (0 == commandlineArguments.count("height")) ) {
    std::cerr << argv[0] << " attaches to a shared memory area containing an ARGB image." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid=<OD4 session> --name=<name of shared memory area> [--verbose]" << std::endl;
    std::cerr << "         --cid:    CID of the OD4Session to send and receive messages" << std::endl;
    std::cerr << "         --name:   name of the shared memory area to attach" << std::endl;
    std::cerr << "         --width:  width of the frame" << std::endl;
    std::cerr << "         --height: height of the frame" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=112 --name=img.argb --width=640 --height=480 --verbose" << std::endl;
  } 
  else {
    const std::string NAME{commandlineArguments["name"]};
    const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
    const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
    const bool VERBOSE{commandlineArguments.count("verbose") != 0};

    // Attach to the shared memory.
    std::unique_ptr<cluon::SharedMemory> sharedMemory{new cluon::SharedMemory{NAME}};
    if (sharedMemory && sharedMemory->valid()) {
      std::clog << argv[0] << ": Attached to shared memory '" << sharedMemory->name() << " (" << sharedMemory->size() << " bytes)." << std::endl;

      // Interface to a running OpenDaVINCI session; here, you can send and receive messages.
      cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

      // Initialize the parameters.
      float confThreshold = 0.3f;  // Confidence threshold
      float nmsThreshold = 0.4f;  // Non-maximum suppression threshold
      uint32_t inpWidth = 320;  // Width of network's input image (320-faster, 608-more accurate)
      uint32_t inpHeight = 320;  // Height of network's input image (320-faster, 608-more accurate)
      cv::Size inpSize(inpWidth, inpHeight);

      std::vector<std::string> classes;
      classes.push_back("Kiwi");
      
      // Load the network.
      std::string modelConfiguration = "/opt/yolo/yolo-obj.cfg";
      std::string modelWeights = "/opt/yolo/yolo-obj.weights";
      cv::dnn::Net net = cv::dnn::readNetFromDarknet(modelConfiguration, modelWeights);
      net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
      net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
      std::vector<cv::String> outNames = net.getUnconnectedOutLayersNames();

      // Endless loop; end the program by pressing Ctrl-C.
      while (od4.isRunning()) {
        cv::Mat imga,img;

        // Wait for a notification of a new frame.
        sharedMemory->wait();

        // Copy the image from memory.
        sharedMemory->lock();
        {
          cv::Mat wrapped(HEIGHT, WIDTH, CV_8UC4, sharedMemory->data());
          imga = wrapped.clone();
        }
        sharedMemory->unlock();

        // Remove the alpha channel (the network expects 3 channels).
        cv::cvtColor(imga, img, cv::COLOR_RGBA2RGB);
        cv::Mat blob;
        cv::dnn::blobFromImage(img, blob, 1.0, inpSize, cv::Scalar(), false, false, CV_8U);

        // Run the detection.
        net.setInput(blob, "", 1.0f/255.0f, cv::Scalar(0,0,0));
        std::vector<cv::Mat> outs;
        net.forward(outs, outNames);

        // Process the result.
        std::vector<uint32_t> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;      
        for (size_t i = 0; i < outs.size(); ++i) {
          float* data = (float*)outs[i].data;
          for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols) {
            cv::Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
            cv::Point classIdPoint;
            double confidence;
            cv::minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
            if (confidence > confThreshold) {
              uint32_t centerX = (uint32_t)(data[0] * img.cols);
              uint32_t centerY = (uint32_t)(data[1] * img.rows);
              uint32_t width = (uint32_t)(data[2] * img.cols);
              uint32_t height = (uint32_t)(data[3] * img.rows);
              uint32_t left = centerX - width / 2;
              uint32_t top = centerY - height / 2;
              classIds.push_back(classIdPoint.x);
              confidences.push_back((float)confidence);
              boxes.push_back(cv::Rect(left, top, width, height));
            }
          }
        }
        // Perform non maximum suppression to eliminate redundant boxes.
        std::vector<int32_t> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

        // Display the detections.
        if (VERBOSE) {
          for (size_t i = 0; i < indices.size(); ++i) {
            uint32_t idx = indices[i];
            cv::Rect box = boxes[idx];
            cv::rectangle(img, cv::Point(box.x, box.y), cv::Point(box.x + box.width, box.y + box.height), 
                          cv::Scalar(0, 0, 255), 2);
          }
          // Display performance information.
          std::vector<double> layersTimes;
          double freq = cv::getTickFrequency() / 1000;
          double t = net.getPerfProfile(layersTimes) / freq;
          std::string label = cv::format("Inference time for a frame : %.2f ms", t);
          cv::putText(img, label, cv::Point(0, 15), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255));

          cv::imshow("Kiwi detection", img);
          cv::waitKey(1);
<<<<<<< HEAD
        }

=======
        } 

        // send out the detection(s)
>>>>>>> 8dad46f475c6e434051cd4f6f534a2460f016110
        opendlv::perception::KiwiBoundingBox kiwi;
        kiwi.imageWidth(WIDTH);
        kiwi.imageHeight(HEIGHT);
        kiwi.nBox(indices.size());

        if (indices.size() == 0) {
          kiwi.x(0);
          kiwi.y(0);
          kiwi.w(0);
          kiwi.h(0);
<<<<<<< HEAD

=======
>>>>>>> 8dad46f475c6e434051cd4f6f534a2460f016110
          cluon::data::TimeStamp sampleTime;
          od4.send(kiwi, sampleTime, 0);
        } else {
          for (size_t i = 0; i < indices.size(); i++) {
            uint32_t idx = indices[i];
            cv::Rect box = boxes[idx];
            kiwi.x(box.x);
            kiwi.y(box.y);
            kiwi.w(box.width);
            kiwi.h(box.height);  
            cluon::data::TimeStamp sampleTime;
            od4.send(kiwi, sampleTime, 0);
<<<<<<< HEAD

=======
>>>>>>> 8dad46f475c6e434051cd4f6f534a2460f016110
          }
        }
      }
    }
    retCode = 0;
  }
  return retCode;
}
