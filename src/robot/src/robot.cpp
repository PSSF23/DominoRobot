#include "robot.h"

#include <plog/Log.h> 


Robot::Robot()
: statusUpdater(),
  server(statusUpdater),
  controller(statusUpdater),
  tray_controller(),
  mm_wrapper(),
  loop_time_averager(20),
  position_time_averager(20)
{
    PLOGI.printf("Robot starting");
}

void Robot::run()
{
    COMMAND curCmd = COMMAND::NONE;

    while(true)
    {
        // Check for new command and try to start it
        COMMAND newCmd = server.oneLoop();
        bool status = tryStartNewCmd(newCmd);

        // Update our current command if we successfully started a new command
        if(status)
        {
            curCmd = newCmd;
            statusUpdater.updateInProgress(true);
        }

        // Service marvelmind
        std::vector<float> positions = mm_wrapper.getPositions();
        if (positions.size() == 3)
        {
            position_time_averager.mark_point();
            controller.inputPosition(positions[0], positions[1], positions[2]);
        }

        // Service controllers
        controller.update();
        tray_controller.update();

        // Check if the current command has finished
        bool done = checkForCmdComplete(curCmd);
        if(done)
        {
            curCmd = COMMAND::NONE;
            statusUpdater.updateInProgress(false);
        }

        // Update loop time and status updater
        loop_time_averager.mark_point();
        statusUpdater.updateLoopTimes(loop_time_averager.get_ms(), position_time_averager.get_ms());
    }
}


bool Robot::tryStartNewCmd(COMMAND cmd)
{
    // Position info doesn't count as a real 'command' since it doesn't interrupt anything
    // Always service it, but don't consider it starting a new command
    if (cmd == COMMAND::POSITION)
    {
        RobotServer::PositionData data = server.getPositionData();
        controller.inputPosition(data.x, data.y, data.a);

        // Update the position rate
        position_time_averager.mark_point();

        return false;
    }
    // Same with ESTOP
    if (cmd == COMMAND::ESTOP)
    {
        controller.estop();
        tray_controller.estop();
        return false;
    }
    // Same with LOAD_COMPLETE
    if (cmd == COMMAND::LOAD_COMPLETE)
    {
        tray_controller.setLoadComplete();
        return false;
    }
    
    // For all other commands, we need to make sure we aren't doing anything else at the moment
    if(statusUpdater.getInProgress())
    {
        PLOGW.printf("Command already running, rejecting new command");
        return false;
    }
    
    // Start new command
    if(cmd == COMMAND::MOVE)
    {
        RobotServer::PositionData data = server.getMoveData();
        controller.moveToPosition(data.x, data.y, data.a);
    }
    else if(cmd == COMMAND::MOVE_REL)
    {
        RobotServer::PositionData data = server.getMoveData();
        controller.moveToPositionRelative(data.x, data.y, data.a);
    }
    else if(cmd == COMMAND::MOVE_FINE)
    {
        RobotServer::PositionData data = server.getMoveData();
        controller.moveToPositionFine(data.x, data.y, data.a);
    }
    else if(cmd == COMMAND::MOVE_CONST_VEL)
    {
        RobotServer::VelocityData data = server.getVelocityData();
        controller.moveConstVel(data.vx, data.vy, data.va, data.t);
    }
    else if(cmd == COMMAND::PLACE_TRAY)
    {
        tray_controller.place();
    }
    else if(cmd == COMMAND::LOAD_TRAY)
    {
        tray_controller.load();
    }
    else if(cmd == COMMAND::INITIALIZE_TRAY)
    {
        tray_controller.initialize();
    }
    else if (cmd == COMMAND::NONE)
    {
      // do nothing...
    }
    else
    {
        PLOGW.printf("Unknown command!");
        return false;
    }

    return true;
}

bool Robot::checkForCmdComplete(COMMAND cmd)
{
    if (cmd == COMMAND::NONE)
    {
        return true;
    }
    else if(cmd == COMMAND::MOVE || 
            cmd == COMMAND::MOVE_REL ||
            cmd == COMMAND::MOVE_FINE ||
            cmd == COMMAND::MOVE_CONST_VEL)
    {
        return controller.isTrajectoryRunning();
    }
    else if(cmd == COMMAND::PLACE_TRAY ||
            cmd == COMMAND::LOAD_TRAY ||
            cmd == COMMAND::INITIALIZE_TRAY)
    {
        return tray_controller.isActionRunning();
    }
    else
    {
        PLOGI.printf("Completion check not implimented for command: %i",cmd);
        return true;
    }
        
}
