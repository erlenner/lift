#include "Comm.h"

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iterator>

void Comm::init(int id, msgs::State* states, std::shared_mutex* mutexes){
    this->id = id;
    this->states = states;
    this->mutexes = mutexes;

    startProcedure(&Comm::sendState);
}

void Comm::stop(){
    shouldStop = true;
    for (auto it=threads.begin();it!=threads.end();++it)
        if (it->joinable()) it->join();
    shouldStop = false;
}

void Comm::startProcedure(void (Comm::*procedure)()){
    threads.push_back(std::thread(
        [=](){
            (*this.*procedure)();
            --(this->running);
        }));
    ++running;
}

void Comm::sendState(){
    int sendStateSocket = createSocket(0);
    if (sendStateSocket < 0)
      std::cerr << "Could not open sendStateSocket" << std::endl;
    // Setup lift IPs
    std::vector<in_addr> liftIPs;
    int r=0;
    for (int i=0; i<config::N_LIFTS-1; ++i, ++r){
        if(r==id) ++r;
        struct in_addr addr = {0};
        inet_pton(AF_INET, config::IPS[r], &(addr));
        liftIPs.push_back(addr);

        char c[INET_ADDRSTRLEN];  // space to hold the IPv4 string
        inet_ntop(AF_INET, &addr, c, INET_ADDRSTRLEN);
        std::cout << "Lift " << r << ": " << std::string(c) << ":" << config::STATE_PORT+r << std::endl;
    }

    //std::cout << "sendState running" << std::endl;
    while(!shouldStop){
        // Send state
        int r=0;
        for (unsigned i=0; i<liftIPs.size(); ++i, ++r){
            if(r==id) ++r;
            msgs::State msg;

            // Get states
            long readWrite = 0;
            long readOnly = POS | DIR | TASKS | PENDING_TASKS | OTHER_STATES;
            lock(mutexes, readWrite, readOnly);

            std::vector<msgs::State> states_copy(states, states + config::N_LIFTS);

            unlock(mutexes, readWrite, readOnly);

            // Serialize all states and save to disk
            std::ofstream os(stateSaveName(), std::ios::binary);
            cereal::PortableBinaryOutputArchive statesArchive(os);
            statesArchive(states_copy);


            // Serialize state
            std::stringstream buff;
            cereal::PortableBinaryOutputArchive stateArchive(buff);
            stateArchive(states_copy[id]);


            // Send state
            struct sockaddr_in addr = {0};
            addr.sin_family = AF_INET;
            addr.sin_addr = liftIPs[i];
            addr.sin_port = htons(config::STATE_PORT+r);
            int bytesSent = sendto(sendStateSocket, buff.str().c_str(), buff.str().size(), 0, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
            if (bytesSent <= 0)
                std::cerr << "Failed to send state" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    close(sendStateSocket);
}

void Comm::receiveState(std::function<void(msgs::State)> callback){
    // receive buffer
    char buff[1000];
    // Setup socket
    int receiveStateSocket= createSocket(config::STATE_PORT+id);
    if (receiveStateSocket < 0)
      std::cerr << "Could not open receiveStateSocket" << std::endl;
    while (!shouldStop){
        // Recv state
        if (shouldRead(receiveStateSocket)){
            int bytesReceived = recv(receiveStateSocket, (void*)buff, sizeof(buff), 0);
            if (bytesReceived > 0){
                std::string s(buff, bytesReceived);
                std::stringstream b(s);
                cereal::PortableBinaryInputArchive iarchive(b);

                msgs::State msg;
                iarchive(msg);

                if (msg.id==id)
                    { std::cerr << "There are more than one lift with id " << id << "\n"; return; }

                callback(msg);

            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    close(receiveStateSocket);
}
