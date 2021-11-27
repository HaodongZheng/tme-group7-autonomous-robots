/*
 * Copyright (C) 2020 Ola Benderius
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

// Struct to hold the data
struct Data {
  opendlv::perception::cognition::NearFarPoints nearFarPoints{};
  opendlv::perception::KiwiBoundingBox kiwiBoundingBox{};
  std::mutex nearFarPointsMutex{};
  std::mutex kiwiBoundingBoxMutex{};
  float previousCrossProduct{};
  int32_t previousNearX{};
  int32_t previousNearY{};
  opendlv::proxy::GroundSteeringRequest previousGroundSteeringRequest{};
  opendlv::proxy::PedalPositionRequest previousPedalPositionRequest{};
};

// Main function
int32_t main(int32_t argc, char **argv) {
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if (0 == commandlineArguments.count("cid") 
      || 0 == commandlineArguments.count("freq")) {
    std::cerr << argv[0] << " The control program for the kiwi car" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --freq=10 " << std::endl;
    retCode = 1;
  } else {
    bool const VERBOSE{commandlineArguments.count("verbose") != 0};
    uint16_t const CID = std::stoi(commandlineArguments["cid"]);
    float const FREQ = std::stof(commandlineArguments["freq"]);
 
    Data data;
    cluon::OD4Session od4(CID);

    auto onNearFarPointsReading{[&data](cluon::data::Envelope &&envelope)
      {
        auto nearFarPointsReading = 
          cluon::extractMessage<opendlv::perception::cognition::NearFarPoints>(
              std::move(envelope));
        std::lock_guard<std::mutex> const lock(data.nearFarPointsMutex);          
        data.nearFarPoints = nearFarPointsReading;
      }};

    auto onKiwiBoundingBox{[&data](cluon::data::Envelope &&envelope)
      {
        auto kiwiBoundingBox = 
          cluon::extractMessage<opendlv::perception::KiwiBoundingBox>(
              std::move(envelope));
        std::lock_guard<std::mutex> const lock(data.kiwiBoundingBoxMutex);
        data.kiwiBoundingBox = kiwiBoundingBox;
      }};

    od4.dataTrigger(opendlv::perception::cognition::NearFarPoints::ID(), onNearFarPointsReading);
    od4.dataTrigger(opendlv::perception::KiwiBoundingBox::ID(), onKiwiBoundingBox);

    cluon::data::TimeStamp startTime = cluon::time::now();
    int64_t startTimeUs = cluon::time::toMicroseconds(startTime);
    
    // wait for the other microservices to start
    bool timeReached = false;
    while(!timeReached) {
      cluon::data::TimeStamp currentTime = cluon::time::now();
      int64_t currentTimeUs = cluon::time::toMicroseconds(currentTime);
      if (currentTimeUs - startTimeUs > 12000000) {
        timeReached = true;
      }
    }
  
    // control logic step
    auto atFrequency{[&VERBOSE, &data, &od4, startTimeUs]() -> bool
      {
        // you can use this as a timer
        // cluon::data::TimeStamp currentTime = cluon::time::now();
        // int64_t currentTimeUs = cluon::time::toMicroseconds(currentTime);
        // float timePassed = static_cast<float>(currentTimeUs - startTimeUs)/1000000;
        // if (timePassed > 60) {
        //   std::cout << "One minute passed." << std::endl;
        // }

        // read the data
        opendlv::perception::cognition::NearFarPoints nfPointsReading;
        opendlv::perception::KiwiBoundingBox kiwiBoundingBox;
        {
          std::lock_guard<std::mutex> lock1(data.nearFarPointsMutex);
          std::lock_guard<std::mutex> lock2(data.kiwiBoundingBoxMutex);
          
          nfPointsReading = data.nearFarPoints;
          kiwiBoundingBox = data.kiwiBoundingBox;
        }

        int32_t previousNearX = data.previousNearX;
        int32_t previousNearY = data.previousNearY;
        int32_t nearX = nfPointsReading.nearX();
        int32_t nearY = nfPointsReading.nearY();
        int32_t farX = nfPointsReading.farX();
        int32_t farY = nfPointsReading.farY();
        bool reachCrossRoad = nfPointsReading.reachCrossRoad();

        // use a simple KF to estimate the near point
        {
          float ks = 600.0f;  // vehicle speed to displacement vector length
          float ka = 1.0f;  // steering angle to displacement vector angle
          float motionVectorLength = ks * data.previousPedalPositionRequest.position();
          float motionVectorAngle = ka * data.previousGroundSteeringRequest.groundSteering();
          int32_t predictedNearX = previousNearX - static_cast<int32_t>(motionVectorLength*cos(motionVectorAngle));
          int32_t predictedNearY = previousNearY - static_cast<int32_t>(motionVectorLength*sin(motionVectorAngle));

          float g = 0.65f;
          nearX = static_cast<int32_t>(g * nearX + (1.0f-g) * predictedNearX);
          nearY = static_cast<int32_t>(g * nearY + (1.0f-g) * predictedNearY);

          data.previousNearX = nearX;
          data.previousNearY = nearY;
        }

        // controller parameters
        float kp = 0.20f;
        float kd = 0.05f;

        float desiredVectorX = (farX + 2*nearX)/2;
        float desiredVectorY = (farY + 2*nearY)/2;
        float desiredVectorLength = std::sqrt(desiredVectorX*desiredVectorX + desiredVectorY*desiredVectorY);

        // deal with the NaN value when desiredVectorLength == 0
        if (desiredVectorLength < 0.01f) {
            desiredVectorLength = 1.0f;
        }

        float crossProductZ =  1.0f * desiredVectorY/desiredVectorLength - 0.0f * desiredVectorX/desiredVectorLength;
        float dotProductZ =  1.0f * desiredVectorX/desiredVectorLength - 0.0f * desiredVectorY/desiredVectorLength;

        float pedalPosition = 0.2f;
        float groundSteeringAngle = 0.0f;

        // lateral control
        float previousCrossProduct = data.previousCrossProduct;
        if (farX == 0 && nearX == 0) {
          groundSteeringAngle = 0.0f;
        } else if (dotProductZ < 0.0f) {
          groundSteeringAngle = kp * ((crossProductZ<0.0f)?-1.0f:1.0f) + kd*(crossProductZ-previousCrossProduct);
        } else {
          groundSteeringAngle = kp*crossProductZ + kd*(crossProductZ-previousCrossProduct);
        }
        data.previousCrossProduct = crossProductZ;

        // slow down when a big turn is required
        pedalPosition = 0.10f*(1.0f - ((groundSteeringAngle<0)?-1.0f:1.0f)*(groundSteeringAngle));

        // longitudinal control (slow down behind Kiwis and at crossings)
        if (kiwiBoundingBox.nBox() > 0) {
          uint32_t boxX = kiwiBoundingBox.x();
          uint32_t boxY = kiwiBoundingBox.y();
          uint32_t boxW = kiwiBoundingBox.w();
          uint32_t boxH = kiwiBoundingBox.h();
          float boxSize = static_cast<float>(boxW * boxH);
          uint32_t imgW = kiwiBoundingBox.imageWidth();
          uint32_t imgH = kiwiBoundingBox.imageHeight();
          float imgSize =  static_cast<float>(imgW*imgH);
          float maxKiwiSizeAllowed = imgSize/10;
          if (boxSize > imgSize/100 && fabs(crossProductZ) < 0.15) {
             pedalPosition = 0.2f*(1.0f - boxSize/maxKiwiSizeAllowed);
             std::cout << "kiwi speed control activated" << std::endl;
             //if (boxSize > maxKiwiSizeAllowed) {
             //  pedalPosition = 0.0f;
             //}
          }
          if (reachCrossRoad && pedalPosition>0.04f) {
            pedalPosition = 0.04f;
          }
          if (boxY!=imgH-1 && reachCrossRoad && boxX+boxW > imgW/2-1 && boxSize > imgSize/20) {
            pedalPosition = 0.0f;
          } 
         }

        // send the calculated control input
        opendlv::proxy::GroundSteeringRequest groundSteeringRequest;
        groundSteeringRequest.groundSteering(groundSteeringAngle);

        opendlv::proxy::PedalPositionRequest pedalPositionRequest;
        pedalPositionRequest.position(pedalPosition);

        cluon::data::TimeStamp sampleTime = cluon::time::now();
        od4.send(groundSteeringRequest, sampleTime, 0);
        od4.send(pedalPositionRequest, sampleTime, 0);

        if (VERBOSE) {
          std::cout << "Ground steering is " << groundSteeringAngle
            << " and pedal position is " << pedalPosition << std::endl;
        }

        data.previousGroundSteeringRequest = groundSteeringRequest;
        data.previousPedalPositionRequest = pedalPositionRequest;

        return true;

      }};

    od4.timeTrigger(FREQ, atFrequency);
  }
  return retCode;
}
