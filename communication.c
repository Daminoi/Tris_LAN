#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "minilogger.h"
#include "common.h"
#include "communication.h"
#include "protocol.h"

extern struct conn_status conn_status;
extern pthread_mutex_t conn_status_mutex;

extern struct message message_queue_in[MESSAGE_QUEUE_SIZE];
extern int message_queue_in_curr_size;
extern pthread_mutex_t message_queue_in_mutex;

extern struct message message_queue_out[MESSAGE_QUEUE_SIZE];
extern int message_queue_out_curr_size;
extern pthread_mutex_t message_queue_out_mutex;

extern int connection_manager_socket;

void print_message(struct message* msg){
    #ifdef DEBUG
    if(msg != NULL)
        printf("Message: comm=%d n_args=%d arg1=%d arg2=%d\n", msg->communication, msg->n_args, msg->arg1, msg->arg2);
    else
        mini_log(ERROR, "print_message", -1, "Invalid parameter");
    #endif
    return;
}

void copy_message(struct message* dest, struct message* src){

    if(dest == NULL || src == NULL){
        mini_log(ERROR, "copy_message", -1, "Invalid parameters");
        return;
    }

    dest->communication = src->communication;
    dest->n_args = src->n_args;
    dest->arg1 = src->arg1;
    dest->arg2 = src->arg2;
}

bool validate_message(struct message* msg){
    if(msg == NULL)
        return false;

    if(msg->communication < 0 || msg->communication > 10){
        return false;
    }

    switch(msg->n_args){
        case 0:
            return true;
        break;
        case 1:
            if(msg->arg1 <= 0 || msg->arg1 > 9)
                return false;
            else
                return true;
        break;
        case 2:
            if(msg->arg1 <= 0 || msg->arg1 > 9 || msg->arg2 <= 0 || msg->arg2 > 9)
                return false;
            else
                return true;
        break;
        default:
            return false;
        break;
    }

    return true;
}

void* connection_manager(){

    struct message received_message;
    const int buffered_message_max_size = sizeof(struct message);

    char buffered_message[buffered_message_max_size];
    int buffered_message_size = 0;
    char temp_message_buffer[buffered_message_max_size];
    int temp_message_buffer_size = 0;

    fd_set socket_read_fd_set;

    struct timeval socket_block_timeout = {0, 250000};

    while(1){

        pthread_mutex_lock(&conn_status_mutex);
        if(conn_status.terminated_by_game == true || conn_status.terminated_by_other_peer == true){
            close_socket(connection_manager_socket);
            mini_log(INFO, "connection_manager", -1, "Terminating as requested");
            pthread_mutex_unlock(&conn_status_mutex);
            return NULL;
        }
        pthread_mutex_unlock(&conn_status_mutex);

        /* if there are any messages, write them to the socket */
        pthread_mutex_lock(&message_queue_out_mutex);

        int bytes_sent;

        while(message_queue_out_curr_size > 0){
            
            bytes_sent = send(connection_manager_socket, &message_queue_out[0], sizeof(struct message), 0);
            if(bytes_sent < 0){
                mini_log(ERROR, "connection_manager", -1, "Unable to send messages!");
                close_socket(connection_manager_socket);

                pthread_mutex_lock(&conn_status_mutex);
                conn_status.terminated_by_conn_manager = true;
                pthread_mutex_unlock(&conn_status_mutex);
                
                pthread_mutex_unlock(&message_queue_out_mutex);
                return NULL;
            }
            mini_log(LOG, "connection_manager", -1, "Message sent");
            print_message(&message_queue_out[0]);

            --message_queue_out_curr_size;

            /* the message queue has max lenght = 2 */
            if(message_queue_out_curr_size == 1){
                copy_message(&message_queue_out[0], &message_queue_out[1]);
            }
        }
        pthread_mutex_unlock(&message_queue_out_mutex);


        /* try to read something from the socket */

        FD_ZERO(&socket_read_fd_set);
        FD_SET(connection_manager_socket, &socket_read_fd_set);

        if(select(connection_manager_socket + 1, &socket_read_fd_set, NULL, NULL, &socket_block_timeout) > 0){

            pthread_mutex_lock(&conn_status_mutex);
            if(conn_status.terminated_by_game == true || conn_status.terminated_by_other_peer == true){
                close_socket(connection_manager_socket);
                mini_log(INFO, "connection_manager", -1, "Terminating as requested");
                pthread_mutex_unlock(&conn_status_mutex);
                return NULL;
            }
            pthread_mutex_unlock(&conn_status_mutex);
            
            if (FD_ISSET(connection_manager_socket, &socket_read_fd_set)){

                temp_message_buffer_size = recv(connection_manager_socket, temp_message_buffer, buffered_message_max_size-buffered_message_size, 0);
                if(temp_message_buffer_size <= 0){
                    if(temp_message_buffer_size == buffered_message_max_size){
                        mini_log(ERROR, "connection_manager", -1, "Message buffer full !");    
                    }
                    else{
                        mini_log(WARNING, "connection_manager", -1, "Recv returned 0 or -1 !");
                    }
                    close_socket(connection_manager_socket);

                    pthread_mutex_lock(&conn_status_mutex);
                    conn_status.terminated_by_conn_manager = true;
                    pthread_mutex_unlock(&conn_status_mutex);
                    return NULL;
                }
                else{
                    /* Copy the bytes received in the message buffer */

                    for(int i=0; i < temp_message_buffer_size; ++i){
                        buffered_message[buffered_message_size + i] = temp_message_buffer[i];
                    }
                    buffered_message_size += temp_message_buffer_size;
                    temp_message_buffer_size = 0;

                    /* if enough bytes were received try to "parse" a message */

                    if(buffered_message_size == buffered_message_max_size){
                        memcpy(&received_message, buffered_message, sizeof(struct message));

                        buffered_message_size = 0;

                        if(validate_message(&received_message)){
                            
                            pthread_mutex_lock(&message_queue_in_mutex);

                            if(message_queue_in_curr_size < MESSAGE_QUEUE_SIZE){
                                copy_message(&message_queue_in[message_queue_in_curr_size], &received_message);
                                ++message_queue_in_curr_size;

                                mini_log(LOG, "connection_manager", -1, "Received a message");
                                print_message(&received_message);

                                pthread_mutex_unlock(&message_queue_in_mutex);
                            }
                            else{
                                /* Too many messages in the queue */
                                mini_log(ERROR, "connection_manager", -1, "Message queue full!");
                                close_socket(connection_manager_socket);

                                pthread_mutex_lock(&conn_status_mutex);
                                conn_status.terminated_by_conn_manager = true;
                                pthread_mutex_unlock(&conn_status_mutex);

                                pthread_mutex_unlock(&message_queue_in_mutex);
                                return NULL;
                            }
                        }
                        else{
                            mini_log(ERROR, "connection_manager", -1, "The message received is not correct!");
                            close_socket(connection_manager_socket);

                            pthread_mutex_lock(&conn_status_mutex);
                            conn_status.terminated_by_conn_manager = true;
                            pthread_mutex_unlock(&conn_status_mutex);

                            return NULL;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}