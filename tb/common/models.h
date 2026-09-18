#pragma once
#include <cstdint>
#include <deque>
extern "C" {
#include "dotp_ref.h"
}

struct FifoModel {
    std::deque<uint64_t> q;
    unsigned depth;
    bool overflow = false;
    explicit FifoModel(unsigned d) : depth(d) {}
    void push(uint64_t v) { if (q.size() >= depth) overflow = true; else q.push_back(v & 0x0000FFFFFFFFFFFFull); }
    uint64_t front() const { return q.empty() ? 0 : q.front(); }
    void pop() { if (!q.empty()) q.pop_front(); }
    unsigned count() const { return (unsigned)q.size(); }
};

static inline uint64_t to48(int64_t v) { return (uint64_t)v & 0x0000FFFFFFFFFFFFull; }
