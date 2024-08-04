/*
 * This file tests the implementation of the Station class in caltrain.cc. You
 * shouldn't need to modify this file (and we will test your code against
 * an unmodified version). Please report any bugs to the course staff.
 *
 * Note that passing these tests doesn't guarantee that your code is correct
 * or meets the specifications given, but hopefully it's at least pretty
 * close.
 */

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <functional>
#include <iostream>
#include <thread>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <map>

#include "caltrain.hh"

using namespace std;

// Interval for nanosleep corresponding to 1 ms.
struct timespec one_ms = {.tv_sec = 0, .tv_nsec = 1000000};

// Total number of passengers that are about to call wait_for_train.
std::atomic<int> passengers_arrived;

// Total number of trains that are about to call load_train
std::atomic<int> trains_arrived;

/// This function is used by tests to invoke wait_for_train in
/// a separate std::thread (so many passengers can be waiting at once).
/// This std::thread does *not* invoke boarded (even though this would
/// "normally" happen in the passenger std::thread); instead, boarded())
/// is invoked in the main std::thread, so it can precisely sequence the
/// calls to produce desired test scenarios.
void passenger(Station& station, atomic<int>& boarding_threads)
{
    passengers_arrived++;
    station.wait_for_train();
    boarding_threads++;
}

/// Runs in a separate std::thread to simulate the arrival of a train.
/// \param station
///      Station where the train is arriving.
/// \param free_seats
///      Number of seats available on the train.
void train(Station& station, int free_seats, atomic<int>& loaded_trains)
{
    trains_arrived++;
    station.load_train(free_seats);
    loaded_trains++;
}

/// Wait for an atomic variable to be a given value.
/// \param var
///      The variable to watch.
/// \param count
///      Wait until @var is the value.
/// \param ms
///      Return after this many milliseconds even if @var hasn't
///      reached the desired value.
/// \return
///      True means the function succeeded, false means it failed.
bool wait_for(atomic<int>& var, int count, int ms)
{
    while (true) {
        if (var.load() == count) {
            return true;
        }
        if (ms <= 0) {
            return false;
        }
        nanosleep(&one_ms, nullptr);
        ms -= 1;
    }
}

// Trains arrive (both with and without space), but no passengers are waiting.
void no_waiting_passengers(void)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished).
     */
    atomic<int> loaded_trains = 0;

    // Spawn a full train to enter the station
    cout << "Full train arrives with no waiting passengers" << endl;
    thread train1(train, ref(station), 0, ref(loaded_trains));
    train1.detach(); // so we don't have to call join
    while (trains_arrived != 1) /* Do nothing */;

    // We should immediately see loaded_trains become 1
    if (!wait_for(loaded_trains, 1, 100)) {
        cout << "Error: load_train didn't return immediately" << endl;
        exit(1);
    } else {
        cout << "load_train returned" << endl;
    }

    // Spawn a train with 10 seats to enter the station
    cout << "Train with 10 seats arrives with no waiting passengers" << endl;
    thread train2(train, ref(station), 10, ref(loaded_trains));
    train2.detach(); // so we don't have to call join
    while (trains_arrived != 2) /* Do nothing */;

    // We should immediately see loaded_trains become 2
    if (!wait_for(loaded_trains, 2, 100)) {
        cout << "Error: load_train didn't return immediately" << endl;
        exit(1);
    } else {
        cout << "load_train returned" << endl;
    }
}

/* A passenger arrives, followed by a train with space available.
 * This test does NOT check that load_train blocks until the passengers board -
 * even if load_train returns immediately, this test will still pass.
 */
void passenger_wait_for_train(void)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished) and number of passengers that have started
     * boarding (i.e. wait_for_train finished).
     */
    atomic<int> loaded_trains = 0;
    atomic<int> boarding_threads = 0;

    cout << "Passenger arrives, begins waiting" << endl;
    thread pass1(passenger, ref(station), ref(boarding_threads));
    pass1.detach(); // so we don't have to call join
    while (passengers_arrived != 1) /* Do nothing */;

    // Wait to see if the passenger incorrectly boards
    if (wait_for(boarding_threads, 1, 1000)) {
        cout << "Error: passenger returned from wait_for_train before"
        << " train arrived" << endl;
        return;
    }

    // Spawn a train with 3 seats to enter the station
    cout << "Train arrives with 3 seats available" << endl;
    thread train1(train, ref(station), 3, ref(loaded_trains));
    train1.detach(); // so we don't have to call join
    while (trains_arrived != 1) /* Do nothing */;

    // We should immediately see boarding_threads become 1
    if (wait_for(boarding_threads, 1, 100)) {
        cout << "Passenger started boarding" << endl;
    } else {
        cout << "Error: passenger didn't return from wait_for_train" << endl;
        return;
    }

    // Finish boarding passenger.
    cout << "Passenger finished boarding" << endl;
    station.boarded();

    // Give the train a chance to depart (but for this test don't
    // record an error if it doesn't). If we don't wait for the train
    // to depart, a segfault could occur (the Station object might get
    // deleted while the train is still accessing it).
    wait_for(loaded_trains, 1, 1000);
}

