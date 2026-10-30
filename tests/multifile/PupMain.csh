#include <memory>
chare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Receiver receivers(CProxy_Receiver::ckNew());
        receivers[1].accept(std::make_shared<State>(42));
    }
};
