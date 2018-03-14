#include "SM.h"
#include "Lift.h"
#include "driver/elev.h"
#include <fstream>

#include <iostream>

void SM::init(int id, msgs::State* states, std::shared_mutex* mutexes){
    this->id = id;
    this->states = states;
    this->mutexes = mutexes;

    for (int i=0; i<config::N_LIFTS; ++i){
        states[i].init(i, -1, UP);
        stateTimestamps[i] = msNow();
    }

    // Read from disk
    std::ifstream is(stateSaveName(), std::ios::binary);
    if (is){
        std::vector<msgs::State> tmp_states;
        cereal::PortableBinaryInputArchive iarchive(is);
        iarchive(tmp_states);

        std::copy(tmp_states.begin(), tmp_states.end(), states);

        for (const auto& task : states[id].tasks)
            Lift::setButtonLamp(task, 1);

        if((states[id].pos % 2) != 0)
            continueTravelling();
        else
            Lift::setMotorDirection(UP);

    }else
        Lift::setMotorDirection(UP);

    pendingTaskMargins = std::vector<int>(states[id].pendingTasks.size(),0);

    time_mutex = new std::shared_mutex;

    pendingTaskWatcher_thread = std::thread(&SM::pendingTaskWatcher, this);
    timeWatcher_thread = std::thread(&SM::timeWatcher, this);
}

void SM::stop(){
    shouldStop=1;
    if(pendingTaskWatcher_thread.joinable())
        pendingTaskWatcher_thread.join();
    if(timeWatcher_thread.joinable())
        timeWatcher_thread.join();
}

void SM::reachFloor(int floor){
    elev_set_floor_indicator(floor);

    writeConcurrent(states[id].pos, 2*floor, mutexes, POS);

    if(shouldWait()){
        elev_set_door_open_lamp(1);
        Lift::setMotorDirection(NONE);
        long inThreeSeconds = msNow() + 3000;
        writeConcurrent(time, inThreeSeconds, time_mutex);
    }
    else
        continueTravelling();
}