/* A passenger arrives, followed by a train with space available.
 * This test checks to make sure that the train doesn't depart until
 * the passenger has boarded.
 */
void train_wait_until_boarded(void)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished) and number of passengers that have started
     * boarding (i.e. wait_for_train finished).
     */
    atomic<int> loaded_trains = 0;
    atomic<int> boarding_threads = 0;

    // A passenger arrives and waits
    cout << "Passenger arrives, begins waiting" << endl;
    thread pass1(passenger, ref(station), ref(boarding_threads));
    pass1.detach(); // so we don't have to call join
    while (passengers_arrived != 1) /* Do nothing */;

    // Wait to see if the passenger incorrectly boards
    if (wait_for(boarding_threads, 1, 1000)) {
        cout << "Error: passenger returned from wait_for_train when"
        << " no empty seats were available" << endl;
        return;
    }

    // Spawn a train with 3 seats to enter the station
    cout << "Train arrives with 3 seats available" << endl;
    thread train1(train, ref(station), 3, ref(loaded_trains));
    train1.detach(); // so we don't have to call join
    while (trains_arrived != 1) /* Do nothing */;

    // We should immediately see boarding_threads become 1
    if (wait_for(boarding_threads, 1, 100)) {
        cout << "Passenger started boarding" << endl;
    } else {
        cout << "Error: passenger didn't return from wait_for_train" << endl;
        exit(1);
    }

    // Train should not leave until passenger is boarded
    if (loaded_trains.load() != 0) {
        cout << "Error: train departed before passenger finished boarding"
             << endl;
        exit(1);
    }

    // Finish boarding passenger.
    cout << "Passenger finished boarding" << endl;
    station.boarded();

    // We should immediately see loaded trains become 1
    if (wait_for(loaded_trains, 1, 100)) {
        cout << "load_train returned, train departed" << endl;
    } else {
        cout << "Error: load_train didn't return after passengers "
        "finished boarding" << endl;
        exit(1);
    }
}

/* A passenger arrives, followed by a full train, then an empty train.
 * This test checks that the first train leaves even though someone is waiting,
 * because there is no space.
 */
void full_train_departs(void)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished) and number of passengers that have started
     * boarding (i.e. wait_for_train finished).
     */
    atomic<int> loaded_trains = 0;
    atomic<int> boarding_threads = 0;

    cout << "Passenger arrives, begins waiting" << endl;
    thread pass1(passenger, ref(station), ref(boarding_threads));
    pass1.detach(); // so we don't have to call join
    while (passengers_arrived != 1) /* Do nothing */;

    // First train has no room.
    cout << "Train arrives with no empty seats" << endl;
    thread train1(train, ref(station), 0, ref(loaded_trains));
    train1.detach(); // so we don't have to call join
    while (trains_arrived != 1) /* Do nothing */;

    // We should immediately see loaded_trains become 1
    if (!wait_for(loaded_trains, 1, 100)) {
        cout << "Error: load_train didn't return immediately" << endl;
        exit(1);
    } else {
        cout << "load_train returned, train departed" << endl;
    }

    // See if the passenger incorrectly boards
    if (boarding_threads.load() == 1) {
        cout << "Error: passenger returned from wait_for_train"
        << " when no empty seats were available" << endl;
        exit(1);
    }

    // Second train arrives with room.
    cout << "Train arrives with 3 seats available" << endl;
    thread train2(train, ref(station), 3, ref(loaded_trains));
    train2.detach(); // so we don't have to call join
    while (trains_arrived != 2) /* Do nothing */;

    // We should immediately see boarding_threads become 1
    if (wait_for(boarding_threads, 1, 100)) {
        cout << "Passenger started boarding" << endl;
    } else {
        cout << "Error: passenger didn't return from wait_for_train" << endl;
        exit(1);
    }

    // The train shouldn't leave yet because the passenger isn't boarded
    if (loaded_trains.load() > 1) {
        cout << "Error: second train departed before passenger finished"
                " boarding" << endl;
        exit(1);
    }

    // Finish boarding passenger.
    cout << "Passenger finished boarding" << endl;
    station.boarded();

    if (!wait_for(loaded_trains, 2, 100)) {
        cout << "Error: second train didn't depart after passenger "
                "finished boarding" << endl;
        exit(1);
    } else {
        cout << "load_train returned, second train departed" << endl;
    }
}

