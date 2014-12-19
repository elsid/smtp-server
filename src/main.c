#include "log.h"
#include "server.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        PRINT_STDERR("Usage: %s <config file name>\n", argv[0]);
        return 1;
    }

    settings_t settings;

    if (settings_init(&settings, argv[1]) < 0) {
        return -1;
    }

    const int result = server_run(&settings);

    settings_destroy(&settings);

    return result;
}
