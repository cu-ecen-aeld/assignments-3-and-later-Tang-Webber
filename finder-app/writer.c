#include <stdio.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    openlog(NULL, 0, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments");
        return 1;
    }

    const char *wf = argv[1];
    const char *wstr = argv[2];

    FILE *out_file = fopen(wf, "w");
    if (out_file == NULL) {
        syslog(LOG_ERR, "Unable to open file");
        return 1;
    }

    fprintf(out_file, "%s", wstr);
    syslog(LOG_DEBUG, "Writing %s to %s", wstr, wf);

    fclose(out_file);
    closelog();

    return 0;
}