/* A passenger arrives, followed by an empty train, followed by a second
 * passenger. This test checks that the train waits for both passengers before
 * departing.
 */
void passenger_arrives_during_boarding(void)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished) and number of passengers that have started
     * boarding (i.e. wait_for_train finished).
     */
    atomic<int> loaded_trains = 0;
    atomic<int> boarding_threads = 0;

    cout << "Passenger arrives, begins waiting" << endl;
    thread pass1(passenger, ref(station), ref(boarding_threads));
    pass1.detach(); // so we don't have to call join
    while (passengers_arrived != 1) /* Do nothing */;

    // Wait to see if the passenger incorrectly boards
    if (wait_for(boarding_threads, 1, 1000)) {
        cout << "Error: passenger returned from wait_for_train when"
        << " no empty seats were available" << endl;
        return;
    }

    // train arrives with room.
    cout << "Train arrives with 3 seats available" << endl;
    thread train1(train, ref(station), 3, ref(loaded_trains));
    train1.detach(); // so we don't have to call join
    while (trains_arrived != 1) /* Do nothing */;

    // We should immediately see boarding_threads become 1
    if (wait_for(boarding_threads, 1, 100)) {
        cout << "Passenger started boarding" << endl;
    } else {
        cout << "Error: passenger didn't return from wait_for_train" << endl;
        exit(1);
    }

    // The train shouldn't leave yet because the passenger isn't boarded
    if (loaded_trains.load() != 0) {
        cout << "Error: train departed before first passenger finished "
                "boarding" << endl;
        exit(1);
    }

    // A second passenger arrives while the first one is boarding.
    cout << "Second passenger arrives" << endl;
    thread pass2(passenger, ref(station), ref(boarding_threads));
    pass2.detach(); // so we don't have to call join
    while (passengers_arrived != 2) /* Do nothing */;

    // We should immediately see boarding_threads become 2
    if (wait_for(boarding_threads, 2, 100)) {
        cout << "Second passenger started boarding" << endl;
    } else {
        cout << "Error: second passenger didn't return from wait_for_train"
             << endl;
        exit(1);
    }

    // The train shouldn't leave yet because the passengers aren't boarded
    if (loaded_trains.load() != 0) {
        cout << "Error: train departed before passengers finished boarding"
             << endl;
        exit(1);
    }

    // Finish boarding one passenger at a time.
    cout << "First passenger finished boarding" << endl;
    station.boarded();

    // The train shouldn't leave yet because the second passenger isn't boarded
    if (wait_for(loaded_trains, 1, 100)) {
        cout << "Error: train departed before second passenger finished "
                "boarding" << endl;
        exit(1);
    }

    cout << "Second passenger finished boarding" << endl;
    station.boarded();

    // Now the train should leave since no-one else is waiting
    if (wait_for(loaded_trains, 1, 100)) {
        cout << "load_train returned, train departed" << endl;
    } else {
        cout << "Error: load_train didn't return after passengers "
                "finished boarding" << endl;
        exit(1);
    }
}

/* 4 passengers arrive, followed by a train with enough space.  Make sure they
 * all board in parallel (i.e. wait_for_train has returned, but the passenger
 * has not invoked boarded).
 */
