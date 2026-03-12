#include "app/Application.h"

int main() {
    TopOpt::Application app;

    if (!app.init(1400, 900)) {
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}
