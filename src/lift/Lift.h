#pragma once

#include "config.h"
#include "msgs.h"
#include "utils.h"
#include "mutexctrl.h"

class Lift {

public:

    Lift(){}
    ~Lift(){ stop(); }

    void init();
    void stop();
    int isRunning(){ return running; }

    template <typename CallbackParameter, typename CallbackClass, void(CallbackClass::*callback)(CallbackParameter)>
    void startProcedure(void (Lift::*procedure)(std::function<void(CallbackParameter)>), CallbackClass* callbackInstance);

    void monitorFloorSensor(std::function<void(int)> callback);
    void monitorExternalButtons(std::function<void(Task)> callback);
    void monitorInternalButtons(std::function<void(Task)> callback);

    static void setMotorDirection(int dir);
    static void setButtonLamp(const Task& task, bool onOff);

private:

    int running = 0;
    bool shouldStop = false;
    std::vector<std::thread> threads;


    void startProcedure(void (Lift::*procedure)());

};

template <typename CallbackParameter, typename CallbackClass, void(CallbackClass::*callback)(CallbackParameter)>
void Lift::startProcedure(void (Lift::*procedure)(std::function<void(CallbackParameter)>), CallbackClass* callbackInstance){
    threads.push_back(std::thread(
        [=](){
            (*this.*procedure)(std::bind(callback, callbackInstance, std::placeholders::_1));
            --(this->running);
        }));
    ++running;
}
