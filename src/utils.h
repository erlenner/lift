#pragma once

#include "cereal/archives/portable_binary.hpp"
#include <thread>
#include <string>
#include <limits.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>
#include "sys/time.h"
#include "config.h"


inline int createSocket(int port){

    struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0){
        return -1;
    }

	if (bind(s, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1){
		return -1;
	}

	return s;
}

inline bool shouldRead(int s){
	struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 1000;
    fd_set f;
	FD_ZERO(&f);
	FD_SET(s, &f);
	int e = select(s+1, &f, NULL, NULL, &t);
	switch (e) {
	case -1:
		return false;
	case 0:
		return false;
	default:
        if (FD_ISSET(s, &f)){
            return true;
        }
	}
	return false;
}

enum Dir : int {DOWN = -1, NONE, UP};

struct Task{
    int dir;
    int floor;

    Task(Dir dir, int floor)
        : dir(static_cast<int>(dir)), floor(floor) {}
    Task()
        : Task(NONE, -1) {}

    friend bool operator==(const Task& lhs, const Task& rhs)
    { return ((lhs.dir==rhs.dir)&&(lhs.floor==rhs.floor)); }
};

namespace cereal{

    template<class Archive>
    void save(Archive & ar, Task const& task)
    { ar(task.dir, task.floor); }

    template<class Archive>
    void load(Archive & ar, Task& task)
    { ar(task.dir, task.floor); }
}

template<typename T>
inline bool contains(const std::vector<T>& vec, T value){
    return (std::find(vec.begin(),vec.end(),value)!=vec.end());
}

inline long msNow(){
    struct timeval tp;
    gettimeofday(&tp, NULL);

    long ms = tp.tv_sec*1000 + tp.tv_usec/1000;
    return ms;
}

inline std::string stateSaveName(const char* argv0 = NULL){
    static std::string exeRelativeDir(argv0);
    if (argv0 != NULL)
        exeRelativeDir = exeRelativeDir.substr(0, exeRelativeDir.find_last_of("\\/"));

    char tmpPath[PATH_MAX];
    getcwd(tmpPath, PATH_MAX-1);

    std::string absoluteToHere(tmpPath);
    std::string slash("/");

    return absoluteToHere + slash + exeRelativeDir + slash + std::string(config::STATE_SAVE_NAME);
}
