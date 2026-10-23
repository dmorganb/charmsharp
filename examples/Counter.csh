#include <cstdio>

// A chare has a normal C++ body; only entry: members are remotely callable.
chare Counter {
private:
    int value = 0;
public:
    int localValue() const { return value; }
entry:
    Counter() {}
    void increment(int amount) { value += amount; }
};
