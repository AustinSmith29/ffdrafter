#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "control.h"

int main(int argc, char *argv[])
{
    Engine engine;

    init_engine(&engine);

	printf("fDraft v%d.%d.%d\nWritten by Austin Smith 2023.\n\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    char cmd_buf[100];
    do 
    {
        printf(">");
        fgets(cmd_buf, 100, stdin); 
        cmd_buf[strcspn(cmd_buf, "\n")] = '\0';
    } while (do_command(cmd_buf, &engine) != QUIT);

    destroy_engine(&engine);

    return EXIT_SUCCESS;
}
