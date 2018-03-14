#pragma once

#include "config.h"
#include "msgs.h"
#include "utils.h"
#include "mutexctrl.h"

class SM {

public:

    SM(){}
    ~SM(){stop();delete time_mutex;}

    void init(int id, msgs::State* states, std::shared_mutex* mutexes);

    void reachFloor(int floor);
    void receiveState(msgs::State state);
    void addTask(Task task);
    void addPendingTask(Task task);

    void stop();

private:

    int id;
    msgs::State* states;
    long stateTimestamps[config::N_LIFTS]; // The last time the other lifts sent their state to us.
    std::vector<int> pendingTaskMargins; // These margins determine how big of a gap between the score of our lift and other lifts that will be ignored when choosing whether to take a pending task
    std::thread pendingTaskWatcher_thread;
    std::thread timeWatcher_thread;
    bool shouldStop = 0;
    long time = 0;

    std::shared_mutex* mutexes;
    std::shared_mutex otherStates_mutex;
    std::shared_mutex* time_mutex;

    // Checks if there are pending tasks that this lift should take since the lifts that should really take them has taken too long to take them.
    // Also checks if there are disconnected lifts and makes their tasks pending.
    void pendingTaskWatcher();

    void timeWatcher(); // Used for waiting at floors

    bool shouldWait();
    void continueTravelling();

    void goToTask(const Task& task);

    int fs(const msgs::State& state, const Task& pendingTask); // The FS (Figure of Suitability) is the score of a lift with respect to a task. The lift with the lowest FS should take the task
};
