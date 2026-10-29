// Two independent template chares in one module test header ordering.
// The complete generated classes must precede CK_TEMPLATES_ONLY.
chare Main {
    int remaining = 2;
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_First<int>::ckNew(7, thisProxy);
        CProxy_Second<double>::ckNew(2.5, thisProxy);
    }

    void complete() {
        if (--remaining == 0) {
            CkPrintf("Ordered template chares completed\n");
            CkExit();
        }
    }
};

template<typename T>
chare First {
entry:
    First(T value, CProxy_Main caller) {
        CkPrintf("First: %d\n", static_cast<int>(value));
        caller.complete();
    }
};

template<typename T>
chare Second {
entry:
    Second(T value, CProxy_Main caller) {
        CkPrintf("Second: %.1f\n", static_cast<double>(value));
        caller.complete();
    }
};
