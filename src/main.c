#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "control.h"

const char* usage = "usage: fdraft [options] [draft-config] [player-pool]\n\n" 
                    "  -h --help              show this message\n"
                    "  --version              show version\n"
                    "\nBug reports, feedback, admiration, abuse, etc, to: smithaustin0129@gmail.com\n";

struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
};

int main(int argc, char *argv[])
{
    int option;
    int option_index = 0;

    while ( (option = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1)
    {
    }
    Engine engine;

    init_engine(&engine);

	printf("fDraft v%d.%d.%d\nWritten by Austin Smith.\n\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

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
