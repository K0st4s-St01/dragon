#include "dragon_editor/app.h"

int main(int argc, char **argv) {
    App *app = app_create(1920, 1080, "Dragon Editor");
    if (!app) return 1;

    if (argc > 1)
        app_open_file(app, argv[1]);

    app_run(app);
    app_destroy(app);
    return 0;
}
