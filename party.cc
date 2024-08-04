// This file contains the implementation of the Party methods.

#include "party.hh"

using namespace std;

string Party::meet(string &my_name, int my_sign, int other_sign)
{
    unique_lock<mutex> lock(mutex_);

    // initialize guest struct for my_name
    string match_name = "";
    bool status = false;
    condition_variable_any cv_;
    Guest my = {my_name, &match_name, &status, &cv_};
    
    // take match off queue if available
    queue<Guest> *matchQueue = &guestsWaiting[other_sign][my_sign];
    if (!matchQueue->empty()) {
        Guest other_guest = matchQueue->front();
        matchQueue->pop();

        // update match with other guest's name
        *my.match = other_guest.name;
        *my.isMatched = true;
 
        // update guest's match with my_name
        *other_guest.match = my_name;
        *other_guest.isMatched = true;
        other_guest.match_found->notify_all();

        return *my.match;
    }

    // if no matches, add guest to respective queue in 2d array
    guestsWaiting[my_sign][other_sign].push(my);
    while (!*my.isMatched) {
        my.match_found->wait(lock);
    }

    return *my.match;
}