void board_in_parallel_all(void)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished) and number of passengers that have started
     * boarding (i.e. wait_for_train finished).
     */
    atomic<int> loaded_trains = 0;
    atomic<int> boarding_threads = 0;

    cout << "4 passengers arrive, begin waiting" << endl;
    thread pass1(passenger, ref(station), ref(boarding_threads));
    pass1.detach(); // so we don't have to call join
    thread pass2(passenger, ref(station), ref(boarding_threads));
    pass2.detach(); // so we don't have to call join
    thread pass3(passenger, ref(station), ref(boarding_threads));
    pass3.detach(); // so we don't have to call join
    thread pass4(passenger, ref(station), ref(boarding_threads));
    pass4.detach(); // so we don't have to call join

    // Wait for a bit to give all the passengers time to wait on
    // condition variables. This is technically a race, since it's
    // possible that one of the threads could be blocked from running
    // for longer than this amount of time, but it's the best we can
    // without additional information from the Station class. Thus
    // this test may occasionally fail.
    usleep(1000000);

    // see if any passengers incorrectly board
    if (boarding_threads.load() > 0) {
        cout << "Error: " << boarding_threads.load()
        << " passenger(s) returned from wait_for_train when"
        << " no empty seats were available" << endl;
        return;
    }

    cout << "Train arrives with 4 empty seats" << endl;
    thread train1(train, ref(station), 4, ref(loaded_trains));
    train1.detach(); // so we don't have to call join
    while (trains_arrived != 1) /* Do nothing */;

    // The passengers should immediately begin boarding
    if (wait_for(boarding_threads, 4, 100)) {
        cout << "4 passengers began boarding" << endl;
    } else {
        cout << "Error: expected 4 passengers to begin boarding, but "
        "actual number is " << boarding_threads.load() << endl;
        return;
    }

    cout << "3 passengers finished boarding" << endl;
    station.boarded();
    station.boarded();
    station.boarded();

    // Train should not depart yet, 1 more passenger is still boarding
    if (wait_for(loaded_trains, 1, 100)) {
        cout << "Error: load_train returned too soon" << endl;
        return;
    }

    cout << "Fourth passenger finished boarding" << endl;
    station.boarded();

    // Now train should leave, since no-one else is waiting
    if (wait_for(loaded_trains, 1, 100)) {
        cout << "load_train returned, train left" << endl;
    } else {
        cout << "Error: load_train didn't return when train was full" << endl;
        return;
    }
}

/* 4 passengers arrive, followed by a train with enough space for all but 1.
 * Make sure any possible threads can board in parallel
 * (i.e. wait_for_train has returned, but the passenger hasn't invoked boarded).
 */
void board_in_parallel(void)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished) and number of passengers that have started
     * boarding (i.e. wait_for_train finished).
     */
    atomic<int> loaded_trains = 0;
    atomic<int> boarding_threads = 0;

    cout << "4 passengers arrive, begin waiting" << endl;
    thread pass1(passenger, ref(station), ref(boarding_threads));
    pass1.detach(); // so we don't have to call join
    thread pass2(passenger, ref(station), ref(boarding_threads));
    pass2.detach(); // so we don't have to call join
    thread pass3(passenger, ref(station), ref(boarding_threads));
    pass3.detach(); // so we don't have to call join
    thread pass4(passenger, ref(station), ref(boarding_threads));
    pass4.detach(); // so we don't have to call join

    // Wait for a bit to give all the passengers time to wait on
    // condition variables. This is technically a race, since it's
    // possible that one of the threads could be blocked from running
    // for longer than this amount of time, but it's the best we can
    // without additional information from the Station class. Thus
    // this test may occasionally fail.
    usleep(1000000);

    // see if any passengers incorrectly board
    if (boarding_threads.load() > 0) {
        cout << "Error: " << boarding_threads.load()
        << " passenger(s) returned from wait_for_train when"
        << " no empty seats were available" << endl;
        return;
    }

    cout << "Train arrives with 3 empty seats" << endl;
    thread train1(train, ref(station), 3, ref(loaded_trains));
    train1.detach(); // so we don't have to call join
    while (trains_arrived != 1) /* Do nothing */;

    // Three passengers should immediately begin boarding
    if (wait_for(boarding_threads, 3, 100)) {
        cout << "3 passengers began boarding" << endl;
    } else {
        cout << "Error: expected 3 passengers to begin boarding, but "
        "actual number is " << boarding_threads.load() << endl;
        exit(1);
    }

    cout << "2 passengers finished boarding" << endl;
    station.boarded();
    station.boarded();

    // Train should not depart yet, 1 more passenger is still boarding
    if (wait_for(loaded_trains, 1, 100)) {
        cout << "Error: load_train returned too soon" << endl;
        exit(1);
    }

    cout << "Third passenger finished boarding" << endl;
    station.boarded();

    // Now train should leave, since it is full
    if (wait_for(loaded_trains, 1, 100)) {
        cout << "load_train returned, train departed" << endl;
    } else {
        cout << "Error: load_train didn't return when train was full" << endl;
        exit(1);
    }

    // Send another train for the last passenger.
    cout << "Another train arrives with 10 empty seats" << endl;
    thread train2(train, ref(station), 10, ref(loaded_trains));
    train2.detach(); // so we don't have to call join
    while (trains_arrived != 2) /* Do nothing */;

    // Now last thread should immediately board
    if (wait_for(boarding_threads, 4, 100)) {
        cout << "Last passenger began boarding" << endl;
    } else {
        cout << "Error: last passenger didn't begin boarding" << endl;
        exit(1);
    }

    cout << "Last passenger finished boarding" << endl;
    station.boarded();

    // Now train should leave, since no-one else is waiting
    if (wait_for(loaded_trains, 2, 100)) {
        cout << "load_train returned, train departed" << endl;
    } else {
        cout << "Error: load_train didn't return after passenger "
        "finished boarding" << endl;
        exit(1);
    }
}

