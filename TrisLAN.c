#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>

#include "common.h"
#include "communication.h"
#include "minilogger.h"
#include "discovery.h"
#include "protocol.h"
#include "gameLogic.h"

#define HOST_LIST_SIZE 10

int tcp_port;

int connection_manager_socket;

bool keep_advertising;
pthread_mutex_t keep_advertising_mutex = PTHREAD_MUTEX_INITIALIZER;

void search_for_hosts(){
    clean_console();

    struct endpoint{
        char ip[INET_ADDRSTRLEN];
        int port;
    };
    struct endpoint host_list[HOST_LIST_SIZE];
    int host_list_index;
    
    int discovery_scanner_socket;
    int connection_socket;

    discovery_scanner_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(discovery_scanner_socket < 0){
        mini_log(ERROR, "search_for_host", -1, "Unable to create the scanner socket");
        return;
    }

    struct sockaddr_in rcv_address;

    rcv_address.sin_family = AF_INET;
    rcv_address.sin_addr.s_addr = INADDR_ANY;
    rcv_address.sin_port = htons(DISCOVERY_PORT);

    if( bind(discovery_scanner_socket, (const struct sockaddr*)&rcv_address, sizeof(struct sockaddr)) < 0){
        mini_log(ERROR, "search_for_host", -1, "Bind returned -1");
        close_socket(discovery_scanner_socket);
        return;
    }

    struct sockaddr_in srv_address;
    int srv_address_size;
    char srv_ip[INET_ADDRSTRLEN];

    for(int i=0; i < INET_ADDRSTRLEN; ++i){
        srv_ip[i] = '\0';
    }
    
    discoveryMesssage msg;
    int n_byte_read;

    int search_time = 4000;
    struct timeval small_timeout = {0, 4000000};
    struct timespec scanner_stop_absolute_time;
    struct timespec current_timestamp;
    fd_set read_fd_set;
    
    get_absolute_time_with_offset(search_time, &scanner_stop_absolute_time);

    host_list_index = 0;

    printf("\n\n\tLooking for games on your LAN (%d seconds search time)...\n", search_time/1000);

    do{

        FD_ZERO(&read_fd_set);
        FD_SET(discovery_scanner_socket, &read_fd_set);

        if(select(discovery_scanner_socket + 1, &read_fd_set, NULL, NULL, &small_timeout) > 0){
            
            if (FD_ISSET(discovery_scanner_socket, &read_fd_set)){

                n_byte_read = recvfrom(discovery_scanner_socket, &msg, 8, 0, (struct sockaddr*)&srv_address, (socklen_t*)&srv_address_size);
                if(n_byte_read <= 0){
                    mini_log(ERROR, "search_for_host", -1, "Recv returned 0 or -1 !");
                    close_socket(discovery_scanner_socket);
                    return;
                }
                else{
                    inet_ntop(AF_INET, &(srv_address.sin_addr), srv_ip, INET_ADDRSTRLEN);
                    #ifdef DEBUG
                    printf("\n\tReceived from %s: Version=%d Tcp port=%d\n", srv_ip, msg.version, msg.tcp_port);
                    #endif

                    if(host_list_index < HOST_LIST_SIZE){
                        bool present = false;
                        
                        for(int i=0; i <= host_list_index; ++i){
                            if(strcmp(srv_ip, host_list[i].ip) == 0 && msg.tcp_port == host_list[i].port){
                                present = true;
                            }
                        }

                        if(!present){
                            strcpy(host_list[host_list_index].ip, srv_ip);
                            host_list[host_list_index].port = msg.tcp_port;
                            ++host_list_index;
                        }
                    }
                }
            }
        }
        get_current_time_in_timespec(&current_timestamp);
    }while(is_greater(&scanner_stop_absolute_time, &current_timestamp));
    
    close_socket(discovery_scanner_socket);

    clean_console();
    if(host_list_index == 0){
        printf("\n\n\tNo hosts are active on your LAN.\n");
        
        printf("\n\tPress ENTER to go back.\n");
    }
    else{
        if(host_list_index == 1){
            printf("\n\n\tThe scan has found 1 game on your LAN.\n");
        }
        else{
            printf("\n\n\tThe scan has found %d games on your LAN.\n", host_list_index);
        }

        printf("\n");
        for(int i=0; i < host_list_index; ++i){
            printf("\t%d. Connect to the game hosted by %s:%d\n", i+1, host_list[i].ip, host_list[i].port);
        }
        printf("\t0. Go back to the main menu\n");
        printf("\n\tTo select an item, input the corresponding number:");

        int option;
        scanf("%d", &option);

        printf("\n");

        if(option <= 0 || option > host_list_index){
            return;
        }
        else{
            option = option - 1; // the array starts at index 0 but it is shown to the user as starting at 1

            inet_pton(AF_INET, host_list[option].ip, &srv_address.sin_addr);
            srv_address.sin_port = htons(host_list[option].port);

            if((connection_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                mini_log(ERROR, "search_for_host", -1, "Unable to create the tcp socket");
                return;
            }

            printf("\tTrying to connect to %s:%d...\n", host_list[option].ip, host_list[option].port);
            if( connect(connection_socket, (struct sockaddr*)&srv_address, sizeof(srv_address)) < 0){
                printf("\tConnection failed\n");
                close(connection_socket);
                return;
            }
            else{
                printf("\tConnection successful\n");

                /* Start the game */

                struct gameState gs;
                gs.role = GUEST;

                connection_manager_socket = connection_socket;

                game(&gs);

                close(connection_socket);
            }
        }
    }

    wait_for_any_key_press();
}

void stop_searching_handler(int signal){
    printf("\n\tStopping\n");
}

void host_new_game(){

    /* Preparing the tcp socket */
    int accept_socket, connection_socket;
    struct sockaddr_in accept_adddress;
    int accept_address_size;
    char guest_ip[INET_ADDRSTRLEN];
    struct sockaddr_in guest_adddress;
    int guest_address_size;


    if ((accept_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        mini_log(ERROR, "host_new_game", -1, "Unable to create the tcp socket");
        return;
    }

    accept_adddress.sin_family = AF_INET;
    accept_adddress.sin_addr.s_addr = INADDR_ANY;
    accept_adddress.sin_port = htons(0);

    if (bind(accept_socket, (struct sockaddr *)&accept_adddress, sizeof(struct sockaddr_in)) < 0){
        mini_log(ERROR, "host_new_game", -1, "Unable to bind the tcp socket");
        close(accept_socket);
        return;
    }
    
    accept_address_size = sizeof(accept_adddress);
    if (getsockname(accept_socket, (struct sockaddr *)&accept_adddress, (socklen_t*)&accept_address_size) < 0){
        mini_log(ERROR, "host_new_game", -1, "Unable to get the tcp port");
        perror("ERROR TCP PORT:");
        close(accept_socket);
        return;
    }
    tcp_port = ntohs(accept_adddress.sin_port);

    if (listen(accept_socket, 0) < 0){
        mini_log(ERROR, "host_new_game", -1, "Unable to listen on the tcp socket");
        close(accept_socket);
        return;
    }
    

    /* Creating the discovery thread */
    keep_advertising = true;

    pthread_t discovery_thread_tid;
    pthread_attr_t discovery_thread_attr;

    pthread_attr_init(&discovery_thread_attr);
    pthread_attr_setdetachstate(&discovery_thread_attr, PTHREAD_CREATE_JOINABLE);

    if(pthread_create(&discovery_thread_tid, NULL, discovery, NULL) < 0){
        mini_log(ERROR, "host_new_game", -1, "Unable to create the discovery thread");
        pthread_attr_destroy(&discovery_thread_attr);
        return;
    }
    else{
        mini_log(LOG, "host_new_game", -1, "Discovery thread created successfully");
    }

    printf("\n\n\tWaiting for a guest to join... (Use [CTRL + C] to go back)\n");

    struct sigaction handle_ctrl_c = {0};
    struct sigaction previous_handler = {0};
    handle_ctrl_c.sa_handler = stop_searching_handler;
    sigaction(SIGINT, &handle_ctrl_c, &previous_handler);

    if ((connection_socket = accept(accept_socket, (struct sockaddr *)&guest_adddress, &guest_address_size)) < 0){
        if(errno != EINTR){
            mini_log(ERROR, "host_new_game", -1, "Unable to accept a guest");
        }
    }
    close(accept_socket);

    sigaction(SIGINT, &previous_handler, NULL);

    pthread_mutex_lock(&keep_advertising_mutex);
    keep_advertising = false;
    pthread_mutex_unlock(&keep_advertising_mutex);

    pthread_join(discovery_thread_tid, NULL);
    mini_log(LOG, "host_new_game", -1, "Discovery thread terminated successfully");
    pthread_attr_destroy(&discovery_thread_attr);

    if(connection_socket > 0){
        inet_ntop(AF_INET, &(guest_adddress.sin_addr), guest_ip, INET_ADDRSTRLEN);
        clean_console();
        printf("\n\tOne player joined, starting the game...\n");

        struct gameState gs;
        gs.role = HOST;

        connection_manager_socket = connection_socket;

        game(&gs);

        close(connection_socket);
    }
}


void show_main_menu_options(){
    printf("\n\n");

    printf("\t1) Host a new game.\n");
    printf("\t2) Look for available games on your LAN.\n");
    printf("\t0) Exit the program.\n");
    
    printf("\n\tTo select an item, input the corresponding number:");
}

int main(){
    int option = -1;

    do{
        clean_console();
        show_main_menu_options();

        scanf("%d", &option);

        switch(option){
            case 0:
                break;
            case 1:
                host_new_game();
                break;
            case 2:
                search_for_hosts();
                break;
        }

    }while(option != 0);
    clean_console();
}
