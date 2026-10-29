template<typename T>
chare OtherWorker {
entry:
    OtherWorker(T initial) {
        CkPrintf("MULTIFILE OTHER TEMPLATE: %d\n", static_cast<int>(initial));
        CkExit();
    }
};

template chare OtherWorker<long>;
