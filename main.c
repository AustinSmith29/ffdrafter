#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "control.h"

int main(int argc, char *argv[])
{

	draftbot_initialize();

    DraftState state;
    init_draftstate(&state);

    while (1) 
    {
        char cmd_buf[100];
        printf(">");
        fgets(cmd_buf, 100, stdin); 
        cmd_buf[strcspn(cmd_buf, "\n")] = '\0';
        int code = do_command(cmd_buf, &state);
    }

	draftbot_destroy();
    return 0;
}
