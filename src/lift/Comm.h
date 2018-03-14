#pragma once

#include "config.h"
#include "msgs.h"
#include "utils.h"
#include "mutexctrl.h"

class Comm {

public:

    Comm(){}
    ~Comm(){ stop(); }

    void init(int id, msgs::State* states, std::shared_mutex* mutexes);
    void stop();
    int isRunning(){ return running; }

    template <typename CallbackParameter, typename CallbackClass, void(CallbackClass::*callback)(CallbackParameter)>
    void startProcedure(void (Comm::*procedure)(std::function<void(CallbackParameter)>), CallbackClass* callbackInstance);

    void receiveState(std::function<void(msgs::State)> callback);

private:

    int id;
    msgs::State* states;

    std::shared_mutex* mutexes;

    int running = 0;
    bool shouldStop = false;
    std::vector<std::thread> threads;


    void startProcedure(void (Comm::*procedure)());

    void sendState();
};

template <typename CallbackParameter, typename CallbackClass, void(CallbackClass::*callback)(CallbackParameter)>
void Comm::startProcedure(void (Comm::*procedure)(std::function<void(CallbackParameter)>), CallbackClass* callbackInstance){
    threads.push_back(std::thread(
        [=](){
            (*this.*procedure)(std::bind(callback, callbackInstance, std::placeholders::_1));
            --(this->running);
        }));
    ++running;
}
