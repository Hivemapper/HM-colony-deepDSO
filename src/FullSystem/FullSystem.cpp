/**
 * This file is part of DSO.
 *
 * Copyright 2016 Technical University of Munich and Intel.
 * Developed by Jakob Engel <engelj at in dot tum dot de>,
 * for more information see <http://vision.in.tum.de/dso>.
 * If you use this code, please cite the respective publications as
 * listed on the above website.
 *
 * DSO is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DSO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DSO. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * KFBuffer.cpp
 *
 *  Created on: Jan 7, 2014
 *      Author: engelj
 */

#include "FullSystem/FullSystem.h"

#include "FullSystem/ImmaturePoint.h"
#include "FullSystem/PixelSelector.h"
#include "FullSystem/PixelSelector2.h"
#include "FullSystem/ResidualProjections.h"
#include "IOWrapper/ImageDisplay.h"
#include "stdio.h"
#include "util/globalCalib.h"
#include "util/globalFuncs.h"
#include <Eigen/Eigenvalues>
#include <Eigen/LU>
#include <Eigen/SVD>
#include <algorithm>

#include "FullSystem/CoarseInitializer.h"
#include "FullSystem/CoarseTracker.h"

#include "OptimizationBackend/EnergyFunctional.h"
#include "OptimizationBackend/EnergyFunctionalStructs.h"

#include "IOWrapper/OpenCV/BinaryCvMat.h"
#include "IOWrapper/Output3DWrapper.h"

#include "util/ImageAndExposure.h"

#include <cmath>

#include <chrono>
#include <monodepth2/monodepth.h>