/* This test checks that, if a train leaves with without using up all of its
 * space, the state is cleaned up so that future passengers don't think there
 * is a train in the station and attempt to board.
 */
void leftover(void)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished) and number of passengers that have started
     * boarding (i.e. wait_for_train finished).
     */
    atomic<int> loaded_trains = 0;
    atomic<int> boarding_threads = 0;

    // train arrives with space, but nobody waiting.
    cout << "Train with 10 seats arrives with no waiting passengers" << endl;
    thread train1(train, ref(station), 10, ref(loaded_trains));
    train1.detach(); // so we don't have to call join
    while (trains_arrived != 1) /* Do nothing */;

    // The train should leave immediately because nobody is waiting
    if (!wait_for(loaded_trains, 1, 100)) {
        cout << "Error: load_train didn't return immediately" << endl;
        exit(1);
    } else {
        cout << "load_train returned" << endl;
    }

    cout << "Passenger arrives, begins waiting" << endl;
    thread pass1(passenger, ref(station), ref(boarding_threads));
    pass1.detach(); // so we don't have to call join
    while (passengers_arrived != 1) /* Do nothing */;

    // Wait to see if the passenger incorrectly boards
    if (wait_for(boarding_threads, 1, 1000)) {
        cout << "Error: passenger returned from wait_for_train when "
                "no train was in the station" << endl;
        exit(1);
    }

    // Another train arrives to pick up the passenger.
    cout << "Second train arrives with 1 seat available" << endl;
    thread train2(train, ref(station), 1, ref(loaded_trains));
    train2.detach(); // so we don't have to call join
    while (trains_arrived != 2) /* Do nothing */;

    // Passenger should start boarding immediately
    if (wait_for(boarding_threads, 1, 100)) {
        cout << "Passenger started boarding" << endl;
    } else {
        cout << "Error: passenger didn't return from wait_for_train"
             << endl;
        exit(1);
    }

    // The train shouldn't leave yet, since the passenger is still boarding
    if (loaded_trains.load() > 1) {
        cout << "Error: second train departed before passenger finished boarding"
             << endl;
        exit(1);
    }

    // Finish boarding the passenger.
    cout << "Passenger finished boarding" << endl;
    station.boarded();

    if (!wait_for(loaded_trains, 2, 100)) {
        cout << "Error: second train didn't depart after passenger "
                "finished boarding" << endl;
        exit(1);
    } else {
        cout << "load_train returned, second train departed" << endl;
    }
}

/* In this test, a large number of passengers arrive all at once, then a
 * series of trains arrive with varying numbers of available seats. The
 * test makes sure that each train leaves with the right number of passengers.
 * The arguments give the total number of passengers and the largest amount
 * available space on any incoming train.
 */
