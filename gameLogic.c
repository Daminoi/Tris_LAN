#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "minilogger.h"
#include "common.h"
#include "gameLogic.h"
#include "protocol.h"
#include "communication.h"

struct conn_status conn_status;
pthread_mutex_t conn_status_mutex;

struct message message_queue_in[MESSAGE_QUEUE_SIZE];
int message_queue_in_curr_size;
pthread_mutex_t message_queue_in_mutex;

struct message message_queue_out[MESSAGE_QUEUE_SIZE];
int message_queue_out_curr_size;
pthread_mutex_t message_queue_out_mutex;

static int game_field[9];
static char game_symbols[2] = {'x', 'o'};
static const int check_victory_patterns[8][3] = 
{
    {0, 1, 2},
    {3, 4, 5},
    {6, 7, 8},
    {0, 3, 6},
    {1, 4, 7},
    {2, 5 ,8},
    {0, 4, 8},
    {2, 4, 6}
};

/* Returns the number of the winner (1=HOST 2=GUEST) or 0 if no one has won */
int check_victory(){
    int aux;

    for(int i=0; i < 8; ++i){
        aux = game_field[check_victory_patterns[i][0]];
    
        if(aux == game_field[check_victory_patterns[i][1]] && aux == game_field[check_victory_patterns[i][2]]){
            return aux;
        }
    }

    return 0;
}

/* Return true if there are no free cells remaining */
bool check_field_full(){
    for(int i=0; i < 9; ++i){
        if(game_field[i] == 0)
            return false;
    }
    
    return true;
}

void print_game_field(){
    printf("\n");
    
    for(int offset=0; offset < 3; ++offset){
        printf("\t\t+---+---+---+\n");

        printf("\t\t");
        for(int i=0; i < 3; ++i){
            if(game_field[(offset * 3) + i] == 0){
                printf("|   ");
            }
            else{
                printf("| %c ", game_symbols[game_field[(offset * 3) + i]-1]);
            }
        }
        printf("|\n");
    }

    printf("\t\t+---+---+---+\n");
}

bool can_place_symbol(int pos){
    if (pos < 0 || pos > 9){
        return false;
    }
    else{
        return game_field[pos] == 0;
    }
}

bool place_symbol(int pos, int symbol){
    if(can_place_symbol(pos) && symbol >= 1 && symbol <= 2){
        game_field[pos] = symbol;
        return true;
    }
    else{
        return false;
    }
}

/* Pops the first message from the incoming messages queue and puts the message in msg */
bool get_incoming_message(struct message* msg){
    if(msg == NULL){
        mini_log(ERROR, "get_incoming_message", -1, "Incorrect parameter");
        return false;
    }

    pthread_mutex_lock(&message_queue_in_mutex);
    
    if(message_queue_in_curr_size > 0){
        copy_message(msg, &message_queue_in[0]);
        
        --message_queue_in_curr_size;

        if(message_queue_out_curr_size == 1){
            copy_message(&message_queue_in[0], &message_queue_in[1]);
        }

        pthread_mutex_unlock(&message_queue_in_mutex);    
        return true;
    }

    pthread_mutex_unlock(&message_queue_in_mutex);
    return false;
}

/* Inserts msg at the of the outgoing messages queue */
bool send_message(struct message* msg){
    if(msg == NULL){
        mini_log(ERROR, "send_message", -1, "Incorrect parameter");
        return false;
    }

    pthread_mutex_lock(&message_queue_out_mutex);

    if(message_queue_out_curr_size >= MESSAGE_QUEUE_SIZE){
        mini_log(ERROR, "send_message", -1, "The message queue is full!");
        pthread_mutex_unlock(&message_queue_out_mutex);
        return false;
    }
    else{
        copy_message(&message_queue_out[message_queue_out_curr_size], msg);
        ++message_queue_out_curr_size;
        pthread_mutex_unlock(&message_queue_out_mutex);
        return true;
    }
}

/* Puts the specified values in the msg pointed by msg */
void prepare_message(struct message* msg, enum comm comm, int n_args, int arg1, int arg2){
    if(msg == NULL){
        mini_log(ERROR, "prepare_message", -1, "Invalid parameter!");
        return;
    }

    msg->communication = comm;
    msg->n_args = n_args;
    msg->arg1 = arg1;
    msg->arg2 = arg2;
}

void reset_conn_status(){   // doesn't need synchronization
    conn_status.terminated_by_conn_manager = false;
    conn_status.terminated_by_game = false;
    conn_status.terminated_by_other_peer = false;
}

/* Returns true if at least one of the fields of conn_status is true */
bool should_terminate(){
    bool res;

    pthread_mutex_lock(&conn_status_mutex);
    res = conn_status.terminated_by_conn_manager || conn_status.terminated_by_game || conn_status.terminated_by_other_peer;
    pthread_mutex_unlock(&conn_status_mutex);

    return res;
}

