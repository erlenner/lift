#include "config.h"
#include "msgs.h"
#include "utils.h"
#include "mutexctrl.h"

#include "Lift.h"
#include "Comm.h"
#include "SM.h"

#include <iostream>
#include <signal.h>
#include <unistd.h>

Lift lift;
Comm comm;
SM sm;

int id;
msgs::State states[config::N_LIFTS];

std::shared_mutex mutexes[N_MUTEXES];


bool shouldStop = 0;

void cleanup(int signum){
    comm.stop();
    sm.stop();
    lift.stop();

    shouldStop=1;
}

int main(int argc, char **argv){

    stateSaveName(argv[0]); // save executable relative path

    if (argc > 1)
        id = std::stoi(std::string(argv[1]));
    else {
        std::cerr << "usage: " << std::string(argv[0]) << " id" << std::endl;
        return -1;
    }

    std::cout << "Lift ID = " << id << std::endl;

    lift.init();
    sm.init(id, states, mutexes);
    comm.init(id, states, mutexes);

    lift.startProcedure<int, SM, &SM::reachFloor>(&Lift::monitorFloorSensor, &sm);
    lift.startProcedure<Task, SM, &SM::addTask>(&Lift::monitorInternalButtons, &sm);
    lift.startProcedure<Task, SM, &SM::addPendingTask>(&Lift::monitorExternalButtons, &sm);

    comm.startProcedure<msgs::State, SM, &SM::receiveState>(&Comm::receiveState, &sm);

    signal(SIGINT, cleanup);

    while(!shouldStop)
        usleep(100);

    return 0;
}
