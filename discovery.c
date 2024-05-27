#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "discovery.h"
#include "minilogger.h"
#include "common.h"

extern int tcp_port;    // the tcp port won't change during the execution of the advertising thread

extern bool keep_advertising;
extern pthread_mutex_t keep_advertising_mutex;

int create_broadcast_socket(int discovery_port){


    if(discovery_port < 0){
        mini_log(ERROR, "create_broadcast_socket", -1, "port number is incorrect");
        return -1;
    }

    int broadcast_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(broadcast_socket < 0){
        mini_log(ERROR, "create_broadcast_socket", -1, "Unable to create the broadcast socket");
        return -1;
    }

    int enable_broadcast = 1;    
    if( setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(enable_broadcast)) < 0){
        mini_log(ERROR, "create_broadcast_socket", -1, "Unable to set socket options");
        close_socket(broadcast_socket);
        return -1;
    }

    return broadcast_socket;
}

int broadcast(int socket, struct sockaddr_in* broadcast_address, const discoveryMesssage* msg){

    return sendto(socket, (const void*)msg, sizeof(discoveryMesssage), 0, (struct sockaddr *)broadcast_address, sizeof(struct sockaddr_in));
}

void prepare_discovery_message(discoveryMesssage* msg, int port){
    if(msg == NULL || tcp_port < 0){
        mini_log(ERROR, "prepare_discovery_message", -1, "incorrect parameters");
        return;
    }

    msg->version = 1;
    msg->tcp_port = port;
}

void* discovery(){

    struct sockaddr_in broadcast_address;
    memset((void *)&broadcast_address, 0, sizeof(struct sockaddr_in));

    broadcast_address.sin_family = AF_INET;
    broadcast_address.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast_address.sin_port = htons(DISCOVERY_PORT);

    discoveryMesssage msg;
    prepare_discovery_message(&msg, tcp_port);

    int broadcast_socket;
    
    broadcast_socket = create_broadcast_socket(DISCOVERY_PORT);
    if(broadcast_socket < 0){
        mini_log(ERROR, "discovery thread", -1, "Unable to create a socket");
        return NULL;
    }

    // advertising packet in broadcast

    while(1){
        pthread_mutex_lock(&keep_advertising_mutex);

        if(keep_advertising){
            pthread_mutex_unlock(&keep_advertising_mutex);

            if( broadcast(broadcast_socket, &broadcast_address, &msg) < 0){
                mini_log(ERROR, "discovery thread", -1, "Unable to send a discovery datagram");
                close_socket(broadcast_socket);
                return NULL;
            }
            else{   // No errors while broadcasting
                //mini_log(LOG, "discovery thread", -1, "discovery datagram sent");
                ms_sleep(500);
            }
        }
        else{   // Stop advertising
            pthread_mutex_unlock(&keep_advertising_mutex);
            close_socket(broadcast_socket);
            mini_log(LOG, "discovery thread", -1, "exiting");
            return NULL;
        }
    }
}