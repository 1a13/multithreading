// This class models a Caltrain station that coordinates trains and loading
// passengers.  Trains can arrive at the station to be loaded; passengers
// wait for trains before boarding, and indicate once they are boarded.

#ifndef CALTRAIN_H
#define CALTRAIN_H

#include <condition_variable>
#include <mutex>

class Station {
public:
    Station();
    
    // Called when a train arrives in the station and has opened its doors.
    // available indicates how many seats are currently free on the train.
    // This method does not return until the train is satisfactorily loaded
    // (all new passengers boarded, and either the train is full or there
    // are no waiting passengers).
    void load_train(int available);
    
    // Invoked when a passenger arrives in the station. This method does
    // not return until a train is in the station (i.e., a call to load_train
    // is in progress) and there are enough free seats on the train to
    // accommodate this passenger. Once this method returns, the passenger
    // can begin boarding.
    void wait_for_train();
    
    // Invoked by each passenger once they have successfully boarded the train.
    void boarded();
    
private:
    // Synchronizes access to all information in this object.
    std::mutex mutex_;

    std::condition_variable_any trainArrived;
    std::condition_variable_any trainLeaving;
    int seatsAvailable;
    int numWaiting;
    int boarding;
};

#endif /* CALTRAIN_H */
