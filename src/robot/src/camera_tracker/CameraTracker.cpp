#include "CameraTracker.h"

#include <plog/Log.h>
#include "constants.h"


CameraTracker::CameraTracker(bool start_thread)
: camera_loop_time_averager_(10),
  rear_cam_(CAMERA_ID::REAR, start_thread),
  side_cam_(CAMERA_ID::SIDE, start_thread),
  output_({{0,0,0}, false, ClockFactory::getFactoryInstance()->get_clock()->now()}),
  last_rear_cam_output_(),
  last_side_cam_output_(),
  side_cam_ok_filter_(0.5),
  rear_cam_ok_filter_(0.5),
  both_cams_ok_filter_(0.5)
{
    // Target points
    robot_P_side_target_ = {cfg.lookup("vision_tracker.physical.side.target_x"), 
                            cfg.lookup("vision_tracker.physical.side.target_y")};
    robot_P_rear_target_ = {cfg.lookup("vision_tracker.physical.rear.target_x"), 
                            cfg.lookup("vision_tracker.physical.rear.target_y")};
}

CameraTracker::~CameraTracker() {}

void CameraTracker::update()
{
    CameraPipelineOutput side_output = side_cam_.getData();
    CameraPipelineOutput rear_output = rear_cam_.getData();
    debug_.side_ok = side_cam_ok_filter_.update(side_output.ok);
    debug_.rear_ok = rear_cam_ok_filter_.update(rear_output.ok);
    bool new_output_pose_ready = false;

    if(rear_output.ok)
    {
        last_rear_cam_output_ = rear_output;
        // PLOGI << "Got valid rear output";
    }
    if(side_output.ok)
    {
        last_side_cam_output_ = side_output;
        // PLOGI << "Got valid side output";
    }

    if(last_side_cam_output_.ok && last_rear_cam_output_.ok) new_output_pose_ready = true;


    // int time_delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>
    //     (last_side_cam_output_.timestamp - last_rear_cam_output_.timestamp).count();
    // if(abs(time_delta_ms) > 400)
    // {
    //     PLOGW << "Camera timestamp delta too large: " << time_delta_ms;
    //     new_output_pose_ready = false;
    // }


    output_.ok = both_cams_ok_filter_.update(new_output_pose_ready);
    debug_.both_ok = output_.ok;
    if(!new_output_pose_ready) return;

    // Populate output
    output_.pose = computeRobotPoseFromImagePoints(last_side_cam_output_.point, last_rear_cam_output_.point);
    output_.timestamp = ClockFactory::getFactoryInstance()->get_clock()->now();
    camera_loop_time_averager_.mark_point();

    // Populate debug
    debug_.side_u = last_side_cam_output_.uv[0];
    debug_.side_v = last_side_cam_output_.uv[1];
    debug_.rear_u = last_rear_cam_output_.uv[0];
    debug_.rear_v = last_rear_cam_output_.uv[1];
    debug_.side_x = last_side_cam_output_.point[0];
    debug_.side_y = last_side_cam_output_.point[1];
    debug_.rear_x = last_rear_cam_output_.point[0];
    debug_.rear_y = last_rear_cam_output_.point[1];
    debug_.pose_x = output_.pose.x;
    debug_.pose_y = output_.pose.y;
    debug_.pose_a = output_.pose.a;
    debug_.loop_ms = camera_loop_time_averager_.get_ms();

    // Mark camera output values as not okay to make sure they are only used once
    last_rear_cam_output_.ok = false;
    last_side_cam_output_.ok = false;
}

CameraTrackerOutput CameraTracker::getPoseFromCamera()
{
    return output_;
}

CameraDebug CameraTracker::getCameraDebug()
{
    return debug_;
}

void CameraTracker::start()
{
    rear_cam_.start();
    side_cam_.start();
}

void CameraTracker::stop()
{
    rear_cam_.stop();
    side_cam_.stop();
}


void CameraTracker::test_function()
{
    // if (isRunning()) 
    // {
    //     PLOGE << "Skipping test function while background thread is running";
    //     return;
    // }
    
    // cv::Mat frame;
    // Timer t0;
    // {
    //     Timer t1;
    //     if(!use_debug_image_) cameras_[CAMERA_ID::SIDE].capture >> frame;
    //     PLOGI << "Side camera frame took " << t1.dt_ms() << "ms to get";
    //     std::vector<cv::KeyPoint> keypoints = allKeypointsInImage(frame, CAMERA_ID::SIDE, true);
    //     cv::Point2f best_point_px = getBestKeypoint(keypoints);
    //     Eigen::Vector2f best_point_m = cameraToRobot(best_point_px, CAMERA_ID::SIDE);
    //     PLOGI << "Best keypoint side at " << best_point_px << " px";
    //     PLOGI << "Best keypoint side at " << best_point_m << " m";
    //     PLOGI << "Side camera took " << t1.dt_ms() << "ms to run";
    // }
    // {
    //     Timer t1;
    //     if(!use_debug_image_) cameras_[CAMERA_ID::REAR].capture >> frame;
    //     PLOGI << "Rear camera frame took " << t1.dt_ms() << "ms to get";
    //     std::vector<cv::KeyPoint> keypoints = allKeypointsInImage(frame, CAMERA_ID::REAR, true);
    //     cv::Point2f best_point_px = getBestKeypoint(keypoints);
    //     Eigen::Vector2f best_point_m = cameraToRobot(best_point_px, CAMERA_ID::REAR);
    //     PLOGI << "Best keypoint rear at " << best_point_px << " px";
    //     PLOGI << "Best keypoint rear at " << best_point_m << " m";
    //     PLOGI << "Rear camera took " << t1.dt_ms() << "ms to run";
    // }

    // PLOGI << "Done with image processing";
    // PLOGI << "Total image processing time: " << t0.dt_ms() << "ms";
}



Point CameraTracker::computeRobotPoseFromImagePoints(Eigen::Vector2f p_side, Eigen::Vector2f p_rear)
{  
    // Solve linear equations to get pose values
    Eigen::Matrix4f A;
    Eigen::Vector4f b;
    A << 1, 0, -p_rear[1], p_rear[0],
         0, 1,  p_rear[0], p_rear[1],
         1, 0, -p_side[1], p_side[0],
         0, 1,  p_side[0], p_side[1];
    b << robot_P_rear_target_[0],
         robot_P_rear_target_[1],
         robot_P_side_target_[0],
         robot_P_side_target_[1];
    Eigen::Vector4f x = A.colPivHouseholderQr().solve(b);
    
    // Populate pose with angle averaging
    float pose_x = x[0];
    float pose_y = x[1];
    float pose_a = atan2(x[2], x[3]);
    return {pose_x, pose_y, pose_a};
}

