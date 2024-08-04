// This class represents one party, which is capable of matching guests
// according to their zodiac signs.

#ifndef PARTY_H
#define PARTY_H

#include <condition_variable>
#include <mutex>
#include <queue>

// The total number of Zodiac signs
static const int NUM_SIGNS = 12;

class Party {
public:
    // Invoked by newly arriving guests; my_name is the guest's name,
    // my_sign is the guest's Zodiac sign, and other_sign is the sign
    // of another guest that this guest would like to meet. Returns
    // only when there is another guest available with matching signs,
    // and returns that guest's name (my_name will be returned to the
    // other guest).
    std::string meet(std::string &my_name, int my_sign, int other_sign);

private:
    // Synchronizes access to this structure.
    std::mutex mutex_;

    typedef struct Guest {
        std::string name;

        // will be modified, so use ptr
        std::string *match;
        bool *isMatched;

        // condition variables can't be copied, so use ptr
        std::condition_variable_any *match_found;
    } Guest;

    std::queue<Guest> guestsWaiting[NUM_SIGNS][NUM_SIGNS];
};

#endif /* PARTY_H */
