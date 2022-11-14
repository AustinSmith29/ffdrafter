#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "control.h"
#include "config.h"

int main(int argc, char *argv[])
{
    DraftConfig config;
    int res = load_config(&config, "nfl.cfg");
	draftbot_initialize();

    DraftState* state = init_draftstate(&config);

	fprintf(stdout, "fDraft v%d.%d.%d\nWritten by Austin Smith.\n\n", VERSION_MAJOR,
																	VERSION_MINOR,
																	VERSION_PATCH);
    int code = 0;
    while (code != QUIT) 
    {
        char cmd_buf[100];
        printf(">");
        fgets(cmd_buf, 100, stdin); 
        cmd_buf[strcspn(cmd_buf, "\n")] = '\0';
        code = do_command(cmd_buf, state, &config);
    }

	draftbot_destroy();
	destroy_draftstate(state, &config);
    return 0;
}