// Used for waiting at floors
void SM::timeWatcher(){
    long lastTime = msNow();
    while (!shouldStop){
        long time_copy = readConcurrent(time, time_mutex);
        long newTime = msNow();
        if ((newTime > time_copy) && (lastTime < time_copy))
            continueTravelling();
        lastTime = newTime;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool SM::shouldWait(){

    locker l(mutexes, 0, POS | TASKS);

    bool takeAllTasks = 1;
    for (auto it=states[id].tasks.begin(); it!= states[id].tasks.end(); ++it)
        if((it->floor*2 - states[id].pos) * states[id].dir > 0)
            takeAllTasks = 0;


    for (auto it=states[id].tasks.begin(); it!= states[id].tasks.end(); ++it)
        if((it->floor*2 == states[id].pos) && ((it->dir==states[id].dir) || (it->dir==NONE) || takeAllTasks))
            return 1;
    return 0;
}

void SM::continueTravelling(){
    std::vector<Task> firstTask;

    long readWrite = POS | DIR | TASKS;
    long readOnly = 0;
    lock(mutexes, readWrite, readOnly);

    bool dontKeepGoing = 1;
    for (auto it=states[id].tasks.begin(); it!= states[id].tasks.end(); ++it)
        if((it->floor*2 - states[id].pos) * states[id].dir > 0)
            dontKeepGoing = 0;
        else if(states[id].dir == NONE){
            firstTask.push_back(states[id].tasks[0]);
            break;
        }

    for (auto it=states[id].tasks.begin(); it!=states[id].tasks.end();)
        if((it->floor*2 == states[id].pos) && ((it->dir==states[id].dir) || (it->dir==NONE) || dontKeepGoing)){
            Lift::setButtonLamp(*it, 0);
            it = states[id].tasks.erase(it);
        }
        else
            ++it;

    if (dontKeepGoing){
        if(states[id].tasks.empty())
            states[id].dir = NONE;
        else{
            states[id].dir *= -1;;
            elev_set_door_open_lamp(0);
        }
    }

    if ((states[id].pos % 2) == 0)
        states[id].pos += states[id].dir;
    Lift::setMotorDirection(states[id].dir);

    unlock(mutexes, readWrite, readOnly);

    if (firstTask.size())
        goToTask(firstTask[0]);
}

void SM::addTask(Task task){

    std::cout << "take task: " << task.floor << "," << task.dir << "\n";

    std::vector<Task> firstTask;

    int readWrite = TASKS;
    int readOnly = 0;
    lock(mutexes, readWrite, readOnly);

    if(!contains(states[id].tasks, task)){
        states[id].tasks.push_back(task);
        Lift::setButtonLamp(task, 1);
        if(states[id].tasks.size() == 1)
            firstTask.push_back(states[id].tasks[0]);
    }
    unlock(mutexes, readWrite, readOnly);

    if (firstTask.size())
        goToTask(firstTask[0]);
}

void SM::addPendingTask(Task task){

    locker l(mutexes, PENDING_TASKS, OTHER_STATES);
    if(!contains(states[id].pendingTasks, task)){
        states[id].pendingTasks.push_back(task);
        pendingTaskMargins.push_back(0);
    }
    for (int i=0; i<config::N_LIFTS; ++i)
        std::cout << "fs " << i << ": " << fs(states[i], task) << "\n";
}

void SM::receiveState(msgs::State state){

    std::vector<Task> shouldAddPending;

    long readWrite = TASKS | PENDING_TASKS | OTHER_STATES;
    long readOnly = 0;
    lock(mutexes, readWrite, readOnly);

    for (auto it=state.tasks.begin(); it!=state.tasks.end(); ++it){

        // search for new taken tasks that were pending before
        auto taken = std::find(states[id].pendingTasks.begin(), states[id].pendingTasks.end(), *it);
        if (taken != states[id].pendingTasks.end()){
            std::cout << "taken: id:\t" << state.id << "\ttask:\t" << taken->floor << "," << taken->dir << "\n";
            states[id].pendingTasks.erase(taken);
            int index = taken - states[id].pendingTasks.begin();
            pendingTaskMargins.erase(pendingTaskMargins.begin() + index);
        }

        // look for conflicts in taken tasks
        auto conflict = std::find(states[id].tasks.begin(), states[id].tasks.end(), *it);
        if ((conflict != states[id].tasks.end()) && (conflict->dir != NONE) && (state.id < id)){
            std::cout << "conflict: id:\t" << state.id << "\ttask:\t" << conflict->floor << "," << conflict->dir << "\n";
            Lift::setButtonLamp(*conflict, 0);
            states[id].tasks.erase(conflict);
        }
    }
    for (auto it=state.pendingTasks.begin(); it!= state.pendingTasks.end(); ++it){
        // look for new pending tasks
        bool isNew = !contains(states[id].pendingTasks, *it)
                  && !contains(states[id].tasks, *it)
                  && !contains(states[state.id].pendingTasks, *it);
        for (int i=0; i<config::N_LIFTS; ++i)
            isNew &= (!contains(states[i].tasks, *it) || (msNow() - stateTimestamps[i] > config::DISCONNECTED_TIMEOUT_MS));
        if(isNew)
            shouldAddPending.push_back(*it);
    }
    states[state.id] = state;
    stateTimestamps[state.id] = msNow();

    unlock(mutexes, readWrite, readOnly);

    for (const auto& task : shouldAddPending)
        addPendingTask(task);
}

// Checks if there are pending tasks that this lift should take since the lifts that should really take them has taken too long to take them.
// Also checks if there are disconnected lifts and makes their tasks pending.
void SM::pendingTaskWatcher(){
    while(!shouldStop){
        std::vector<Task> shouldAdd;
        std::vector<Task> shouldAddPending;

        long readWrite = TASKS | PENDING_TASKS | OTHER_STATES;
        long readOnly = POS | DIR;
        lock(mutexes, readWrite, readOnly);

        // look for disconnected lifts and make their tasks pending
        long now = msNow();
        for (int i=0; i<config::N_LIFTS; ++i)
            if ((now - stateTimestamps[i] > config::DISCONNECTED_TIMEOUT_MS) && (i!=id)){
                for (auto it=states[i].tasks.begin(); it!=states[i].tasks.end(); ++it)
                    if (it->dir != NONE)
                        shouldAddPending.push_back(*it);

                if (!shouldAddPending.empty()){
                    std::cout << "lift disconnected: " << i << ":\nnew tasks: ";
                    for (const auto& task : states[i].tasks)
                        std::cout << task.floor << "," << task.dir << "\t";
                    std::cout << "\n";
                }
                states[i].tasks.clear();
            }

        // increment pendingTaskMargins
        for(int& pendingTaskMargin : pendingTaskMargins)
            ++pendingTaskMargin;

        // look for neglected pending tasks
        for(auto it = states[id].pendingTasks.begin(); it!=states[id].pendingTasks.end();){
            int index = it - states[id].pendingTasks.begin();
            int myFs = fs(states[id], *it);
            bool shouldTake = 1;

            for(int i=0; i<config::N_LIFTS; ++i){
                if (states[i].id == id)
                    continue;

                int otherFs = fs(states[i], *it);
                int margin = pendingTaskMargins[index] / 30;
                if((myFs-otherFs >= margin) && ((myFs-otherFs!= margin) || (id>states[i].id)))
                    shouldTake = 0;
            }
            if (shouldTake){
                shouldAdd.push_back(*it);
                it = states[id].pendingTasks.erase(it);
                pendingTaskMargins.erase(pendingTaskMargins.begin() + index);
            }
            else
                ++it;
        }
        unlock(mutexes, readWrite, readOnly);

        for (const auto& task : shouldAdd)
            addTask(task);
        for (const auto& task : shouldAddPending)
            addPendingTask(task);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void SM::goToTask(const Task& task){
    bool alreadyThere = 0;

    long readWrite = POS | DIR;
    long readOnly = 0;

    lock(mutexes, readWrite, readOnly);

    if((states[id].pos == -1) || (states[id].dir != NONE))
        return;

    int dir = task.floor*2 - states[id].pos;

    if(dir == 0)
        alreadyThere = 1;
    else{
        dir /= abs(dir);
        elev_set_door_open_lamp(0);
    }
    states[id].dir = dir;
    if (states[id].pos % 2 == 0)
        states[id].pos += dir;

    Lift::setMotorDirection(dir);
    int pos_copy = states[id].pos;

    unlock(mutexes, readWrite, readOnly);

    if (alreadyThere)
        reachFloor(pos_copy/2);
}

int SM::fs(const msgs::State& state, const Task& pendingTask){
    int diffPos = abs(state.pos - 2*pendingTask.floor);
    int stateDir = state.dir;
    if ((pendingTask.floor*2 - state.pos) * stateDir < 0){
        if(stateDir == UP)
            diffPos = 4*N_FLOORS - state.pos - 2*pendingTask.floor;
        else if(stateDir == DOWN)
            diffPos = state.pos + 2*pendingTask.floor;
        stateDir *= -1;
    }
    bool disConnected = 0;
    if (((msNow() - stateTimestamps[state.id]) > config::DISCONNECTED_TIMEOUT_MS) && (state.id != id))
        disConnected = 1;
    return (((stateDir || pendingTask.dir) && (stateDir != pendingTask.dir)) * config::INCORRECT_DIRECTION_PENALTY
            + diffPos * config::POSITION_DIFFERENCE_PENALTY
            + disConnected * config::DISCONNECTED_PENALTY);
}
