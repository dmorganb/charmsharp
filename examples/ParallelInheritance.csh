class Audit {
public:
    int code() const { return 42; }
};

chare Base {
entry:
    Base() {}
    void inheritedEntry() {
        CkPrintf("Hello from inherited proxy: 42\n");
        CkExit();
    }
};

chare Derived : public Base, public Audit {
entry:
    Derived() {}
    void ownEntry() { (void)code(); }
};

mainchare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Derived child = CProxy_Derived::ckNew();
        CProxy_Base parent = child;
        parent.inheritedEntry();
    }
};