/* Main gameloop function */
void game(struct gameState* game_state){

    /* set up */
    reset_conn_status();

    message_queue_in_curr_size = 0;
    message_queue_out_curr_size = 0;

    for(int i=0; i < 9; ++i){
        game_field[i] = 0;
    }

    struct message rcv_msg;
    struct message snd_msg;

    pthread_t communication_thread_tid;
    pthread_attr_t communication_thread_attr;

    pthread_attr_init(&communication_thread_attr);
    pthread_attr_setdetachstate(&communication_thread_attr, PTHREAD_CREATE_JOINABLE);

    if(pthread_create(&communication_thread_tid, NULL, connection_manager, NULL) < 0){
        mini_log(ERROR, "game", -1, "Unable to create the connection manager thread");
        pthread_attr_destroy(&communication_thread_attr);

        pthread_mutex_lock(&conn_status_mutex);
        conn_status.terminated_by_game = true;
        pthread_mutex_lock(&conn_status_mutex);

        return;
    }
    else{
        mini_log(LOG, "game", -1, "Connection manager thread created successfully");
    }
    
    bool message_available = false;
    
    int first_turn = HOST;

    if(game_state->role == HOST){
        do{
            printf("\n\tChoose the player that will play first:\n");
            printf("\t1. You\n");
            printf("\t2. Your opponent\n");

            scanf("%d", &first_turn);

            clean_console();
            
            if(first_turn != 1 && first_turn != 2){
                printf("\n\tPlease, choose again\n");
            }
        }while(first_turn != 1 && first_turn != 2);
    }
    else{
        printf("\n\tWaiting for the host's choice...\n");
    }

    /* Starting the game protocol */
    if(game_state->role == HOST){
        prepare_message(&snd_msg, WELCOME, 1, first_turn, 0);
        send_message(&snd_msg);

        mini_log(LOG, "game", -1, "Host: sent WELCOME message");

        game_state->phase = OPEN_CONNECTION;
        game_state->last_comm = WELCOME;

        do{
            pthread_mutex_lock(&message_queue_in_mutex);
            if(message_queue_in_curr_size > 0){
                message_available = true;
            }
            pthread_mutex_unlock(&message_queue_in_mutex);
            ms_sleep(500);
        }while(message_available == false && should_terminate() == false);

        if(!should_terminate()){

            get_incoming_message(&rcv_msg);
            mini_log(LOG, "game", -1, "Host: received first message");

            if(rcv_msg.communication != OK){
                mini_log(ERROR, "game", __LINE__, "HOST OPENING SEQUENCE FAILED");

                pthread_mutex_lock(&conn_status_mutex);
                conn_status.terminated_by_game = true;
                pthread_mutex_unlock(&conn_status_mutex);
            }
            else{
                if(first_turn == HOST)
                    game_state->phase = GAME_TURN_HOST;
                else
                    game_state->phase = GAME_TURN_GUEST;
                game_state->last_comm = WELCOME;
            }
        }
        else{
            mini_log(ERROR, "game", __LINE__, "HOST OPENING SEQUENCE FAILED (connection_manager terminated)");
        }
    }
    else{
        mini_log(LOG, "game", -1, "Guest: waiting for WELCOME");

        do{
            pthread_mutex_lock(&message_queue_in_mutex);
            if(message_queue_in_curr_size > 0){
                message_available = true;
            }
            pthread_mutex_unlock(&message_queue_in_mutex);
            ms_sleep(500);
        }while(message_available == false && should_terminate() == false);

        if(!should_terminate()){

            get_incoming_message(&rcv_msg);
            mini_log(LOG, "game", -1, "Guest: received first message");

            if(rcv_msg.communication != WELCOME){
                mini_log(ERROR, "game", __LINE__, "GUEST OPENING SEQUENCE FAILED");

                pthread_mutex_lock(&conn_status_mutex);
                conn_status.terminated_by_game = true;
                pthread_mutex_unlock(&conn_status_mutex);
            }
            else{
                first_turn = rcv_msg.arg1;
                if(first_turn == GUEST)
                    game_state->phase = GAME_TURN_GUEST;
                else
                    game_state->phase = GAME_TURN_HOST;
                game_state->last_comm = WELCOME;

                prepare_message(&snd_msg, OK, 0, 0, 0);
                send_message(&snd_msg);
                mini_log(LOG, "game", -1, "Guest: OK sent");
            }
        }
        else{
            mini_log(ERROR, "game", __LINE__, "GUEST OPENING SEQUENCE FAILED (connection_manager terminated)");
        }
    }

    if(!should_terminate())
        mini_log(LOG, "game", -1, "Opening sequence completed");

    if(first_turn != game_state->role){
        printf("\n\tWaiting for the other player's move...\n");
    }

    while(!should_terminate()){

        if(first_turn == game_state->role && first_turn != 0){
            goto FIRST_TURN_START;              /* sad but necessary, only used if it's the first turn */
            mini_log(LOG, "game", -1, "First turn!");
        }

        /* Wait for a message from the other peer */
        message_available = false;

        do{
            pthread_mutex_lock(&message_queue_in_mutex);
            if(message_queue_in_curr_size > 0){
                message_available = true;
            }
            pthread_mutex_unlock(&message_queue_in_mutex);
            ms_sleep(500);
        }while(should_terminate() == false && message_available == false);

        if(!should_terminate()){
                
            get_incoming_message(&rcv_msg);

            /* Filter the message and act accordingly */

            if(rcv_msg.communication == NO_UNEXPECTED || rcv_msg.communication == DISCONNECT){
                pthread_mutex_lock(&conn_status_mutex);
                conn_status.terminated_by_other_peer = true;
                pthread_mutex_unlock(&conn_status_mutex);

                game_state->phase = GAME_INTERRUPTED;
            }


            clean_console();

            if(!should_terminate()){

                switch(game_state->phase){
                    case GAME_TURN_HOST:
                    case GAME_TURN_GUEST:
                        /* the phase where each player chooses where to place a x or o */

                        switch(rcv_msg.communication){
                            case PLACE:

                                if(can_place_symbol(rcv_msg.arg1-1)){
                                    if(game_state->role == HOST){
                                        place_symbol(rcv_msg.arg1-1, GUEST);
                                    }
                                    else{
                                        place_symbol(rcv_msg.arg1-1, HOST);
                                    }

    FIRST_TURN_START:
                                    first_turn = 0;

                                    int victory = check_victory();
                                    if(victory != 0){
                                        /* this section signals the other peer's victory*/
                                        prepare_message(&snd_msg, WIN, 1, victory, 0);
                                        send_message(&snd_msg);

                                        game_state->phase = GAME_END;
                                        game_state->last_comm = WIN;
                                    }
                                    else{
                                        if(victory == 0 && check_field_full() == true){
                                            /* draw expected, the value 3 represents draw */
                                            prepare_message(&snd_msg, WIN, 1, 3, 0);
                                            send_message(&snd_msg);

                                            game_state->phase = GAME_END;
                                            game_state->last_comm = WIN;
                                        }
                                        else{
                                            int choice;

                                            print_game_field();

                                            do{
                                                printf("\n\tWrite a number from 1 to 9 to place your symbol on the corresponding cell\n");
                                                printf("\tYou can also insert 0 to leave the game:");

                                                scanf("%d", &choice);

                                                if(choice != 0 && can_place_symbol(choice-1) == false)
                                                    printf("\n\tYou can't choose that cell.\n");
                
                                            }while(choice != 0 && can_place_symbol(choice-1) == false);

                                            printf("\n");

                                            if(choice == 0){
                                                prepare_message(&snd_msg, DISCONNECT, 0, 0, 0);
                                                send_message(&snd_msg);

                                                pthread_mutex_lock(&conn_status_mutex);
                                                conn_status.terminated_by_game = true;
                                                pthread_mutex_unlock(&conn_status_mutex);

                                                game_state->phase = GAME_INTERRUPTED;
                                                game_state->last_comm = DISCONNECT;
                                            }
                                            else{
                                                place_symbol(choice-1, game_state->role);
                                                print_game_field();

                                                prepare_message(&snd_msg, PLACE, 1, choice, 0);
                                                send_message(&snd_msg);

                                                if(game_state->role == HOST)
                                                    game_state->phase = GAME_TURN_GUEST;
                                                else
                                                    game_state->phase = GAME_TURN_HOST;
                                                game_state->last_comm = PLACE;

                                                printf("\n\tWaiting for the other player's move...\n");
                                            }
                                        }
                                    }

                                }
                                else{
                                    prepare_message(&snd_msg, NO_RESYNC, 0, 0, 0);
                                    send_message(&snd_msg);

                                    if(game_state->role == HOST){
                                        prepare_message(&snd_msg, SYNC_START, 0, 0, 0);
                                        send_message(&snd_msg);

                                        game_state->phase = RESYNC;
                                        game_state->last_comm = SYNC_START;
                                    }
                                    else{
                                        game_state->phase = RESYNC;
                                        game_state->last_comm = NO_RESYNC;
                                    }
                                }
                            break;
                            case NO_RESYNC:
                                mini_log(ERROR, "game", __LINE__, "RESYNC NOT YET SUPPORTED");

                                pthread_mutex_lock(&conn_status_mutex);
                                conn_status.terminated_by_game = true;
                                pthread_mutex_unlock(&conn_status_mutex);
                            break;
                            case WIN:
                                /* In this case this peer has received a victory message */
                                int victory = check_victory();

                                if(victory == rcv_msg.arg1){
                                    if(victory == game_state->role){
                                        printf("\n\n\tY O U  H A V E  W O N  !!\n");
                                    }
                                    else{
                                        /* draw not implemented*/
                                        printf("\n\n\tYou have LOST.\n");
                                    }
                                    
                                    prepare_message(&snd_msg, OK, 0, 0, 0);
                                    send_message(&snd_msg);

                                    printf("\n\n\tPress ENTER to continue...\n");
                                    wait_for_any_key_press();

                                    pthread_mutex_lock(&conn_status_mutex);
                                    conn_status.terminated_by_game = true;
                                    pthread_mutex_unlock(&conn_status_mutex);

                                    game_state->phase = GAME_END;
                                    game_state->last_comm = OK;
                                }
                                else{
                                    if(victory == 0 && rcv_msg.arg1 == 3 && check_field_full() == true){
                                        printf("\n\n\tIt's a draw!\n");
                                        prepare_message(&snd_msg, OK, 0, 0, 0);
                                        send_message(&snd_msg);

                                        pthread_mutex_lock(&conn_status_mutex);
                                        conn_status.terminated_by_game = true;
                                        pthread_mutex_unlock(&conn_status_mutex);

                                        game_state->phase = GAME_END;
                                        game_state->last_comm = OK;
                                    }
                                    else{
                                        prepare_message(&snd_msg, NO_RESYNC, 0, 0, 0);
                                        send_message(&snd_msg);

                                        if(game_state->role == HOST){
                                            prepare_message(&snd_msg, SYNC_START, 0, 0, 0);
                                            send_message(&snd_msg);

                                            game_state->phase = RESYNC;
                                            game_state->last_comm = SYNC_START;
                                        }
                                        else{
                                            game_state->phase = RESYNC;
                                            game_state->last_comm = NO_RESYNC;
                                        }
                                    }
                                }
                            break;
                            default:
                                mini_log(ERROR, "game", __LINE__, "INVALID MESSAGE RECEIVED (STATE: GAME_TURN)");

                                pthread_mutex_lock(&conn_status_mutex);
                                conn_status.terminated_by_game = true;
                                pthread_mutex_unlock(&conn_status_mutex);
                            break;
                        }
                    break;
                    case GAME_END:
                        /* This peer knows the game is over and waits for confirmation */
                        switch(rcv_msg.communication){
                            case OK:

                                int victory = check_victory();
                                if(victory != 0){
                                    if(victory == game_state->role){
                                        printf("\n\n\tY O U  H A V E  W O N  !!\n");
                                    }
                                    else{
                                        printf("\n\n\tYou have LOST.\n");
                                    }

                                    printf("\n\n\tPress ENTER to continue...\n");
                                    wait_for_any_key_press();

                                    pthread_mutex_lock(&conn_status_mutex);
                                    conn_status.terminated_by_game = true;
                                    pthread_mutex_unlock(&conn_status_mutex);

                                    game_state->phase = GAME_END;
                                    game_state->last_comm = OK;
                                }
                                else{
                                    prepare_message(&snd_msg, NO_RESYNC, 0, 0, 0);
                                    send_message(&snd_msg);

                                    if(game_state->role == HOST){
                                        prepare_message(&snd_msg, SYNC_START, 0, 0, 0);
                                        send_message(&snd_msg);

                                        game_state->phase = RESYNC;
                                        game_state->last_comm = SYNC_START;
                                    }
                                    else{
                                        game_state->phase = RESYNC;
                                        game_state->last_comm = NO_RESYNC;
                                    }
                                }
                            break;
                            case NO_RESYNC:
                                mini_log(ERROR, "game", __LINE__, "RESYNC NOT YET SUPPORTED");

                                pthread_mutex_lock(&conn_status_mutex);
                                conn_status.terminated_by_game = true;
                                pthread_mutex_unlock(&conn_status_mutex);
                            break;
                            default:
                                mini_log(ERROR, "game", __LINE__, "INVALID MESSAGE RECEIVED (STATE: GAME_END)");

                                pthread_mutex_lock(&conn_status_mutex);
                                conn_status.terminated_by_game = true;
                                pthread_mutex_unlock(&conn_status_mutex);
                            break;
                        }
                    break;
                    default:
                        mini_log(ERROR, "game", __LINE__, "GAME_STATE NOT YET SUPPORTED");

                        pthread_mutex_lock(&conn_status_mutex);
                        conn_status.terminated_by_game = true;
                        pthread_mutex_unlock(&conn_status_mutex);
                    break;
                }
            }
        }
    }


    mini_log(LOG, "game", -1, "Waiting for the communication thread to terminate");
    pthread_join(communication_thread_tid, NULL);
    mini_log(LOG, "game", -1, "Communication thread closed");

    pthread_attr_destroy(&communication_thread_attr);
}