namespace dso {
int FrameHessian::instanceCounter = 0;
int PointHessian::instanceCounter = 0;
int CalibHessian::instanceCounter = 0;

FullSystem::FullSystem(const std::string &path_cnn) {

  int retstat = 0;

  // Make an output folder
  std::string cmd = ("mkdir -p " + outputs_folder + "/invdepthmaps");
  auto junk = system(cmd.c_str());

  if (setting_logStuff) {

    retstat += system("rm -rf logs");
    retstat += system("mkdir logs");

    retstat += system("rm -rf mats");
    retstat += system("mkdir mats");
    calibLog = new std::ofstream();
    calibLog->open("logs/calibLog.txt", std::ios::trunc | std::ios::out);
    calibLog->precision(12);

    numsLog = new std::ofstream();
    numsLog->open("logs/numsLog.txt", std::ios::trunc | std::ios::out);
    numsLog->precision(10);

    coarseTrackingLog = new std::ofstream();
    coarseTrackingLog->open("logs/coarseTrackingLog.txt",
                            std::ios::trunc | std::ios::out);
    coarseTrackingLog->precision(10);

    eigenAllLog = new std::ofstream();
    eigenAllLog->open("logs/eigenAllLog.txt", std::ios::trunc | std::ios::out);
    eigenAllLog->precision(10);

    eigenPLog = new std::ofstream();
    eigenPLog->open("logs/eigenPLog.txt", std::ios::trunc | std::ios::out);
    eigenPLog->precision(10);

    eigenALog = new std::ofstream();
    eigenALog->open("logs/eigenALog.txt", std::ios::trunc | std::ios::out);
    eigenALog->precision(10);

    DiagonalLog = new std::ofstream();
    DiagonalLog->open("logs/diagonal.txt", std::ios::trunc | std::ios::out);
    DiagonalLog->precision(10);

    variancesLog = new std::ofstream();
    variancesLog->open("logs/variancesLog.txt",
                       std::ios::trunc | std::ios::out);
    variancesLog->precision(10);

    nullspacesLog = new std::ofstream();
    nullspacesLog->open("logs/nullspacesLog.txt",
                        std::ios::trunc | std::ios::out);
    nullspacesLog->precision(10);
  } else {
    nullspacesLog = 0;
    variancesLog = 0;
    DiagonalLog = 0;
    eigenALog = 0;
    eigenPLog = 0;
    eigenAllLog = 0;
    numsLog = 0;
    calibLog = 0;
  }

  assert(retstat != 293847);

  selectionMap = new float[wG[0] * hG[0]];

  coarseDistanceMap = new CoarseDistanceMap(wG[0], hG[0]);
  coarseTracker = new CoarseTracker(wG[0], hG[0]);
  coarseTracker_forNewKF = new CoarseTracker(wG[0], hG[0]);
  coarseInitializer = new CoarseInitializer(wG[0], hG[0]);
  pixelSelector = new PixelSelector(wG[0], hG[0]);

  statistics_lastNumOptIts = 0;
  statistics_numDroppedPoints = 0;
  statistics_numActivatedPoints = 0;
  statistics_numCreatedPoints = 0;
  statistics_numForceDroppedResBwd = 0;
  statistics_numForceDroppedResFwd = 0;
  statistics_numMargResFwd = 0;
  statistics_numMargResBwd = 0;

  lastCoarseRMSE.setConstant(100);

  currentMinActDist = 2;
  initialized = false;

  ef = new EnergyFunctional();
  ef->red = &this->treadReduce;

  isLost = false;
  initFailed = false;

  needNewKFAfter = -1;

  linearizeOperation = true;
  runMapping = true;
  mappingThread = boost::thread(&FullSystem::mappingLoop, this);
  lastRefStopID = 0;

  minIdJetVisDebug = -1;
  maxIdJetVisDebug = -1;
  minIdJetVisTracker = -1;
  maxIdJetVisTracker = -1;

  bool useGPU = true;
  depthPredictor = new MonoDepth(path_cnn, useGPU);
}
FullSystem::~FullSystem() {
  blockUntilMappingIsFinished();

  if (setting_logStuff) {
    calibLog->close();
    delete calibLog;
    numsLog->close();
    delete numsLog;
    coarseTrackingLog->close();
    delete coarseTrackingLog;
    // errorsLog->close(); delete errorsLog;
    eigenAllLog->close();
    delete eigenAllLog;
    eigenPLog->close();
    delete eigenPLog;
    eigenALog->close();
    delete eigenALog;
    DiagonalLog->close();
    delete DiagonalLog;
    variancesLog->close();
    delete variancesLog;
    nullspacesLog->close();
    delete nullspacesLog;
  }

  delete[] selectionMap;

  for (FrameShell *s : allFrameHistory)
    delete s;
  for (FrameHessian *fh : unmappedTrackedFrames)
    delete fh;

  delete coarseDistanceMap;
  delete coarseTracker;
  delete coarseTracker_forNewKF;
  delete coarseInitializer;
  delete pixelSelector;
  delete ef;
  delete depthPredictor;
}

void FullSystem::setOriginalCalib(const VecXf &originalCalib, int originalW,
                                  int originalH) {}

void FullSystem::setGammaFunction(float *BInv) {
  if (BInv == 0)
    return;

  // copy BInv.
  memcpy(Hcalib.Binv, BInv, sizeof(float) * 256);

  // invert.
  for (int i = 1; i < 255; i++) {
    // find val, such that Binv[val] = i.
    // I dont care about speed for this, so do it the stupid way.

    for (int s = 1; s < 255; s++) {
      if (BInv[s] <= i && BInv[s + 1] >= i) {
        Hcalib.B[i] = s + (i - BInv[s]) / (BInv[s + 1] - BInv[s]);
        break;
      }
    }
  }
  Hcalib.B[0] = 0;
  Hcalib.B[255] = 255;
}

void FullSystem::printResult(std::string file) {
  boost::unique_lock<boost::mutex> lock(trackMutex);
  boost::unique_lock<boost::mutex> crlock(shellPoseMutex);

  std::cout << "All frame history size: " << allFrameHistory.size() << "\n";
  std::cout << "All keyframe history size: " << allKeyFramesHistory.size() << "\n";
  std::cout << "frame hessians: " << frameHessians.size() << "\n";

  std::ofstream myfile;
  myfile.open(file.c_str());
  myfile << std::setprecision(15);

  myfile << "timestamp "
         << "file_prefix "
         << "translation[0] "
         << "translation[1] "
         << "translation[2] "
         << "rotation[0][0] "
         << "rotation[0][1] "
         << "rotation[0][2] "
         << "rotation[1][0] "
         << "rotation[1][1] "
         << "rotation[1][2] "
         << "rotation[2][0] "
         << "rotation[2][1] "
         << "rotation[2][2] "
         << "\n";

  for (FrameShell *s : allFrameHistory) {
    if (!s->poseValid) {
      // std::cout << "Frame_prefix " << s->file_prefix
      //           << " has an invalid pose and won't be logged: " << std::endl;
      continue;
    }

    if (setting_onlyLogKFPoses && s->marginalizedAt == s->id) {
      // std::cout << "Frame_prefix " << s->file_prefix
      //           << " is considered MARGINALIZED and won't be logged: "
      //           << std::endl;
      continue;
    }

    // Since "translation" vector is really global position, we need to convert
    // to actual T (translation of the global origin from the camera coordinate
    // system.
    // Pos = -R^T * T
    // T = -R * Pos
    std::vector<float> T;
    float t0 =
        (s->camToWorld.rotationMatrix()(0)) * (s->camToWorld.translation()(0)) +
        (s->camToWorld.rotationMatrix()(1)) * (s->camToWorld.translation()(1)) +
        (s->camToWorld.rotationMatrix()(2)) * (s->camToWorld.translation()(2));
    T.push_back(-1.0 * t0);
    float t1 =
        (s->camToWorld.rotationMatrix()(3)) * (s->camToWorld.translation()(0)) +
        (s->camToWorld.rotationMatrix()(4)) * (s->camToWorld.translation()(1)) +
        (s->camToWorld.rotationMatrix()(5)) * (s->camToWorld.translation()(2));
    T.push_back(-1.0 * t1);
    float t2 =
        (s->camToWorld.rotationMatrix()(6)) * (s->camToWorld.translation()(0)) +
        (s->camToWorld.rotationMatrix()(7)) * (s->camToWorld.translation()(1)) +
        (s->camToWorld.rotationMatrix()(8)) * (s->camToWorld.translation()(2));
    T.push_back(-1.0 * t2);

    // READ IN EACH COL AND PLACE AS A ROW
    // This has been verified to correctly write out the mathematical
    // form of R, such that R^T*[0 0 01] gives the correct viewing
    // direction of the camera
    // https://math.stackexchange.com/questions/82602/how-to-find-camera-position-and-rotation-from-a-4x4-matrix
    myfile << s->timestamp << " " << s->file_prefix << " " 
           << T[0] << " "
           << T[1] << " " 
           << T[2] << " " 
           << s->camToWorld.rotationMatrix()(0) << " " 
           << s->camToWorld.rotationMatrix()(1) << " "
           << s->camToWorld.rotationMatrix()(2) << " "
           << s->camToWorld.rotationMatrix()(3) << " "
           << s->camToWorld.rotationMatrix()(4) << " "
           << s->camToWorld.rotationMatrix()(5) << " "
           << s->camToWorld.rotationMatrix()(6) << " "
           << s->camToWorld.rotationMatrix()(7) << " "
           << s->camToWorld.rotationMatrix()(8) << "\n";
  }
  myfile.close();
}

void FullSystem::printPC(std::string file) {
  boost::unique_lock<boost::mutex> lock(trackMutex);
  boost::unique_lock<boost::mutex> crlock(shellPoseMutex);

  std::cout << "Total saved points: " << point_cloud.size() << "\n";

  std::ofstream myfile;
  myfile.open(file.c_str());
  myfile << std::setprecision(15);

  myfile << "ply\n"
         << "format ascii 1.0\n"
         << "element vertex " << point_cloud.size() << "\n"
         << "property float x\n"
         << "property float y\n"
         << "property float z\n"
         << "end_header\n";

  for (Vec3d point : point_cloud)
  {
    myfile << point[0] << " "
           << point[1] << " "
           << point[2] << "\n";
  }

  myfile.close();
}

Vec4 FullSystem::trackNewCoarse(FrameHessian *fh) {

  assert(allFrameHistory.size() > 0);
  // set pose initialization.

  for (IOWrap::Output3DWrapper *ow : outputWrapper)
    ow->pushLiveFrame(fh);

  FrameHessian *lastF = coarseTracker->lastRef;

  AffLight aff_last_2_l = AffLight(0, 0);

  std::vector<SE3, Eigen::aligned_allocator<SE3>> lastF_2_fh_tries;
  if (allFrameHistory.size() == 2) {
    initializeFromInitializerCNN(fh);

    lastF_2_fh_tries.push_back(SE3(Eigen::Matrix<double, 3, 3>::Identity(),
                                   Eigen::Matrix<double, 3, 1>::Zero()));
    for (float rotDelta = 0.02; rotDelta < 0.05; rotDelta = rotDelta + 0.02) {
      lastF_2_fh_tries.push_back(SE3(Sophus::Quaterniond(1, rotDelta, 0, 0),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(SE3(Sophus::Quaterniond(1, 0, rotDelta, 0),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(SE3(Sophus::Quaterniond(1, 0, 0, rotDelta),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(SE3(Sophus::Quaterniond(1, -rotDelta, 0, 0),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(SE3(Sophus::Quaterniond(1, 0, -rotDelta, 0),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(SE3(Sophus::Quaterniond(1, 0, 0, -rotDelta),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, rotDelta, rotDelta, 0),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, 0, rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, rotDelta, 0, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, -rotDelta, rotDelta, 0),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, 0, -rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, -rotDelta, 0, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, rotDelta, -rotDelta, 0),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, 0, rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, rotDelta, 0, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, -rotDelta, -rotDelta, 0),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, 0, -rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, -rotDelta, 0, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, -rotDelta, -rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, -rotDelta, -rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, -rotDelta, rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, -rotDelta, rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, rotDelta, -rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, rotDelta, -rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, rotDelta, rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          SE3(Sophus::Quaterniond(1, rotDelta, rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
    }

    coarseTracker->makeK(&Hcalib);
    coarseTracker->setCTRefForFirstFrame(frameHessians);

    lastF = coarseTracker->lastRef;
  } else {
    FrameShell *slast = allFrameHistory[allFrameHistory.size() - 2];
    FrameShell *sprelast = allFrameHistory[allFrameHistory.size() - 3];
    SE3 slast_2_sprelast;
    SE3 lastF_2_slast;
    { // lock on global pose consistency!
      boost::unique_lock<boost::mutex> crlock(shellPoseMutex);
      slast_2_sprelast = sprelast->camToWorld.inverse() * slast->camToWorld;
      lastF_2_slast = slast->camToWorld.inverse() * lastF->shell->camToWorld;
      aff_last_2_l = slast->aff_g2l;
    }
    SE3 fh_2_slast = slast_2_sprelast; // assumed to be the same as fh_2_slast.

    // get last delta-movement.
    lastF_2_fh_tries.push_back(fh_2_slast.inverse() *
                               lastF_2_slast); // assume constant motion.
    lastF_2_fh_tries.push_back(
        fh_2_slast.inverse() * fh_2_slast.inverse() *
        lastF_2_slast); // assume double motion (frame skipped)
    lastF_2_fh_tries.push_back(SE3::exp(fh_2_slast.log() * 0.5).inverse() *
                               lastF_2_slast); // assume half motion.
    lastF_2_fh_tries.push_back(lastF_2_slast); // assume zero motion.
    lastF_2_fh_tries.push_back(SE3());         // assume zero motion FROM KF.

    // just try a TON of different initializations (all rotations). In the end,
    // if they don't work they will only be tried on the coarsest level, which
    // is super fast anyway. also, if tracking rails here we loose, so we
    // really, really want to avoid that.
    for (float rotDelta = 0.02; rotDelta < 0.05; rotDelta++) {
      lastF_2_fh_tries.push_back(fh_2_slast.inverse() * lastF_2_slast *
                                 SE3(Sophus::Quaterniond(1, rotDelta, 0, 0),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(fh_2_slast.inverse() * lastF_2_slast *
                                 SE3(Sophus::Quaterniond(1, 0, rotDelta, 0),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(fh_2_slast.inverse() * lastF_2_slast *
                                 SE3(Sophus::Quaterniond(1, 0, 0, rotDelta),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(fh_2_slast.inverse() * lastF_2_slast *
                                 SE3(Sophus::Quaterniond(1, -rotDelta, 0, 0),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(fh_2_slast.inverse() * lastF_2_slast *
                                 SE3(Sophus::Quaterniond(1, 0, -rotDelta, 0),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(fh_2_slast.inverse() * lastF_2_slast *
                                 SE3(Sophus::Quaterniond(1, 0, 0, -rotDelta),
                                     Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, rotDelta, rotDelta, 0),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, 0, rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, rotDelta, 0, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, -rotDelta, rotDelta, 0),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, 0, -rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, -rotDelta, 0, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, rotDelta, -rotDelta, 0),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, 0, rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, rotDelta, 0, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, -rotDelta, -rotDelta, 0),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, 0, -rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, -rotDelta, 0, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, -rotDelta, -rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, -rotDelta, -rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, -rotDelta, rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, -rotDelta, rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, rotDelta, -rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, rotDelta, -rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, rotDelta, rotDelta, -rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
      lastF_2_fh_tries.push_back(
          fh_2_slast.inverse() * lastF_2_slast *
          SE3(Sophus::Quaterniond(1, rotDelta, rotDelta, rotDelta),
              Vec3(0, 0, 0))); // assume constant motion.
    }

    if (!slast->poseValid || !sprelast->poseValid || !lastF->shell->poseValid) {
      lastF_2_fh_tries.clear();
      lastF_2_fh_tries.push_back(SE3());
    }
  }

  Vec3 flowVecs = Vec3(100, 100, 100);
  SE3 lastF_2_fh = SE3();
  AffLight aff_g2l = AffLight(0, 0);

  // as long as maxResForImmediateAccept is not reached, I'll continue through
  // the options. I'll keep track of the so-far best achieved residual for each
  // level in achievedRes. If on a coarse level, tracking is WORSE than
  // achievedRes, we will not continue to save time.

  Vec5 achievedRes = Vec5::Constant(NAN);
  bool haveOneGood = false;
  int tryIterations = 0;
  for (unsigned int i = 0; i < lastF_2_fh_tries.size(); i++) {
    AffLight aff_g2l_this = aff_last_2_l;
    SE3 lastF_2_fh_this = lastF_2_fh_tries[i];
    bool trackingIsGood = coarseTracker->trackNewestCoarse(
        fh, lastF_2_fh_this, aff_g2l_this, pyrLevelsUsed - 1,
        achievedRes); // in each level has to be at least as good as the last
                      // try.
    tryIterations++;

    if (i != 0) {
      printf("RE-TRACK ATTEMPT %d with initOption %d and start-lvl %d (ab %f "
             "%f): %f %f %f %f %f -> %f %f %f %f %f \n",
             i, i, pyrLevelsUsed - 1, aff_g2l_this.a, aff_g2l_this.b,
             achievedRes[0], achievedRes[1], achievedRes[2], achievedRes[3],
             achievedRes[4], coarseTracker->lastResiduals[0],
             coarseTracker->lastResiduals[1], coarseTracker->lastResiduals[2],
             coarseTracker->lastResiduals[3], coarseTracker->lastResiduals[4]);
    }

    // do we have a new winner?
    if (trackingIsGood &&
        std::isfinite((float)coarseTracker->lastResiduals[0]) &&
        !(coarseTracker->lastResiduals[0] >= achievedRes[0])) {
      // printf("take over. minRes %f -> %f!\n", achievedRes[0],
      // coarseTracker->lastResiduals[0]);
      flowVecs = coarseTracker->lastFlowIndicators;
      aff_g2l = aff_g2l_this;
      lastF_2_fh = lastF_2_fh_this;
      haveOneGood = true;
    }

    // take over achieved res (always).
    if (haveOneGood) {
      for (int i = 0; i < 5; i++) {
        if (!std::isfinite((float)achievedRes[i]) ||
            achievedRes[i] >
                coarseTracker->lastResiduals[i]) // take over if achievedRes is
                                                 // either bigger or NAN.
          achievedRes[i] = coarseTracker->lastResiduals[i];
      }
    }

    if (haveOneGood &&
        achievedRes[0] < lastCoarseRMSE[0] * setting_reTrackThreshold)
      break;
  }

  if (!haveOneGood) {
    printf("BIG ERROR! tracking failed entirely. Take predictred pose and hope "
           "we may somehow recover.\n");
    flowVecs = Vec3(0, 0, 0);
    aff_g2l = aff_last_2_l;
    lastF_2_fh = lastF_2_fh_tries[0];
  }

  lastCoarseRMSE = achievedRes;

  // no lock required, as fh is not used anywhere yet.
  fh->shell->camToTrackingRef = lastF_2_fh.inverse();
  fh->shell->trackingRef = lastF->shell;
  fh->shell->aff_g2l = aff_g2l;
  fh->shell->camToWorld =
      fh->shell->trackingRef->camToWorld * fh->shell->camToTrackingRef;

  if (coarseTracker->firstCoarseRMSE < 0)
    coarseTracker->firstCoarseRMSE = achievedRes[0];

  if (!setting_debugout_runquiet)
    printf("Coarse Tracker tracked ab = %f %f (exp %f). Res %f!\n", aff_g2l.a,
           aff_g2l.b, fh->ab_exposure, achievedRes[0]);

  if (setting_logStuff) {
    (*coarseTrackingLog) << std::setprecision(16) << fh->shell->id << " "
                         << fh->shell->timestamp << " " << fh->ab_exposure
                         << " " << fh->shell->camToWorld.log().transpose()
                         << " " << aff_g2l.a << " " << aff_g2l.b << " "
                         << achievedRes[0] << " " << tryIterations << "\n";
  }

  return Vec4(achievedRes[0], flowVecs[0], flowVecs[1], flowVecs[2]);
}

void FullSystem::traceNewCoarse(FrameHessian *fh) {
  boost::unique_lock<boost::mutex> lock(mapMutex);

  int trace_total = 0, trace_good = 0, trace_oob = 0, trace_out = 0,
      trace_skip = 0, trace_badcondition = 0, trace_uninitialized = 0;

  Mat33f K = Mat33f::Identity();
  K(0, 0) = Hcalib.fxl();
  K(1, 1) = Hcalib.fyl();
  K(0, 2) = Hcalib.cxl();
  K(1, 2) = Hcalib.cyl();

  for (FrameHessian *host : frameHessians) // go through all active frames
  {

    SE3 hostToNew = fh->PRE_worldToCam * host->PRE_camToWorld;
    Mat33f KRKi = K * hostToNew.rotationMatrix().cast<float>() * K.inverse();
    Vec3f Kt = K * hostToNew.translation().cast<float>();

    Vec2f aff = AffLight::fromToVecExposure(host->ab_exposure, fh->ab_exposure,
                                            host->aff_g2l(), fh->aff_g2l())
                    .cast<float>();

    for (ImmaturePoint *ph : host->immaturePoints) {
      ph->traceOn(fh, KRKi, Kt, aff, &Hcalib, false);
      //			std::cout<<"idepth "<<ph->idepth_min<<"
      //"<<ph->idepth_max<<std::endl;

      if (ph->lastTraceStatus == ImmaturePointStatus::IPS_GOOD)
        trace_good++;
      if (ph->lastTraceStatus == ImmaturePointStatus::IPS_BADCONDITION)
        trace_badcondition++;
      if (ph->lastTraceStatus == ImmaturePointStatus::IPS_OOB)
        trace_oob++;
      if (ph->lastTraceStatus == ImmaturePointStatus::IPS_OUTLIER)
        trace_out++;
      if (ph->lastTraceStatus == ImmaturePointStatus::IPS_SKIPPED)
        trace_skip++;
      if (ph->lastTraceStatus == ImmaturePointStatus::IPS_UNINITIALIZED)
        trace_uninitialized++;
      trace_total++;
    }
  }
  //	printf("ADD: TRACE: %'d points. %'d (%.0f%%) good. %'d (%.0f%%) skip.
  //%'d (%.0f%%) badcond. %'d (%.0f%%) oob. %'d (%.0f%%) out. %'d (%.0f%%)
  // uninit.\n", 			trace_total, trace_good,
  // 100*trace_good/(float)trace_total, 			trace_skip,
  // 100*trace_skip/(float)trace_total, trace_badcondition,
  // 100*trace_badcondition/(float)trace_total, trace_oob,
  // 100*trace_oob/(float)trace_total, 			trace_out,
  // 100*trace_out/(float)trace_total, 			trace_uninitialized,
  // 100*trace_uninitialized/(float)trace_total);
}

void FullSystem::activatePointsMT_Reductor(
    std::vector<PointHessian *> *optimized,
    std::vector<ImmaturePoint *> *toOptimize, int min, int max, Vec10 *stats,
    int tid) {
  ImmaturePointTemporaryResidual *tr =
      new ImmaturePointTemporaryResidual[frameHessians.size()];
  for (int k = min; k < max; k++) {
    (*optimized)[k] = optimizeImmaturePoint((*toOptimize)[k], 1, tr);
  }
  delete[] tr;
}

void FullSystem::activatePointsMT() {

  if (ef->nPoints < setting_desiredPointDensity * 0.66)
    currentMinActDist -= 0.8;
  if (ef->nPoints < setting_desiredPointDensity * 0.8)
    currentMinActDist -= 0.5;
  else if (ef->nPoints < setting_desiredPointDensity * 0.9)
    currentMinActDist -= 0.2;
  else if (ef->nPoints < setting_desiredPointDensity)
    currentMinActDist -= 0.1;

  if (ef->nPoints > setting_desiredPointDensity * 1.5)
    currentMinActDist += 0.8;
  if (ef->nPoints > setting_desiredPointDensity * 1.3)
    currentMinActDist += 0.5;
  if (ef->nPoints > setting_desiredPointDensity * 1.15)
    currentMinActDist += 0.2;
  if (ef->nPoints > setting_desiredPointDensity)
    currentMinActDist += 0.1;

  if (currentMinActDist < 0)
    currentMinActDist = 0;
  if (currentMinActDist > 4)
    currentMinActDist = 4;

  if (!setting_debugout_runquiet)
    printf("SPARSITY:  MinActDist %f (need %d points, have %d points)!\n",
           currentMinActDist, (int)(setting_desiredPointDensity), ef->nPoints);

  FrameHessian *newestHs = frameHessians.back();

  // make dist map.
  coarseDistanceMap->makeK(&Hcalib);
  coarseDistanceMap->makeDistanceMap(frameHessians, newestHs);

  // coarseTracker->debugPlotDistMap("distMap");

  std::vector<ImmaturePoint *> toOptimize;
  toOptimize.reserve(20000);

  for (FrameHessian *host : frameHessians) // go through all active frames
  {
    if (host == newestHs)
      continue;

    SE3 fhToNew = newestHs->PRE_worldToCam * host->PRE_camToWorld;
    Mat33f KRKi =
        (coarseDistanceMap->K[1] * fhToNew.rotationMatrix().cast<float>() *
         coarseDistanceMap->Ki[0]);
    Vec3f Kt = (coarseDistanceMap->K[1] * fhToNew.translation().cast<float>());

    for (unsigned int i = 0; i < host->immaturePoints.size(); i += 1) {
      ImmaturePoint *ph = host->immaturePoints[i];
      ph->idxInImmaturePoints = i;

      // delete points that have never been traced successfully, or that are
      // outlier on the last trace.
      if (!std::isfinite(ph->idepth_max) ||
          ph->lastTraceStatus == IPS_OUTLIER) {
        //				immature_invalid_deleted++;
        // remove point.
        delete ph;
        host->immaturePoints[i] = 0;
        continue;
      }

      // can activate only if this is true.
      bool canActivate = (ph->lastTraceStatus == IPS_GOOD ||
                          ph->lastTraceStatus == IPS_SKIPPED ||
                          ph->lastTraceStatus == IPS_BADCONDITION ||
                          ph->lastTraceStatus == IPS_OOB) &&
                         ph->lastTracePixelInterval < 8 &&
                         ph->quality > setting_minTraceQuality &&
                         (ph->idepth_max + ph->idepth_min) > 0;

      // if I cannot activate the point, skip it. Maybe also delete it.
      if (!canActivate) {
        // if point will be out afterwards, delete it instead.
        if (ph->host->flaggedForMarginalization ||
            ph->lastTraceStatus == IPS_OOB) {
          //					immature_notReady_deleted++;
          delete ph;
          host->immaturePoints[i] = 0;
        }
        //				immature_notReady_skipped++;
        continue;
      }

      // see if we need to activate point due to distance map.
      Vec3f ptp = KRKi * Vec3f(ph->u, ph->v, 1) +
                  Kt * (0.5f * (ph->idepth_max + ph->idepth_min));
      int u = ptp[0] / ptp[2] + 0.5f;
      int v = ptp[1] / ptp[2] + 0.5f;

      if ((u > 0 && v > 0 && u < wG[1] && v < hG[1])) {

        float dist = coarseDistanceMap->fwdWarpedIDDistFinal[u + wG[1] * v] +
                     (ptp[0] - floorf((float)(ptp[0])));

        if (dist >= currentMinActDist * ph->my_type) {
          coarseDistanceMap->addIntoDistFinal(u, v);
          toOptimize.push_back(ph);
        }
      } else {
        delete ph;
        host->immaturePoints[i] = 0;
      }
    }
  }

  //	printf("ACTIVATE: %d. (del %d, notReady %d, marg %d, good %d, marg-skip
  //%d)\n", 			(int)toOptimize.size(), immature_deleted,
  // immature_notReady, immature_needMarg, immature_want, immature_margskip);

  std::vector<PointHessian *> optimized;
  optimized.resize(toOptimize.size());

  if (multiThreading)
    treadReduce.reduce(boost::bind(&FullSystem::activatePointsMT_Reductor, this,
                                   &optimized, &toOptimize, _1, _2, _3, _4),
                       0, toOptimize.size(), 50);

  else
    activatePointsMT_Reductor(&optimized, &toOptimize, 0, toOptimize.size(), 0,
                              0);

  for (unsigned k = 0; k < toOptimize.size(); k++) {
    PointHessian *newpoint = optimized[k];
    ImmaturePoint *ph = toOptimize[k];

    if (newpoint != 0 && newpoint != (PointHessian *)((long)(-1))) {
      newpoint->host->immaturePoints[ph->idxInImmaturePoints] = 0;
      newpoint->host->pointHessians.push_back(newpoint);
      ef->insertPoint(newpoint);
      for (PointFrameResidual *r : newpoint->residuals)
        ef->insertResidual(r);
      assert(newpoint->efPoint != 0);
      delete ph;
    } else if (newpoint == (PointHessian *)((long)(-1)) ||
               ph->lastTraceStatus == IPS_OOB) {
      delete ph;
      ph->host->immaturePoints[ph->idxInImmaturePoints] = 0;
    } else {
      assert(newpoint == 0 || newpoint == (PointHessian *)((long)(-1)));
    }
  }

  for (FrameHessian *host : frameHessians) {
    for (int i = 0; i < (int)host->immaturePoints.size(); i++) {
      if (host->immaturePoints[i] == 0) {
        host->immaturePoints[i] = host->immaturePoints.back();
        host->immaturePoints.pop_back();
        i--;
      }
    }
  }
}

void FullSystem::activatePointsOldFirst() { assert(false); }

void FullSystem::flagPointsForRemoval() {
  assert(EFIndicesValid);

  std::vector<FrameHessian *> fhsToKeepPoints;
  std::vector<FrameHessian *> fhsToMargPoints;

  // if(setting_margPointVisWindow>0)
  {
    for (int i = ((int)frameHessians.size()) - 1;
         i >= 0 && i >= ((int)frameHessians.size()); i--)
      if (!frameHessians[i]->flaggedForMarginalization)
        fhsToKeepPoints.push_back(frameHessians[i]);

    for (int i = 0; i < (int)frameHessians.size(); i++)
      if (frameHessians[i]->flaggedForMarginalization)
        fhsToMargPoints.push_back(frameHessians[i]);
  }

  // ef->setAdjointsF();
  // ef->setDeltaF(&Hcalib);
  int flag_oob = 0, flag_in = 0, flag_inin = 0, flag_nores = 0;

  for (FrameHessian *host : frameHessians) // go through all active frames
  {
    for (unsigned int i = 0; i < host->pointHessians.size(); i++) {
      PointHessian *ph = host->pointHessians[i];
      if (ph == 0)
        continue;

      if (ph->idepth_scaled < 0 || ph->residuals.size() == 0) {
        host->pointHessiansOut.push_back(ph);
        ph->efPoint->stateFlag = EFPointStatus::PS_DROP;
        host->pointHessians[i] = 0;
        flag_nores++;
      } else if (ph->isOOB(fhsToKeepPoints, fhsToMargPoints) ||
                 host->flaggedForMarginalization) {
        flag_oob++;
        if (ph->isInlierNew()) {
          flag_in++;
          int ngoodRes = 0;
          for (PointFrameResidual *r : ph->residuals) {
            r->resetOOB();
            r->linearize(&Hcalib);
            r->efResidual->isLinearized = false;
            r->applyRes(true);
            if (r->efResidual->isActive()) {
              r->efResidual->fixLinearizationF(ef);
              ngoodRes++;
            }
          }
          if (ph->idepth_hessian > setting_minIdepthH_marg) {
            flag_inin++;
            ph->efPoint->stateFlag = EFPointStatus::PS_MARGINALIZE;
            host->pointHessiansMarginalized.push_back(ph);
          } else {
            ph->efPoint->stateFlag = EFPointStatus::PS_DROP;
            host->pointHessiansOut.push_back(ph);
          }

        } else {
          host->pointHessiansOut.push_back(ph);
          ph->efPoint->stateFlag = EFPointStatus::PS_DROP;

          // printf("drop point in frame %d (%d goodRes, %d activeRes)\n",
          // ph->host->idx, ph->numGoodResiduals, (int)ph->residuals.size());
        }

        host->pointHessians[i] = 0;
      }
    }

    for (int i = 0; i < (int)host->pointHessians.size(); i++) {
      if (host->pointHessians[i] == 0) {
        host->pointHessians[i] = host->pointHessians.back();
        host->pointHessians.pop_back();
        i--;
      }
    }
  }
}

void FullSystem::addActiveFrame(ImageAndExposure *image, int id,
                                std::string prefix) {

  if (isLost) {
    return;
  }
  boost::unique_lock<boost::mutex> lock(trackMutex);

  // =========================== add into allFrameHistory
  // =========================
  FrameHessian *fh = new FrameHessian();
  FrameShell *shell = new FrameShell();
  shell->camToWorld =
      SE3(); // no lock required, as fh is not used anywhere yet.
  shell->aff_g2l = AffLight(0, 0);
  shell->marginalizedAt = shell->id = allFrameHistory.size();
  shell->timestamp = image->timestamp;
  shell->incoming_id = id;
  shell->file_prefix = prefix;
  fh->shell = shell;
  allFrameHistory.push_back(shell);

  // =========================== make Images / derivatives etc.
  // =========================
  fh->ab_exposure = image->exposure_time;
  fh->makeImages(image->image, &Hcalib);
  fh->rgb_image = image->rgb_image;

  if (!initialized) {
    // use initializer!
    if (coarseInitializer->frameID <
        0) // first frame set. fh is kept by coarseInitializer.
    {
      coarseInitializer->setFirst(&Hcalib, fh, getDepthMap(fh));
      initialized = true;
    }

    return;
  } else // do front-end operation.
  {
    // =========================== SWAP tracking reference?.
    // =========================
    if (coarseTracker_forNewKF->refFrameID > coarseTracker->refFrameID) {
      boost::unique_lock<boost::mutex> crlock(coarseTrackerSwapMutex);
      CoarseTracker *tmp = coarseTracker;
      coarseTracker = coarseTracker_forNewKF;
      coarseTracker_forNewKF = tmp;
    }

    Vec4 tres = trackNewCoarse(fh);
    if (!std::isfinite((double)tres[0]) || !std::isfinite((double)tres[1]) ||
        !std::isfinite((double)tres[2]) || !std::isfinite((double)tres[3])) {
      printf("Initial Tracking failed: LOST!\n");
      isLost = true;
      return;
    }

    bool needToMakeKF = false;
    if (setting_keyframesPerSecond > 0) {
      needToMakeKF =
          allFrameHistory.size() == 1 ||
          (fh->shell->timestamp - allKeyFramesHistory.back()->timestamp) >
              0.95f / setting_keyframesPerSecond;
    } else {
      Vec2 refToFh = AffLight::fromToVecExposure(
          coarseTracker->lastRef->ab_exposure, fh->ab_exposure,
          coarseTracker->lastRef_aff_g2l, fh->shell->aff_g2l);

      // BRIGHTNESS CHECK
      needToMakeKF = allFrameHistory.size() == 1 ||
                     setting_kfGlobalWeight * setting_maxShiftWeightT *
                                 sqrtf((double)tres[1]) / (wG[0] + hG[0]) +
                             setting_kfGlobalWeight * setting_maxShiftWeightR *
                                 sqrtf((double)tres[2]) / (wG[0] + hG[0]) +
                             setting_kfGlobalWeight * setting_maxShiftWeightRT *
                                 sqrtf((double)tres[3]) / (wG[0] + hG[0]) +
                             setting_kfGlobalWeight * setting_maxAffineWeight *
                                 fabs(logf((float)refToFh[0])) >
                         1 ||
                     2 * coarseTracker->firstCoarseRMSE < tres[0];
    }

    for (IOWrap::Output3DWrapper *ow : outputWrapper)
      ow->publishCamPose(fh->shell, &Hcalib);

    lock.unlock();
    deliverTrackedFrame(fh, needToMakeKF);
    return;
  }
}
void FullSystem::deliverTrackedFrame(FrameHessian *fh, bool needKF) {

  if (linearizeOperation) {
    if (goStepByStep && lastRefStopID != coarseTracker->refFrameID) {
      MinimalImageF3 img(wG[0], hG[0], fh->dI);
      IOWrap::displayImage("frameToTrack", &img);
      while (true) {
        char k = IOWrap::waitKey(0);
        if (k == ' ')
          break;
        handleKey(k);
      }
      lastRefStopID = coarseTracker->refFrameID;
    } else
      handleKey(IOWrap::waitKey(1));

    if (needKF) {
      makeKeyFrame(fh);
    } else {
      makeNonKeyFrame(fh);
    }
  } else {
    boost::unique_lock<boost::mutex> lock(trackMapSyncMutex);
    unmappedTrackedFrames.push_back(fh);
    if (needKF)
      needNewKFAfter = fh->shell->trackingRef->id;
    trackedFrameSignal.notify_all();

    while (coarseTracker_forNewKF->refFrameID == -1 &&
           coarseTracker->refFrameID == -1) {
      mappedFrameSignal.wait(lock);
    }

    lock.unlock();
  }
}

void FullSystem::mappingLoop() {
  boost::unique_lock<boost::mutex> lock(trackMapSyncMutex);

  while (runMapping) {
    while (unmappedTrackedFrames.size() == 0) {
      trackedFrameSignal.wait(lock);
      if (!runMapping)
        return;
    }

    FrameHessian *fh = unmappedTrackedFrames.front();
    unmappedTrackedFrames.pop_front();

    // guaranteed to make a KF for the very first two tracked frames.
    if (allKeyFramesHistory.size() <= 2) {
      lock.unlock();
      makeKeyFrame(fh);
      lock.lock();
      mappedFrameSignal.notify_all();
      continue;
    }

    if (unmappedTrackedFrames.size() > 3)
      needToKetchupMapping = true;

    if (unmappedTrackedFrames.size() >
        0) // if there are other frames to tracke, do that first.
    {
      lock.unlock();
      makeNonKeyFrame(fh);
      lock.lock();

      if (needToKetchupMapping && unmappedTrackedFrames.size() > 0) {
        FrameHessian *fh = unmappedTrackedFrames.front();
        unmappedTrackedFrames.pop_front();
        {
          boost::unique_lock<boost::mutex> crlock(shellPoseMutex);
          assert(fh->shell->trackingRef != 0);
          fh->shell->camToWorld =
              fh->shell->trackingRef->camToWorld * fh->shell->camToTrackingRef;
          fh->setEvalPT_scaled(fh->shell->camToWorld.inverse(),
                               fh->shell->aff_g2l);
        }
        delete fh;
      }

    } else {
      if (setting_realTimeMaxKF ||
          needNewKFAfter >= frameHessians.back()->shell->id) {
        lock.unlock();
        makeKeyFrame(fh);
        needToKetchupMapping = false;
        lock.lock();
      } else {
        lock.unlock();
        makeNonKeyFrame(fh);
        lock.lock();
      }
    }
    mappedFrameSignal.notify_all();
  }
  printf("MAPPING FINISHED!\n");
}

void FullSystem::blockUntilMappingIsFinished() {
  boost::unique_lock<boost::mutex> lock(trackMapSyncMutex);
  runMapping = false;
  trackedFrameSignal.notify_all();
  lock.unlock();

  mappingThread.join();
}

void FullSystem::makeNonKeyFrame(FrameHessian *fh) {
  // needs to be set by mapping thread. no lock required since we are in mapping
  // thread.
  {
    boost::unique_lock<boost::mutex> crlock(shellPoseMutex);
    assert(fh->shell->trackingRef != 0);
    fh->shell->camToWorld =
        fh->shell->trackingRef->camToWorld * fh->shell->camToTrackingRef;
    fh->setEvalPT_scaled(fh->shell->camToWorld.inverse(), fh->shell->aff_g2l);
  }

  traceNewCoarse(fh);
  delete fh;
}

void FullSystem::makeKeyFrame(FrameHessian *fh) {
  // needs to be set by mapping thread
  {
    boost::unique_lock<boost::mutex> crlock(shellPoseMutex);
    assert(fh->shell->trackingRef != 0);
    fh->shell->camToWorld =
        fh->shell->trackingRef->camToWorld * fh->shell->camToTrackingRef;
    fh->setEvalPT_scaled(fh->shell->camToWorld.inverse(), fh->shell->aff_g2l);
  }

  traceNewCoarse(fh);

  boost::unique_lock<boost::mutex> lock(mapMutex);

  // =========================== Flag Frames to be Marginalized.
  // =========================
  flagFramesForMarginalization(fh);

  // =========================== add New Frame to Hessian Struct.
  // =========================
  fh->idx = frameHessians.size();
  frameHessians.push_back(fh);
  fh->frameID = allKeyFramesHistory.size();
  allKeyFramesHistory.push_back(fh->shell);
  ef->insertFrame(fh, &Hcalib);

  setPrecalcValues();

  // =========================== add new residuals for old points
  // =========================
  int numFwdResAdde = 0;
  for (FrameHessian *fh1 : frameHessians) // go through all active frames
  {
    if (fh1 == fh)
      continue;
    for (PointHessian *ph : fh1->pointHessians) {
      PointFrameResidual *r = new PointFrameResidual(ph, fh1, fh);
      r->setState(ResState::IN);
      ph->residuals.push_back(r);
      ef->insertResidual(r);
      ph->lastResiduals[1] = ph->lastResiduals[0];
      ph->lastResiduals[0] =
          std::pair<PointFrameResidual *, ResState>(r, ResState::IN);
      numFwdResAdde += 1;
    }
  }

  // =========================== Activate Points (& flag for marginalization).
  // =========================
  activatePointsMT();
  ef->makeIDX();

  // =========================== OPTIMIZE ALL =========================

  fh->frameEnergyTH = frameHessians.back()->frameEnergyTH;
  float rmse = optimize(setting_maxOptIterations);

  // =========================== Figure Out if INITIALIZATION FAILED
  // =========================
  if (allKeyFramesHistory.size() <= 4) {
    if (allKeyFramesHistory.size() == 2 &&
        rmse > 20 * benchmark_initializerSlackFactor) {
      printf("I THINK INITIALIZATINO FAILED! Resetting.\n");
      initFailed = true;
    }
    if (allKeyFramesHistory.size() == 3 &&
        rmse > 13 * benchmark_initializerSlackFactor) {
      printf("I THINK INITIALIZATINO FAILED! Resetting.\n");
      initFailed = true;
    }
    if (allKeyFramesHistory.size() == 4 &&
        rmse > 9 * benchmark_initializerSlackFactor) {
      printf("I THINK INITIALIZATINO FAILED! Resetting.\n");
      initFailed = true;
    }
  }

  if (isLost)
    return;

  // =========================== REMOVE OUTLIER =========================
  removeOutliers();

  {
    boost::unique_lock<boost::mutex> crlock(coarseTrackerSwapMutex);
    coarseTracker_forNewKF->makeK(&Hcalib);
    coarseTracker_forNewKF->setCoarseTrackingRef(frameHessians);

    coarseTracker_forNewKF->debugPlotIDepthMap(
        &minIdJetVisTracker, &maxIdJetVisTracker, outputWrapper);
    coarseTracker_forNewKF->debugPlotIDepthMapFloat(outputWrapper);
  }

  debugPlot("post Optimize");

  // =========================== (Activate-)Marginalize Points
  // =========================
  flagPointsForRemoval();
  ef->dropPointsF();
  getNullspaces(ef->lastNullspaces_pose, ef->lastNullspaces_scale,
                ef->lastNullspaces_affA, ef->lastNullspaces_affB);
  ef->marginalizePointsF();

  // =========================== add new Immature points & new residuals
  // =========================
  makeNewTraces(fh, 0);

  for (IOWrap::Output3DWrapper *ow : outputWrapper) {
    ow->publishGraph(ef->connectivityMap);
    ow->publishKeyframes(frameHessians, false, &Hcalib);
  }

  // =========================== Marginalize Frames =========================

  for (unsigned int i = 0; i < frameHessians.size(); i++)
    if (frameHessians[i]->flaggedForMarginalization) {
      marginalizeFrame(frameHessians[i]);
      i = 0;
    }

  printLogLine();
  // printEigenValLine();
}

void FullSystem::initializeFromInitializer(FrameHessian *newFrame) {
  boost::unique_lock<boost::mutex> lock(mapMutex);

  // add firstframe.
  FrameHessian *firstFrame = coarseInitializer->firstFrame;
  firstFrame->idx = frameHessians.size();
  frameHessians.push_back(firstFrame);
  firstFrame->frameID = allKeyFramesHistory.size();
  allKeyFramesHistory.push_back(firstFrame->shell);
  ef->insertFrame(firstFrame, &Hcalib);
  setPrecalcValues();

  // int numPointsTotal = makePixelStatus(firstFrame->dI, selectionMap, wG[0],
  // hG[0], setting_desiredDensity); int numPointsTotal =
  // pixelSelector->makeMaps(firstFrame->dIp,
  // selectionMap,setting_desiredDensity);

  firstFrame->pointHessians.reserve(wG[0] * hG[0] * 0.2f);
  firstFrame->pointHessiansMarginalized.reserve(wG[0] * hG[0] * 0.2f);
  firstFrame->pointHessiansOut.reserve(wG[0] * hG[0] * 0.2f);

  float sumID = 1e-5, numID = 1e-5;
  for (int i = 0; i < coarseInitializer->numPoints[0]; i++) {
    sumID += coarseInitializer->points[0][i].iR;
    numID++;
  }
  float rescaleFactor = 1 / (sumID / numID);

  // randomly sub-select the points I need.
  float keepPercentage =
      setting_desiredPointDensity / coarseInitializer->numPoints[0];

  if (!setting_debugout_runquiet)
    printf("Initialization: keep %.1f%% (need %d, have %d)!\n",
           100 * keepPercentage, (int)(setting_desiredPointDensity),
           coarseInitializer->numPoints[0]);

  for (int i = 0; i < coarseInitializer->numPoints[0]; i++) {
    if (rand() / (float)RAND_MAX > keepPercentage)
      continue;

    Pnt *point = coarseInitializer->points[0] + i;
    ImmaturePoint *pt = new ImmaturePoint(point->u + 0.5f, point->v + 0.5f,
                                          firstFrame, point->my_type, &Hcalib);

    if (!std::isfinite(pt->energyTH)) {
      delete pt;
      continue;
    }
    pt->idepth_max = pt->idepth_min = 1;
    PointHessian *ph = new PointHessian(pt, &Hcalib);
    delete pt;
    if (!std::isfinite(ph->energyTH)) {
      delete ph;
      continue;
    }

    ph->setIdepthScaled(point->iR * rescaleFactor);
    ph->setIdepthZero(ph->idepth);
    ph->hasDepthPrior = true;
    ph->setPointStatus(PointHessian::ACTIVE);

    firstFrame->pointHessians.push_back(ph);
    ef->insertPoint(ph);
  }

  SE3 firstToNew = coarseInitializer->thisToNext;
  firstToNew.translation() /= rescaleFactor;

  // really no lock required, as we are initializing.
  {
    boost::unique_lock<boost::mutex> crlock(shellPoseMutex);
    firstFrame->shell->camToWorld = SE3();
    firstFrame->shell->aff_g2l = AffLight(0, 0);
    firstFrame->setEvalPT_scaled(firstFrame->shell->camToWorld.inverse(),
                                 firstFrame->shell->aff_g2l);
    firstFrame->shell->trackingRef = 0;
    firstFrame->shell->camToTrackingRef = SE3();

    newFrame->shell->camToWorld = firstToNew.inverse();
    newFrame->shell->aff_g2l = AffLight(0, 0);
    newFrame->setEvalPT_scaled(newFrame->shell->camToWorld.inverse(),
                               newFrame->shell->aff_g2l);
    newFrame->shell->trackingRef = firstFrame->shell;
    newFrame->shell->camToTrackingRef = firstToNew.inverse();
  }

  initialized = true;
  printf("INITIALIZE FROM INITIALIZER (%d pts)!\n",
         (int)firstFrame->pointHessians.size());
}

void FullSystem::initializeFromInitializerCNN(FrameHessian *newFrame) {
  boost::unique_lock<boost::mutex> lock(mapMutex);

  // add firstframe.
  FrameHessian *firstFrame = coarseInitializer->firstFrame;
  firstFrame->idx = frameHessians.size();
  frameHessians.push_back(firstFrame);
  firstFrame->frameID = allKeyFramesHistory.size();
  allKeyFramesHistory.push_back(firstFrame->shell);
  ef->insertFrame(firstFrame, &Hcalib);
  setPrecalcValues();

  firstFrame->pointHessians.reserve(wG[0] * hG[0] * 0.2f);
  firstFrame->pointHessiansMarginalized.reserve(wG[0] * hG[0] * 0.2f);
  firstFrame->pointHessiansOut.reserve(wG[0] * hG[0] * 0.2f);

  float sumID = 1e-5, numID = 1e-5;
  for (int i = 0; i < coarseInitializer->numPoints[0]; i++) {
    sumID += coarseInitializer->points[0][i].iR;
    numID++;
  }

  // randomly sub-select the points I need.
  float keepPercentage =
      setting_desiredPointDensity / coarseInitializer->numPoints[0];

  if (!setting_debugout_runquiet)
    printf("Initialization: keep %.1f%% (need %d, have %d)!\n",
           100 * keepPercentage, (int)(setting_desiredPointDensity),
           coarseInitializer->numPoints[0]);

  // Save the cropped input image file for use downstream in dense point cloud
  // fusion
  cv::Mat image = newFrame->rgb_image;
  cv::Mat bgr_channels[3], rgb_channels[3];
  cv::split(image, rgb_channels);
  bgr_channels[0] = rgb_channels[2];
  bgr_channels[1] = rgb_channels[1];
  bgr_channels[2] = rgb_channels[0];
  cv::Mat bgr_image;
  cv::merge(bgr_channels, 3, bgr_image);
  std::string imagefile =
      outputs_folder + "/images/" + newFrame->shell->file_prefix + ".png";
  cv::imwrite(imagefile, bgr_image);

  // Save inverse depthmap to file as binary
  cv::Mat invdepth = getDepthMap(firstFrame);
  // std::string invdepthfile =
  //     outputs_folder + "/invdepthmaps/" + firstFrame->shell->file_prefix + ".bin";
  // SaveMatBinary(invdepthfile, invdepth);

  float *invdepthmap_ptr = (float *)invdepth.data;
  for (int i = 0; i < coarseInitializer->numPoints[0]; i++) {
    if (rand() / (float)RAND_MAX > keepPercentage)
      continue;

    Pnt *point = coarseInitializer->points[0] + i;
    ImmaturePoint *pt = new ImmaturePoint(point->u + 0.5f, point->v + 0.5f,
                                          firstFrame, point->my_type, &Hcalib);

    float idepth = *(invdepthmap_ptr + int((point->v * wG[0] + point->u + 0.5f)));
    float depth = 1.0 / idepth;
    float var = 1.0 / (6 * depth);
    pt->idepth_max = idepth;
    pt->idepth_min = idepth;
    if (pt->idepth_min < 0)
      pt->idepth_min = 0;

    PointHessian *ph = new PointHessian(pt, &Hcalib);
    delete pt;
    if (!std::isfinite(ph->energyTH)) {
      delete ph;
      continue;
    }

    ph->setIdepthScaled(idepth);
    ph->setIdepthZero(idepth);
    ph->hasDepthPrior = true;
    ph->setPointStatus(PointHessian::ACTIVE);

    firstFrame->pointHessians.push_back(ph);
    ef->insertPoint(ph);
  }

  SE3 firstToNew = coarseInitializer->thisToNext;

  // really no lock required, as we are initializing.
  {
    boost::unique_lock<boost::mutex> crlock(shellPoseMutex);
    firstFrame->shell->camToWorld = SE3();
    firstFrame->shell->aff_g2l = AffLight(0, 0);
    firstFrame->setEvalPT_scaled(firstFrame->shell->camToWorld.inverse(),
                                 firstFrame->shell->aff_g2l);
    firstFrame->shell->trackingRef = 0;
    firstFrame->shell->camToTrackingRef = SE3();

    newFrame->shell->camToWorld = firstToNew.inverse();
    newFrame->shell->aff_g2l = AffLight(0, 0);
    newFrame->setEvalPT_scaled(newFrame->shell->camToWorld.inverse(),
                               newFrame->shell->aff_g2l);
    newFrame->shell->trackingRef = firstFrame->shell;
    newFrame->shell->camToTrackingRef = firstToNew.inverse();
  }

  initialized = true;
  printf("INITIALIZE FROM INITIALIZER (%d pts)!\n",
         (int)firstFrame->pointHessians.size());
}

void FullSystem::makeNewTraces(FrameHessian *newFrame, float *gtDepth) {
  pixelSelector->allowFast = true;
  // int numPointsTotal = makePixelStatus(newFrame->dI, selectionMap, wG[0],
  // hG[0], setting_desiredDensity);
  int numPointsTotal = pixelSelector->makeMaps(newFrame, selectionMap,
                                               setting_desiredImmatureDensity);

  newFrame->pointHessians.reserve(numPointsTotal * 1.2f);
  // fh->pointHessiansInactive.reserve(numPointsTotal*1.2f);
  newFrame->pointHessiansMarginalized.reserve(numPointsTotal * 1.2f);
  newFrame->pointHessiansOut.reserve(numPointsTotal * 1.2f);

  // Save the cropped input image file for use downstream in dense point cloud
  // fusion
  cv::Mat image = newFrame->rgb_image;
  cv::Mat bgr_channels[3], rgb_channels[3];
  cv::split(image, rgb_channels);
  bgr_channels[0] = rgb_channels[2];
  bgr_channels[1] = rgb_channels[1];
  bgr_channels[2] = rgb_channels[0];
  cv::Mat bgr_image;
  cv::merge(bgr_channels, 3, bgr_image);
  std::string imagefile =
      outputs_folder + "/images/" + newFrame->shell->file_prefix + ".png";
  cv::imwrite(imagefile, bgr_image);

  // Save inverse depthmap to file as binary
  cv::Mat invdepth = getDepthMap(newFrame);
  // std::string invdepthfile =
  //     outputs_folder + "/invdepthmaps/" + newFrame->shell->file_prefix + ".bin";
  // std::cout << " makeNewTraces: Saving inverse depth map " << invdepthfile
  //           << std::endl;
  // SaveMatBinary(invdepthfile, invdepth);

  for (IOWrap::Output3DWrapper *ow : outputWrapper) {

    cv::Mat invdepth_show = invdepth.clone();
    dispToDisplay(invdepth_show);
    ow->pushCNNImage(invdepth_show);
  }

  float *invdepthmap_ptr = (float *)invdepth.data;

  for (int y = patternPadding + 1; y < hG[0] - patternPadding - 2; y++) {
    for (int x = patternPadding + 1; x < wG[0] - patternPadding - 2; x++) {

      int i = x + y * wG[0];
      if (selectionMap[i] == 0) {
        continue;
      }

      ImmaturePoint *impt =
          new ImmaturePoint(x, y, newFrame, selectionMap[i], &Hcalib);
      impt->idepth_max = (*(invdepthmap_ptr + i));
      impt->idepth_min = (*(invdepthmap_ptr + i));

      if (impt->idepth_min < 0) {
        impt->idepth_min = 0;
      }

      if (!std::isfinite(impt->energyTH)) {
        delete impt;
      } else {
        newFrame->immaturePoints.push_back(impt);
      }
    }
  }
  // printf("MADE %d IMMATURE POINTS!\n", (int)newFrame->immaturePoints.size());
}

void FullSystem::setPrecalcValues() {
  for (FrameHessian *fh : frameHessians) {
    fh->targetPrecalc.resize(frameHessians.size());
    for (unsigned int i = 0; i < frameHessians.size(); i++)
      fh->targetPrecalc[i].set(fh, frameHessians[i], &Hcalib);
  }

  ef->setDeltaF(&Hcalib);
}

void FullSystem::printLogLine() {
  if (frameHessians.size() == 0)
    return;

  if (!setting_debugout_runquiet)
    printf("LOG %d: %.3f fine. Res: %d A, %d L, %d M; (%'d / %'d) forceDrop. "
           "a=%f, b=%f. Window %d (%d)\n",
           allKeyFramesHistory.back()->id, statistics_lastFineTrackRMSE,
           ef->resInA, ef->resInL, ef->resInM,
           (int)statistics_numForceDroppedResFwd,
           (int)statistics_numForceDroppedResBwd,
           allKeyFramesHistory.back()->aff_g2l.a,
           allKeyFramesHistory.back()->aff_g2l.b,
           frameHessians.back()->shell->id - frameHessians.front()->shell->id,
           (int)frameHessians.size());

  if (!setting_logStuff)
    return;

  if (numsLog != 0) {
    (*numsLog) << allKeyFramesHistory.back()->id << " "
               << statistics_lastFineTrackRMSE << " "
               << (int)statistics_numCreatedPoints << " "
               << (int)statistics_numActivatedPoints << " "
               << (int)statistics_numDroppedPoints << " "
               << (int)statistics_lastNumOptIts << " " << ef->resInA << " "
               << ef->resInL << " " << ef->resInM << " "
               << statistics_numMargResFwd << " " << statistics_numMargResBwd
               << " " << statistics_numForceDroppedResFwd << " "
               << statistics_numForceDroppedResBwd << " "
               << frameHessians.back()->aff_g2l().a << " "
               << frameHessians.back()->aff_g2l().b << " "
               << frameHessians.back()->shell->id -
                      frameHessians.front()->shell->id
               << " " << (int)frameHessians.size() << " "
               << "\n";
    numsLog->flush();
  }
}

cv::Mat FullSystem::getDepthMap(FrameHessian *fh) {
  cv::Mat image = fh->rgb_image;
  cv::Mat invdepth;
  depthPredictor->inference(image, invdepth);
  // depth = 0.3128f / (depth + 0.00001f);
  return invdepth;
}

void FullSystem::dispToDisplay(cv::Mat &disp) {
  assert(!disp.empty());

  double minVal;
  double maxVal;
  cv::minMaxLoc(disp, &minVal, &maxVal);
  disp /= maxVal;
  disp *= 255;

  disp.convertTo(disp, CV_8UC1);
  cv::cvtColor(disp, disp, cv::COLOR_GRAY2BGR);
}

} // namespace dso
