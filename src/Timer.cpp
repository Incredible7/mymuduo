#include "Timer.h"

std::atomic_int_least64_t Timer::s_numCreated;

void Timer::restart(Timestamp now) {
    if (repeat_) {
        expiration_ = addTime(now, interval_);
    } else {
        expiration_ = Timestamp::invalid();
    }
}