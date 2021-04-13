#ifndef RobotControllerModeVision_h
#define RobotControllerModeVision_h

#include "RobotControllerModeBase.h"
#include "SmoothTrajectoryGenerator.h"
#include "utils.h"
#include "camera_tracker/CameraTracker.h"
#include "KalmanFilter.h"

class RobotControllerModeVision : public RobotControllerModeBase
{

  public:

    RobotControllerModeVision(bool fake_perfect_motion);

    bool startMove(Point target_point);

    virtual Velocity computeTargetVelocity(Point current_position, Velocity current_velocity, bool log_this_cycle) override;

    virtual bool checkForMoveComplete(Point current_position, Velocity current_velocity) override;

  protected:


    SmoothTrajectoryGenerator traj_gen_; 
    Point goal_point_;
    Point current_point_;
    PVTPoint current_target_;
    CameraTrackerBase* camera_tracker_;
    Timer traj_done_timer_;

    TrajectoryTolerances tolerances_;

    PositionController x_controller_;
    PositionController y_controller_;
    PositionController a_controller_;    
    KalmanFilter kf_;

};

#endif //RobotControllerModeVision_h