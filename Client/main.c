#include "main.h"

volatile sig_atomic_t in_while = true;

void kill_exec_signal_handlers(int signal) {

    if (signal == SIGINT) {
        avs_log(main, INFO, ANSI_COLOR_YELLOW "|| ====== || EXITING FROM THE PROCESS OF REMOTE CONTROLLER || ====== ||" ANSI_COLOR_RESET);
        in_while = false;
    }
    else if (signal == SIGKILL) {
        avs_log(main, INFO, ANSI_COLOR_RED "|| ====== || KILL THE PROCESS OF REMOTE CONTROLLER || ====== ||" ANSI_COLOR_RESET);
        exit(0);
    }
}

void print_help_info(void) {

    // array of pointers to strings of availible options
    char *availible_options[] = {
        "==========================================================================================================\n"
        "=   Long option: '--endpoint-name' | short option: '-e'   = endpoint_name;                               =\n"
        "=   Long option: '--server-uri'    | short option: '-u'   = server_uri;                                  =\n"
        "=   Long option: '--lifetime'      | short option: '-l'   = time of registration update;                 =\n"
        "=   Long option: '--bootstrap'     | short option: '-b'   = bootstrap ON/OFF;                            =\n"
        "==========================================================================================================\n"
        "=   Supported security modes       : ANJAY_UDP_SECURITY_PSK                                              =\n"
    };
    
    // size of array of availible options
    int availible_options_size = sizeof(**availible_options);
    for (int i = 0; i < availible_options_size; i++) {
        printf("%s", availible_options[i]);
    }

return;
}

int main(int argc, char* argv[]) {

    signal(SIGINT, kill_exec_signal_handlers);
    signal(SIGUSR1, kill_exec_signal_handlers);

    char  *server_uri       = "coaps://127.0.0.1:5684";
    char  *endpoint_name    = "RPI_3B+";
    char  *fw_marker_path   = "/tmp/coros_fw-updated";
    bool  bootstrap_state   = false;
    int   time_to_wait      = DEFAULT_TIME_TO_WAIT;
    int   lifetime          = DEFAULT_ANJAY_LIFETIME;
   
    static struct option long_options[] = {
        { "endpoint-name",                 required_argument, 0, 'e' },
        { "server-uri",                    required_argument, 0, 'u' },
        { "lifetime",                      required_argument, 0, 'l' },
        { "bootstrap",                     no_argument,       0, 'b' },
        { "fw-updated-marker-path",        required_argument, 0, 'W' },
        { "help",                          no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };

    while(true) {
    int option_index = 0;
    int  getopt_var = getopt_long(argc, argv, "e:u:l:bw:h", long_options,
                                  &option_index);

        if (getopt_var == -1) {
            break;
        }

        switch (getopt_var) {

            case 'e': {
                endpoint_name = optarg;
                break;
            }

            case 'u': {
                if(!strncmp(optarg, "coaps", 5)){
                    server_uri = optarg;
                    break;
                } else {
                    avs_log(toyota_client, ERROR, ANSI_COLOR_RED "Unknown protocol - coaps expected" ANSI_COLOR_RESET);
                    return -1;
                }
            }

            case 'l': {
                lifetime = atoi(optarg);
                if(lifetime < 60){
                    avs_log(toyota_client, ERROR, ANSI_COLOR_RED "Lifetime is too short, please check!" ANSI_COLOR_RESET);
                    return -1;
                }
                avs_log(toyota_client, ERROR, "|| ===========|| Instance lifetime is: %i %s ||===========||", lifetime, "sec.");
                break;
            }

            case 'b': {
                bootstrap_state = true;
                avs_log(toyota_client, INFO, ANSI_COLOR_GREEN "|======| BOOTSTRAP CONNECTION ON |======|" ANSI_COLOR_RESET);
                break;
            }
            
            case 'w': {
                fw_marker_path = optarg;
                avs_log(toyota_client, INFO, ANSI_COLOR_GREEN "Firmware update marker file: %s" ANSI_COLOR_RESET, fw_marker_path);
                break;
            }

            case 'h': {
                print_help_info();
                return -1;
            }
        }
    }
    
    // default log level - DEBUG
    avs_log_set_default_level(AVS_LOG_DEBUG);
    
    client_t *obj_client = remote_client_create(1, endpoint_name,
                                                server_uri, "U",
                                                lifetime,
                                                bootstrap_state,
                                                fw_marker_path,
                                                (const char *const *) argv);
    if (!obj_client) {
        avs_log(toyota_client, ERROR, ANSI_COLOR_RED "failed to create client." ANSI_COLOR_RESET);
        return -1;
    }

    toyota_client_push_headlights_control(obj_client, true, 75);
    toyota_client_push_humidity(obj_client, 77.19, false);

    while (in_while) {
           remote_client_poll_sockets(obj_client, MIN(time_to_wait/1000, MAX_WAIT_TIME));
    }
    client_destroy(obj_client);

    return 0;
}
