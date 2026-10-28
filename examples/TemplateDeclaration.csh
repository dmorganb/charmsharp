// Phase 6.3.1: uninstantiated chare template declaration.
// Concrete specialization registration and runtime use follow in 6.3.2.
template<typename T>
chare Worker {
entry:
    Worker(T initial) : value(initial) {}
    void process(T next) { value = next; }
private:
    T value;
};
