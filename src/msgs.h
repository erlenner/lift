#pragma once

#include "cereal/types/vector.hpp"
#include "utils.h"
#include <vector>

namespace msgs {

struct State {
    int id;
    int pos; // 0 = ground floor, 1 = between ground floor and first floor, 2 = first floor etc.
    int dir; // Takes values of enum Dir int utils.h: -1 = DOWN, 0 = NONE, 1 = UP
    std::vector<Task> tasks; // Task is defined in utils.h
    std::vector<Task> pendingTasks;

    void init(int id, int pos, Dir dir)
    { this->id=id; this->pos=pos; this->dir=static_cast<int>(dir);
      this->tasks=std::vector<Task>(0); this->pendingTasks=std::vector<Task>(0); }

    // boost cereal serialization
    template<class Archive>
    void serialize(Archive& ar){
        ar(id, pos, dir, tasks, pendingTasks);
    }
};


}
