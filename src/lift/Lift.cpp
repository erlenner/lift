#include "Lift.h"
#include "driver/elev.h"

#include <iostream>

void Lift::init(){

    elev_init();
}

void Lift::stop(){
    shouldStop = true;
    for (auto it=threads.begin();it!=threads.end();++it)
        if (it->joinable()) it->join();
    shouldStop = false;
    setMotorDirection(NONE);
}

void Lift::startProcedure(void (Lift::*procedure)()){
    threads.push_back(std::thread(
        [=](){
            (*this.*procedure)();
            --(this->running);
        }));
    ++running;
}


void Lift::monitorExternalButtons(std::function<void(Task)> callback){
    int lastUpSignals[N_FLOORS];
    int lastDownSignals[N_FLOORS];

    std::fill_n(lastUpSignals,N_FLOORS,0);
    std::fill_n(lastDownSignals,N_FLOORS,0);

    while (!shouldStop){
        for (int i=0;i<N_FLOORS-1;++i){
            int newSignal = elev_get_button_signal(BUTTON_CALL_UP,i);
            if(newSignal==1 && lastUpSignals[i]==0)
                callback(Task(UP,i));
            lastUpSignals[i] = newSignal;
        }
        for (int i=1;i<N_FLOORS;++i){
            int newSignal = elev_get_button_signal(BUTTON_CALL_DOWN,i);
            if(newSignal==1 && lastDownSignals[i]==0)
                callback(Task(DOWN,i));
            lastDownSignals[i] = newSignal;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    --running;
}

void Lift::monitorInternalButtons(std::function<void(Task)> callback){
    int lastFloorSignals [N_FLOORS];

    std::fill_n(lastFloorSignals,N_FLOORS,0);

    while (!shouldStop){
        for (int i=0;i<N_FLOORS;++i){
            int newSignal = elev_get_button_signal(BUTTON_COMMAND,i);
            if(newSignal==1 && lastFloorSignals[i]==0)
                callback(Task(NONE,i));
            lastFloorSignals[i] = newSignal;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    --running;
}

void Lift::monitorFloorSensor(std::function<void(int)> callback){
    int lastFloorSensorSignal = -1;

    while (!shouldStop){
        int newSignal = elev_get_floor_sensor_signal();
        if (newSignal>-1 && lastFloorSensorSignal==-1)
            callback(newSignal);
        lastFloorSensorSignal = newSignal;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    --running;
}

void Lift::setMotorDirection(int dir){
    Dir _dir = static_cast<Dir>(dir);
    switch(_dir){
        case DOWN:
            elev_set_motor_direction(DIRN_DOWN);
            break;
        case NONE:
            elev_set_motor_direction(DIRN_STOP);
            break;
        case UP:
            elev_set_motor_direction(DIRN_UP);
            break;
    }
}

void Lift::setButtonLamp(const Task& task, bool onOff){
    switch (task.dir){
        case UP:
            elev_set_button_lamp(BUTTON_CALL_UP, task.floor, onOff);
            break;
        case NONE:
            elev_set_button_lamp(BUTTON_COMMAND, task.floor, onOff);
            break;
        case DOWN:
            elev_set_button_lamp(BUTTON_CALL_DOWN, task.floor, onOff);
            break;
    }
}
