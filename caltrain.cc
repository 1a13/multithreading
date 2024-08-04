// This file contains the implementation of the Caltrain methods.

#include "caltrain.hh"

using namespace std;

Station::Station()
{
    seatsAvailable = 0;
    numWaiting = 0;
    boarding = 0;
}

void Station::load_train(int available)
{
    unique_lock<mutex> lock(mutex_);
    seatsAvailable = available;

    // let passengers on board
    if (seatsAvailable > 0) {
        trainArrived.notify_all();
    }

    // wait until train is fully loaded
    while (seatsAvailable > 0 && numWaiting > 0) {
        trainLeaving.wait(lock);
    }

    seatsAvailable = 0;
}

void Station::wait_for_train()
{
    unique_lock<mutex> lock(mutex_);
    numWaiting++;

    // wait until there are seats available
    while (seatsAvailable == 0) {
        trainArrived.wait(lock);
    }
    seatsAvailable--;
    numWaiting--;
    boarding++;
}

void Station::boarded()
{
    unique_lock<mutex> lock(mutex_);
    boarding--;

    // train leaves when everyone is seated
    if (boarding == 0) {
        trainLeaving.notify_all();
    }
}
