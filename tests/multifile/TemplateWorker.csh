template<typename T>
chare GenericWorker {
    T value;
entry:
    GenericWorker(T initial) : value(initial) {
        CkPrintf("MULTIFILE TEMPLATE PASS: %d\n", static_cast<int>(value));
        CkExit();
    }
};

template chare GenericWorker<int>;