void random(int total_passengers, int max_free_seats_per_train)
{
    trains_arrived = 0;
    passengers_arrived = 0;

    Station station;

    /* an int that can be atomically accessed/ modified without a separate lock
     * that represents number of trains that have left the station
     * (i.e. load_train finished) and number of passengers that have started
     * boarding (i.e. wait_for_train finished).
     */
    atomic<int> loaded_trains = 0;
    atomic<int> boarding_threads = 0;

    try {
        // A large number of passengers arrive, then a series of trains
        // arrive with varying numbers of available seats. Make sure that
        // each train leaves with the right number of passengers.

        // Create a large number of passengers waiting in the station.
        cout << "Starting randomized test with " << total_passengers
             << " passengers" << endl;
        thread passengers[total_passengers];
        for (int i = 0; i < total_passengers; i++) {
            passengers[i] = thread(passenger, ref(station),
                ref(boarding_threads));
            passengers[i].detach(); // so we don't have to call join
        }

        usleep(100000);

        if (boarding_threads.load() > 0) {
            cout << "Error: passenger(s) started boarding before the first "
                    "train arrived" << endl;
            exit(1);
        }

        // Each iteration through this loop processes a train with random
        // capacity.
        int passengers_left = total_passengers;
        while (passengers_left > 0) {

            // Pick a random train size and reset state
            int free_seats = rand() % (max_free_seats_per_train + 1);
            boarding_threads = 0;
            loaded_trains = 0;

            cout << "Train entering station with " << free_seats
            << " free seats, " << passengers_left
            << " waiting passengers" << endl;

            thread train_thread(train, ref(station), free_seats,
                ref(loaded_trains));
            train_thread.detach(); // so we don't have to call join
            while (trains_arrived != 1) /* Do nothing */;
            trains_arrived = 0;

            int expected_boarders = min(passengers_left, free_seats);
            int boarded = 0;

            // Board passengers on this train
            while (true) {
                // boarding_threads will reach at most expected_boarders,
                // boarded should eventually reach boarding_threads
                while (boarding_threads > boarded) {
                    // board one more passenger
                    station.boarded();
                    boarded++;
                    passengers_left--;
                }

                if (boarded >= expected_boarders) {
                    break;
                }

                // boarding_threads should continue increasing
                if (boarding_threads.load() == boarded) {
                    usleep(100000);
                }
                if (boarding_threads.load() == boarded) {
                    cout << "Error: stuck waiting for passenger " << boarded
                    << " to start boarding" << endl;
                    exit(1);
                }

                // The train shouldn't leave yet
                if (loaded_trains.load() != 0) {
                    cout << "Error: load_train returned after only "
                    << boarded <<  " passengers finished boarding"
                    << endl;
                    exit(1);
                }
            }

            // Now train should leave
            if (!wait_for(loaded_trains, 1, 100)) {
                cout << "Error: load_train didn't return after " << boarded
                     << " passengers boarded" << endl;
                exit(1);
            }

            nanosleep(&one_ms, nullptr);

            if (expected_boarders != boarding_threads) {
                cout << "Error: " << boarding_threads.load()
                     << " passengers started boarding (expected "
                     << expected_boarders << ")" << endl;
                exit(1);
            }
        }
        cout << "Test completed with no errors" << endl;
    }
    catch (system_error &e) {
        if (e.code().value() != EAGAIN)
            throw;
        cout << "Error: resource temporarily unavailable" << endl
        << "This usually means that the system ran out of threads,"
        << " which is not caused by anything in your own program."
        << "  Try running again, or, if it fails repeatedly,"
        << " log out and log in to a different myth machine." << endl
        << "  You can also try changing total_passengers in the"
        << " code to a smaller number (see code at end of main()),"
        << " but remember to change it back!"
        << endl;
        exit(1);
    }
}


/*
 * This creates a bunch of threads to simulate arriving trains and passengers.
 */
int main(int argc, char *argv[])
{
    srand(getpid() ^ time(NULL));

    // Add any new functions to map here
    unordered_map<string, function<void(void)>> testFns;
    testFns["no_waiting_passengers"] = no_waiting_passengers;
    testFns["passenger_wait_for_train"] = passenger_wait_for_train;
    testFns["train_wait_until_boarded"] = train_wait_until_boarded;
    testFns["full_train_departs"] = full_train_departs;
    testFns["passenger_arrives_during_boarding"] = passenger_arrives_during_boarding;
    testFns["board_in_parallel"] = board_in_parallel;
    testFns["board_in_parallel_all"] = board_in_parallel_all;
    testFns["leftover"] = leftover;
    // random is omitted, as it takes arguments

    if (argc == 1) {
        cout << "Available tests are:" << endl;
        for (auto p : testFns) {
            cout << "\t" << p.first << endl;
        }
        cout << "\t" << "random [NUM_PASSENGERS] [MAX_TRAIN_SIZE]" << endl;
        return 0;
    }

    auto search = testFns.find(argv[1]);
    if (search != testFns.end()) {
        search->second();
    } else if (string(argv[1]) == "random") {
        // CHANGE HERE TO CHANGE DEFAULTS (change numbers after : )
        int passengers = argc > 2 ? atoi(argv[2]) : 1000;
        int max_train_size = argc > 3 ? atoi(argv[3]) : 50;
        if (max_train_size < 2) {
            cout << "max train capacity must be at least 2" << endl;
            return 1;
        } else if (passengers < 0) {
            cout << "passengers must be >= 0" << endl;
            return 1;
        } else random(passengers, max_train_size);
    } else {
        cout << "No test named '" << argv[1] << "'" << endl;
    }

    return 0;
}
