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

#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

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

            // Handler to receive distance readings (realized as C++ lambda).
            std::mutex distancesMutex;
            std::mutex kiwiBoundingBoxMutex;
            float front{0};
            float rear{0};
            float left{0};
            float right{0};
            uint32_t boxX{0};
            uint32_t boxY{0};
            uint32_t boxW{0};
            uint32_t boxH{0};
            
            auto onDistance = [&distancesMutex, &front, &rear, &left, &right](cluon::data::Envelope &&env){
                auto senderStamp = env.senderStamp();
                // Now, we unpack the cluon::data::Envelope to get the desired DistanceReading.
                opendlv::proxy::DistanceReading dr = cluon::extractMessage<opendlv::proxy::DistanceReading>(std::move(env));

                // Store distance readings.
                std::lock_guard<std::mutex> lck(distancesMutex);
                switch (senderStamp) {
                    case 0: front = dr.distance(); break;
                    case 2: rear = dr.distance(); break;
                    case 1: left = dr.distance(); break;
                    case 3: right = dr.distance(); break;
                }
            };

            auto onKiwiBoundingBox = [&kiwiBoundingBoxMutex, &boxX, &boxY, &boxW, &boxH](cluon::data::Envelope &&env){
                auto senderStamp = env.senderStamp();
                // Now, we unpack the cluon::data::Envelope to get the desired DistanceReading.
                opendlv::perception::KiwiBoundingBox kiwiBoundingBox = cluon::extractMessage<opendlv::perception::KiwiBoundingBox>(std::move(env));

                // Store distance readings.
                std::lock_guard<std::mutex> lck(kiwiBoundingBoxMutex);
                if (senderStamp == 0) {
                  boxX = kiwiBoundingBox.x();
                  boxY = kiwiBoundingBox.y();
                  boxW = kiwiBoundingBox.w();
                  boxH = kiwiBoundingBox.h();
                }
            };
            // Finally, we register our lambda for the message identifier for opendlv::proxy::DistanceReading.
            od4.dataTrigger(opendlv::proxy::DistanceReading::ID(), onDistance);
            od4.dataTrigger(opendlv::perception::KiwiBoundingBox::ID(), onKiwiBoundingBox);
            cv::Point previousNearPoint(WIDTH/2-1,HEIGHT/2-1);

            // Endless loop; end the program by pressing Ctrl-C.
            while (od4.isRunning()) {
                cv::Mat img;

                // Wait for a notification of a new frame.
                sharedMemory->wait();

                // Lock the shared memory.
                sharedMemory->lock();
                {
                    // Copy image into cvMat structure.
                    // Be aware of that any code between lock/unlock is blocking
                    // the camera to provide the next frame. Thus, any
                    // computationally heavy algorithms should be placed outside
                    // lock/unlock
                    cv::Mat wrapped(HEIGHT, WIDTH, CV_8UC4, sharedMemory->data());
                    img = wrapped.clone();
                }
                sharedMemory->unlock();

                // TODO: Do something with the frame.
                img = img(cv::Rect(0,HEIGHT/2-1,WIDTH, HEIGHT/2));
                cv::line(img, cv::Point(0,39), cv::Point(WIDTH-1,39), cv::Scalar(255, 255, 0), 2, cv::LINE_AA);  
                               

                cv::Mat hsv;
                cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
                // calculate the mean value.
                cv::Scalar meanHSVLeft =cv::mean(hsv(cv::Rect(0,0,WIDTH/2, HEIGHT/2)));
                cv::Scalar meanHSVRight =cv::mean(hsv(cv::Rect(WIDTH/2-1,0,WIDTH/2, HEIGHT/2)));
                //seting uninterested region to black 
                hsv(cv::Rect(WIDTH/4-1,3*HEIGHT/8-1,WIDTH/2, HEIGHT/8)) = cv::Scalar(0,0,0);
                hsv(cv::Rect(0,0,WIDTH, 40)) = cv::Scalar(0,0,0);

                int32_t NewboxY = static_cast<int32_t>(boxY) - static_cast<int32_t>(HEIGHT/2);
                int32_t NewboxH = NewboxY + static_cast<int32_t>(boxH);
                if (NewboxY < 0) {
                  boxY = 0;
                } else {
                  boxY = static_cast<uint32_t> (NewboxY);
                }
                if (NewboxH < 0) {
                  boxH = 0;
                } else {
                   boxH = static_cast<uint32_t> (NewboxH);
                }
                hsv(cv::Rect(static_cast<uint32_t>(boxX+0.25*boxW),boxY,static_cast<uint32_t>(0.5*boxW), 
                    static_cast<uint32_t>(0.7*boxH)))= cv::Scalar(0,0,0);

                cv::Scalar blueHsvLow(110, static_cast<uint32_t>(101+ 1.0*(meanHSVRight[1] - 45)), 20);
                cv::Scalar blueHsvHi(130, 255, 150);

                cv::Scalar yellowHsvLow(10, static_cast<uint32_t>(70+ 1.0*(meanHSVLeft[1] - 45)), 100);
                cv::Scalar yellowHsvHi(40, 255, 255);
                

                cv::Scalar redHsvLow(156,120, 70);
                cv::Scalar redHsvHi(180, 255, 255);
                cv::Mat blueCones;
                cv::Mat yellowCones;
                cv::Mat redCones;
                cv::inRange(hsv, blueHsvLow, blueHsvHi, blueCones);
                cv::inRange(hsv, yellowHsvLow, yellowHsvHi, yellowCones);
                cv::inRange(hsv, redHsvLow, redHsvHi, redCones);

                cv::Mat blueDilate;
                uint32_t iterations{4};
                cv::dilate(blueCones, blueDilate, cv::Mat(), cv::Point(-1, -1), iterations, 1, 1);

                cv::Mat yellowDilate;
                cv::dilate(yellowCones, yellowDilate, cv::Mat(), cv::Point(-1, -1), iterations, 1, 1);

                cv::Mat redDilate;
                cv::dilate(redCones, redDilate, cv::Mat(), cv::Point(-1, -1), iterations, 1, 1);

                cv::Mat blueErode;
                cv::erode(blueDilate, blueErode, cv::Mat(), cv::Point(-1, -1), iterations, 1, 1);

                cv::Mat yellowErode;
                cv::erode(yellowDilate, yellowErode, cv::Mat(), cv::Point(-1, -1), iterations, 1, 1); 

                cv::Mat redErode;
                cv::erode(redDilate, redErode, cv::Mat(), cv::Point(-1, -1), iterations, 1, 1); 

                cv::Mat buleCanny;
                cv::Canny(blueErode,buleCanny,30,90,3);
                std::vector<std::vector<cv::Point>> blueContours;
                std::vector<cv::Vec4i> hierarchy;
                cv::findContours(buleCanny, blueContours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cv::Point(0,0));                

                cv::Mat yellowCanny;
                cv::Canny(yellowErode,yellowCanny,30,90,3);
                std::vector<std::vector<cv::Point>> yellowContours;
                cv::findContours(yellowCanny, yellowContours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cv::Point(0,0));  

                cv::Mat redCanny;
                cv::Canny(redErode,redCanny,30,90,3);
                std::vector<std::vector<cv::Point>> redContours;
                cv::findContours(redCanny, redContours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cv::Point(0,0));  

                std::vector<std::vector<cv::Point>> blueApproxContours(blueContours.size());
                std::vector<std::vector<cv::Point>> yellowApproxContours(yellowContours.size());
                std::vector<std::vector<cv::Point>> redApproxContours(redContours.size());

                    
                for (size_t k = 0; k < blueContours.size(); k++)
                { 
                  cv::approxPolyDP(cv::Mat(blueContours[k]),blueApproxContours[k], 3, true);                
                }

                for (size_t k = 0; k < yellowContours.size(); k++)
                { 
                  cv::approxPolyDP(cv::Mat(yellowContours[k]),yellowApproxContours[k], 3, true);                 
                }

                for (size_t k = 0; k < redContours.size(); k++)
                { 
                  cv::approxPolyDP(cv::Mat(redContours[k]),redApproxContours[k], 3, true);                
                }

                int bubbleSortCounter;
                size_t  hIndex = 0; 
                cv::Point swap;
                cv::Rect swapBounding;                
                int overlapTolerance = 25;
                std::vector<std::vector<cv::Point>> redHull(redApproxContours.size());
                std::vector<cv::Point> redTrack(40);
                std::vector<cv::Rect> boundingRedCones(redApproxContours.size());
                for (size_t i = 0; i < redApproxContours.size(); i++)
                { 
                  size_t nPoints = redApproxContours[i].size();
                  if (nPoints <= 30 && nPoints >= 3) {
                    cv::convexHull(cv::Mat(redApproxContours[i]), redHull[hIndex], false);
                    cv::Point leftMostPoint(WIDTH,0);
                    cv::Point rightMostPoint(0,0);
                    cv::Point topPoint(0,HEIGHT);
                    cv::Point bottomPoint(0,0);
                    for(size_t j = 0; j < redHull[hIndex].size(); j++)
                    {
                      cv::Point point = redHull[hIndex][j];
                      if(point.x < leftMostPoint.x) {
                        leftMostPoint =  point;
                      }     
                      if(point.x > rightMostPoint.x) {
                        rightMostPoint =  point;
                      }     
                      if(point.y < topPoint.y) {
                        topPoint =  point;
                      }     
                      if(point.y > bottomPoint.y) {
                        bottomPoint =  point;
                      }     
                    }
                    float height = bottomPoint.y - topPoint.y;
                    float width = rightMostPoint.x  - leftMostPoint.x;
                    float yMid = (bottomPoint.y + topPoint.y)/2;
                    float xMid = (rightMostPoint.x  + leftMostPoint.x)/2;
                    float area = width*height;
                    //Find the right object based on shape and calculate the center of mass.
                    if (width/height < 0.8 && width/height > 0.15) {
                      if (area > 200 && area < WIDTH*HEIGHT/20 && rightMostPoint.y> yMid && leftMostPoint.y> yMid) {
                        redTrack[hIndex].x = static_cast<int> (std::round(xMid));
                        redTrack[hIndex].y = static_cast<int> (std::round(yMid)); 
                        boundingRedCones[hIndex] = cv::boundingRect(redHull[hIndex]);
                        cv::rectangle(img,boundingRedCones[hIndex].tl(), boundingRedCones[hIndex].br(),cv::Scalar(0,0,255),2);                 
                        ++hIndex;
                      } else {
                        redHull.erase(redHull.begin() + hIndex);
                        boundingRedCones.erase(boundingRedCones.begin() + hIndex);
                      }
                    } else {
                      redHull.erase(redHull.begin() + hIndex);
                      boundingRedCones.erase(boundingRedCones.begin() + hIndex);
                    }
                  } else { 
                    redHull.pop_back();
                    boundingRedCones.pop_back();
                  } 
                }
                // Now we obtain all the center of mass in blue track. The next step is to remove the leftbottom quarter and overlapped points.
                redTrack.erase(redTrack.begin() + redHull.size(),redTrack.begin() + redTrack.size()-1);   

                for (size_t index = 0; index < redTrack.size(); index ++)
                { 
                    if (abs(redTrack[index].x - redTrack[index+1].x) < overlapTolerance && 
                        abs(redTrack[index].y - redTrack[index+1].y) < overlapTolerance) {
                        redTrack.erase(redTrack.begin() + index + 1);
                  }
                }
                
                bool bubbleSortRedComplete = false;
                if (redTrack.size() > 1) {
                  while (!bubbleSortRedComplete) {
                    bubbleSortCounter = 0;
                    for (size_t index = 0; index < redTrack.size() - 1; index ++)
                    { 
                      if (redTrack[index].y < redTrack[index+1].y ) {
                        swap = redTrack[index];
                        redTrack[index] = redTrack[index + 1];
                        redTrack[index + 1] = swap;
                        bubbleSortCounter = bubbleSortCounter + 1;
                      }
                    }
                    if (bubbleSortCounter == 0) {
                        bubbleSortRedComplete = true;
                        break;
                    }
                  }
                }
                
                uint32_t meanX = 0;
                uint32_t meanY = 0;
                int32_t paramThreshold = - static_cast<int32_t>(WIDTH/6*WIDTH/6);
                bool findRedConeMatch = false;

                for (size_t index = 0; index < redTrack.size(); index ++)
                { 
                  if (index < redTrack.size() - 1) {  
                    int32_t param = (static_cast<int32_t>(redTrack[index].x) - static_cast<int32_t>(WIDTH/2-1)) * 
                                    (static_cast<int32_t>(redTrack[index + 1].x) - static_cast<int32_t>(WIDTH/2-1));  
                    int32_t  yDistance = abs(static_cast<int32_t>(redTrack[index].y) - static_cast<int32_t>(redTrack[index + 1].y));
                    if (param < paramThreshold && yDistance <= 70 )  {
                        cv::line(img, redTrack[index], redTrack[index +1], cv::Scalar(255, 255, 255), 2, cv::LINE_AA); 
                        meanX = (redTrack[index].x + redTrack[index+1].x)/2;
                        meanY = (redTrack[index].y + redTrack[index+1].y)/2;
                        findRedConeMatch = true;
                        cv::circle(img, cv::Point(meanX,meanY), 3, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA); 
                        break;  
                    }              
                  }
                }
                  
                uint32_t  maxYRed = redTrack[0].y;

                bool reachCrossRoad = false;   
                if (redTrack.size() > 1) {
                   reachCrossRoad = true;
                }
               
                
                std::vector<std::vector<cv::Point>> blueHull(blueApproxContours.size());
                std::vector<cv::Rect> boundingBlueCones(blueApproxContours.size());
                hIndex = 0; 
                std::vector<cv::Point> blueTrack(40);

                //std::vector<cv::Point> eTri(3);
                for(size_t i = 0; i < blueApproxContours.size(); i++)
                { 
                  size_t nPoints = blueApproxContours[i].size();
                  if (nPoints <= 30 && nPoints >= 3) {
                    cv::convexHull(cv::Mat(blueApproxContours[i]), blueHull[hIndex], false);
                    cv::Point leftMostPoint(WIDTH,0);
                    cv::Point rightMostPoint(0,0);
                    cv::Point topPoint(0,HEIGHT);
                    cv::Point bottomPoint(0,0);
   
                    for (size_t j = 0; j < blueHull[hIndex].size(); j++)
                    {
                      cv::Point point = blueHull[hIndex][j];
                      if (point.x < leftMostPoint.x) {
                        leftMostPoint =  point;
                      }     
                      if (point.x > rightMostPoint.x) {
                        rightMostPoint =  point;
                      }     
                      if (point.y < topPoint.y) {
                        topPoint =  point;
                      }     
                      if (point.y > bottomPoint.y) {
                        bottomPoint =  point;
                      }     
                    }
                    float height = bottomPoint.y - topPoint.y;
                    float width = rightMostPoint.x  - leftMostPoint.x;
                    float yMid = (bottomPoint.y + topPoint.y)/2;
                    float xMid = (rightMostPoint.x  + leftMostPoint.x)/2;
                    float area = width*height;

                    //Find the right object based on shape and calculate the center of mass.
                    if (width/height < 0.8  &&  width/height >= 0.15) {
                      if ((yMid< HEIGHT/4 || xMid > WIDTH/2) && area > 200 && area < WIDTH*HEIGHT/20 && yMid > maxYRed ) {
                        blueTrack[hIndex].x = static_cast<int> (std::round(xMid));
                        blueTrack[hIndex].y = static_cast<int> (std::round(yMid));    
                        boundingBlueCones[hIndex] = cv::boundingRect(blueHull[hIndex]);
                        cv::rectangle(img,boundingBlueCones[hIndex].tl(), boundingBlueCones[hIndex].br(),cv::Scalar(255,0,0),2);           
                        ++hIndex;
                      } else {
                        blueHull.erase(blueHull.begin() + hIndex);
                        boundingBlueCones.erase(boundingBlueCones.begin() + hIndex);
                      }
                    } else {
                      blueHull.erase(blueHull.begin() + hIndex);
                      boundingBlueCones.erase(boundingBlueCones.begin() + hIndex);
                    }
                  } else { 
                    blueHull.pop_back();
                    boundingBlueCones.pop_back();
                  } 
                }
                // Now we obtain all the center of mass in blue track. The next step is to remove the overlapped points.
                blueTrack.erase(blueTrack.begin() + blueHull.size(),blueTrack.begin() + blueTrack.size()-1);   

                for(size_t index = 0; index < blueTrack.size(); index ++)
                { 
                  if (abs(blueTrack[index].x - blueTrack[index+1].x) < overlapTolerance && 
                      abs(blueTrack[index].y - blueTrack[index+1].y) < overlapTolerance) {
                    blueTrack.erase(blueTrack.begin() + index + 1);
                  }
                }
                
                bool bubbleSortBlueComplete = false;

                if (blueTrack.size() > 1) {
                  while (!bubbleSortBlueComplete) {
                    bubbleSortCounter = 0;
                    for(size_t index = 0; index < blueTrack.size() - 1; index ++)
                    { 
                      if(blueTrack[index].y < blueTrack[index+1].y ) {
                        swap = blueTrack[index];
                        blueTrack[index] = blueTrack[index + 1];
                        blueTrack[index + 1] = swap;
                        bubbleSortCounter = bubbleSortCounter + 1;
                      }

                    }
                    if (bubbleSortCounter == 0) {
                      bubbleSortBlueComplete = true;
                      break;
                    }
                  }
                }


                for(size_t index = 0; index < blueTrack.size(); index ++)
                { 
                  if(index < blueTrack.size() - 1 ) {                        
                    cv::line(img, blueTrack[index], blueTrack[index +1], cv::Scalar(0, 255, 0), 2, cv::LINE_AA);    
                  }
                }

                std::vector<std::vector<cv::Point>> yellowHull(yellowApproxContours.size());
                hIndex = 0; 
                std::vector<cv::Point> yellowTrack(40);
                std::vector<cv::Rect> boundingYellowCones(yellowApproxContours.size());
                for (size_t i = 0; i < yellowApproxContours.size(); i++)
                { 
                  size_t nPoints = yellowApproxContours[i].size();
                  if (nPoints <= 30 && nPoints >= 3) {
                    cv::convexHull(cv::Mat(yellowApproxContours[i]), yellowHull[hIndex], false);
                    cv::Point leftMostPoint(WIDTH,0);
                    cv::Point rightMostPoint(0,0);
                    cv::Point topPoint(0,HEIGHT);
                    cv::Point bottomPoint(0,0);
                    for(size_t j = 0; j < yellowHull[hIndex].size(); j++)
                    {
                      cv::Point point = yellowHull[hIndex][j];
                      if(point.x < leftMostPoint.x) {
                        leftMostPoint =  point;
                      }     
                      if(point.x > rightMostPoint.x) {
                        rightMostPoint =  point;
                      }     
                      if(point.y < topPoint.y) {
                        topPoint =  point;
                      }     
                      if(point.y > bottomPoint.y) {
                        bottomPoint =  point;
                      }     
                    }
                    float height = bottomPoint.y - topPoint.y;
                    float width = rightMostPoint.x  - leftMostPoint.x;
                    float yMid = (bottomPoint.y + topPoint.y)/2;
                    float xMid = (rightMostPoint.x  + leftMostPoint.x)/2;
                    float area = width*height;

                    //Find the right object based on shape and calculate the center of mass.
                    if (width/height < 0.8 && width/height > 0.15) {
                      if ((yMid< HEIGHT/4 || xMid < WIDTH/2) && area > 200 && area < WIDTH*HEIGHT/20  && yMid > maxYRed ) {
                        yellowTrack[hIndex].x = static_cast<int> (std::round(xMid));
                        yellowTrack[hIndex].y = static_cast<int> (std::round(yMid));  
                        boundingYellowCones[hIndex] = cv::boundingRect(yellowHull[hIndex]);
                        cv::rectangle(img,boundingYellowCones[hIndex].tl(), boundingYellowCones[hIndex].br(),cv::Scalar(0,255,255),2);                
                        ++hIndex;
                      } else {
                        yellowHull.erase(yellowHull.begin() + hIndex);
                        boundingYellowCones.erase(boundingYellowCones.begin() + hIndex);
                      }
                    } else {
                      yellowHull.erase(yellowHull.begin() + hIndex);
                      boundingYellowCones.erase(boundingYellowCones.begin() + hIndex);
                    }
                  } else { 
                    yellowHull.pop_back();
                    boundingYellowCones.pop_back();
                  } 
                }
                // Now we obtain all the center of mass in blue track. The next step is to remove overlapped points.
                yellowTrack.erase(yellowTrack.begin() + yellowHull.size(),yellowTrack.begin() + yellowTrack.size()-1);   

                for(size_t index = 0; index < yellowTrack.size(); index ++)
                { 
                  if (abs(yellowTrack[index].x - yellowTrack[index+1].x) < overlapTolerance &&
                      abs(yellowTrack[index].y - yellowTrack[index+1].y) < overlapTolerance) {
                    yellowTrack.erase(yellowTrack.begin() + index + 1);
                  }
                }
                

                bool bubbleSortYellowComplete = false;
                if (yellowTrack.size() > 1) {
                  while (!bubbleSortYellowComplete) {
                    bubbleSortCounter = 0;
                    for(size_t index = 0; index < yellowTrack.size() - 1; index ++)
                    { 
                      if(yellowTrack[index].y < yellowTrack[index+1].y ) {
                        swap = yellowTrack[index];
                        yellowTrack[index] = yellowTrack[index + 1];
                        yellowTrack[index + 1] = swap;
                        bubbleSortCounter = bubbleSortCounter + 1;
                      }
                    }
                    if (bubbleSortCounter == 0) {
                      bubbleSortYellowComplete = true;
                      break;
                    }
                  }
                }
                
                for (size_t index = 0; index < yellowTrack.size(); index ++)
                { 
                  if (index < yellowTrack.size() - 1) {                       
                    cv::line(img, yellowTrack[index], yellowTrack[index +1], cv::Scalar(0, 255, 0), 2, cv::LINE_AA);    
                  }
                }

                size_t size = std::max(yellowTrack.size() , blueTrack.size());
                size_t nPair = std::min(yellowTrack.size(), blueTrack.size());
                std::vector<cv::Point> realTrack(size);
          
                if (nPair == 0 && yellowTrack.size() > blueTrack.size()) {
                  cv::Point blue(WIDTH-51,HEIGHT/2-51);
                  blueTrack.push_back(blue);
                  nPair = 1;
                } else if (nPair == 0 && blueTrack.size() > yellowTrack.size())  {
                  cv::Point yellow(50,HEIGHT/2-51);
                  yellowTrack.push_back(yellow);
                  nPair = 1;
                }

                for (size_t index = 0; index < size; index ++)
                {                   
                   if (index < nPair) {
                     realTrack[index].x = (blueTrack[index].x + yellowTrack[index].x)/2;               
                     realTrack[index].y = (blueTrack[index].y + yellowTrack[index].y)/2;  
                     cv::line(img, yellowTrack[index] , blueTrack[index] , cv::Scalar(255, 255, 255), 4, cv::LINE_AA);   
                     cv::circle(img, realTrack[index], 5, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA); 
                   } else if (yellowTrack.size() > blueTrack.size() && yellowTrack.size() > 1 ) {
                     if (index < yellowTrack.size()  && nPair != 0) {
                         realTrack[index].x = (blueTrack[blueTrack.size()-1].x + yellowTrack[index].x)/2;               
                         realTrack[index].y = (blueTrack[blueTrack.size()-1].y + yellowTrack[index].y)/2;
                         cv::line(img, yellowTrack[index] , blueTrack[blueTrack.size()-1] , cv::Scalar(255, 255, 255), 4, cv::LINE_AA);
                         cv::circle(img, realTrack[index], 5, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA);     
                     }
                   } else if (blueTrack.size() > 1 && index < blueTrack.size()) {
                     if (index < blueTrack.size() && nPair != 0) {
                        realTrack[index].x = (blueTrack[index].x + yellowTrack[yellowTrack.size()-1].x)/2;               
                        realTrack[index].y = (blueTrack[index].y + yellowTrack[yellowTrack.size()-1].y)/2;
                        cv::line(img, yellowTrack[yellowTrack.size()-1] , blueTrack[index] , cv::Scalar(255, 255, 255), 4, cv::LINE_AA);
                        cv::circle(img, realTrack[index], 5, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA);       
                     }
                   }  
                }

                if (findRedConeMatch) {
                   realTrack.push_back(cv::Point(meanX,meanY));
                }

                if (realTrack.size() > 0) {
                  float horizontalMovement = static_cast<float>(fabs(static_cast<double>(previousNearPoint.x) - static_cast<double>(realTrack[0].x)));
                  if (horizontalMovement > static_cast<float> (WIDTH/25)) { 
                    uint32_t newX = (realTrack[0].x + previousNearPoint.x)/2;
                    uint32_t newY = (realTrack[0].y + previousNearPoint.y)/2;
                    realTrack[0].x = newX;
                    realTrack[0].y = newY;
                    cv::circle(img, realTrack[0], 5, cv::Scalar(0, 255, 255), cv::FILLED, cv::LINE_AA);  
                  }
                  previousNearPoint = realTrack[0];
                }

                if (VERBOSE) {
                    cv::imshow("Cone detection", img);
                    cv::waitKey(1);
                }

                int nx;
                int ny;
                int fx;
                int fy;
                if  (realTrack.size() != 0) { 
                  cv::Point nearPoint = realTrack[0];
                  cv::Point farPoint = realTrack[realTrack.size()-1];
                  ny = -(nearPoint.x-WIDTH/2+1);
                  nx = (HEIGHT/2-1-nearPoint.y);
                  fy = -(farPoint.x - WIDTH/2+1);
                  fx = (HEIGHT/2-1-farPoint.y);       
                  if (realTrack.size() > 1) {
                    fy = ny;
                    fx = nx;
                  }          
                } else { 
                  nx = 0;
                  ny = 0;
                  fx = 0;
                  fy = 0;
                }
                cluon::data::TimeStamp sampleTime = cluon::time::now();
                opendlv::perception::cognition::NearFarPoints nfPoints;                
                nfPoints.nearX(nx);
                nfPoints.nearY(ny);
                nfPoints.farX(fx);
                nfPoints.farY(fy);
                nfPoints.reachCrossRoad(reachCrossRoad);
                od4.send(nfPoints, sampleTime, 0);
            }
        }
        retCode = 0;
    }
    return retCode;
}
