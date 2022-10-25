#pragma once

#include <iostream>
#include <string>

// 时间戳类，用于获取本地时间
class Timestamp {
public:
    Timestamp();
    Timestamp(int64_t microSecondsSinceEpoch);

    static Timestamp now();
    std::string      toString() const;

    bool valid() const {
        return microSecondsSinceEpoch_ > 0;
    }
    int64_t microSecondsSinceEpoch() const {
        return microSecondsSinceEpoch_;
    }
    static Timestamp invalid() {
        return Timestamp();
    }
    static const int kMicroSecondsPerSecond = 1000 * 1000;

private:
    int64_t microSecondsSinceEpoch_;
};

// 处理时间为定时事件触发的时间
inline Timestamp addTime(Timestamp timestamp, double seconds) {
    int64_t delta =
        static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

inline bool operator<(Timestamp lhs, Timestamp rhs) {
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs) {
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline double timeDifference(Timestamp high, Timestamp low) {
    int64_t diff =
        high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
    return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}