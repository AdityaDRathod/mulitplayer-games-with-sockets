#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <ctype.h>
#include <stdarg.h>

#define PORT 9000
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define NAME_LEN 50

typedef struct
{
    int socket;
    char name[NAME_LEN];
    int player_id;
    int is_active;
} Client;

typedef struct
{
    int p1_socket;
    int p2_socket;
    char p1_name[NAME_LEN];
    char p2_name[NAME_LEN];
    char game_type[20];
    int game_active;
} GameSession;

int waiting_ttt_sockets[MAX_CLIENTS];
char waiting_ttt_names[MAX_CLIENTS][NAME_LEN];
int waiting_ttt_count = 0;

int waiting_rps_sockets[MAX_CLIENTS];
char waiting_rps_names[MAX_CLIENTS][NAME_LEN];
int waiting_rps_count = 0;

Client clients[MAX_CLIENTS];
int total_clients = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int send_msg(int socket, const char *msg)
{
    if (!socket)
        return -1;
    return send(socket, msg, strlen(msg), 0);
}

int sendf(int sock, const char *fmt, ...)
{
    char buf[BUFFER_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return send_msg(sock, buf);
}

int recv_msg(int socket, char *buffer, size_t max)
{
    memset(buffer, 0, max);
    ssize_t n = recv(socket, buffer, max - 1, 0);
    if (n > 0)
    {
        buffer[n] = '\0';
        return (int)n;
    }
    return 0;
}

void display_ttt_board(char board[9], char *output)
{
    sprintf(output,
            "\n┌───┬───┬───┐\n"
            "│ %c │ %c │ %c │\n"
            "├───┼───┼───┤\n"
            "│ %c │ %c │ %c │\n"
            "├───┼───┼───┤\n"
            "│ %c │ %c │ %c │\n"
            "└───┴───┴───┘\n\n",
            board[0], board[1], board[2],
            board[3], board[4], board[5],
            board[6], board[7], board[8]);
}

int check_ttt_win(char board[9])
{
    int win_combinations[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};

    for (int i = 0; i < 8; i++)
    {
        if (board[win_combinations[i][0]] != ' ' &&
            board[win_combinations[i][0]] == board[win_combinations[i][1]] &&
            board[win_combinations[i][1]] == board[win_combinations[i][2]])
        {
            return 1;
        }
    }
    return 0;
}

int check_ttt_full(char board[9])
{
    for (int i = 0; i < 9; i++)
    {
        if (board[i] == ' ')
            return 0;
    }
    return 1;
}

void play_ttt(GameSession *session)
{
    char board[9];
    for (int i = 0; i < 9; i++)
        board[i] = ' ';

    char display[512];
    char msg[BUFFER_SIZE];
    char input[BUFFER_SIZE];

    sprintf(msg, "\n========== TIC-TAC-TOE ==========\n");
    send_msg(session->p1_socket, msg);
    send_msg(session->p2_socket, msg);

    sprintf(msg, "Player 1: %s (X)\nPlayer 2: %s (O)\n\n", session->p1_name, session->p2_name);
    send_msg(session->p1_socket, msg);
    send_msg(session->p2_socket, msg);

    int turn = 1;

    while (1)
    {
        display_ttt_board(board, display);
        send_msg(session->p1_socket, display);
        send_msg(session->p2_socket, display);

        int current_socket = (turn == 1) ? session->p1_socket : session->p2_socket;
        int other_socket = (turn == 1) ? session->p2_socket : session->p1_socket;
        char mark = (turn == 1) ? 'X' : 'O';

        sprintf(msg, "[%s's Turn] Enter cell (1-9): ", (turn == 1) ? session->p1_name : session->p2_name);
        send_msg(current_socket, msg);
        send_msg(other_socket, msg);

        if (recv_msg(current_socket, input, BUFFER_SIZE) <= 0)
        {
            send_msg(other_socket, "\n[Game] Other player disconnected. Game over.\n");
            break;
        }

        int cell = atoi(input);

        if (cell < 1 || cell > 9 || board[cell - 1] != ' ')
        {
            sprintf(msg, "[Invalid Move] Cell %d is invalid or occupied. Try again.\n\n", cell);
            send_msg(current_socket, msg);
            continue;
        }

        board[cell - 1] = mark;
        sprintf(msg, "[Move] %s placed %c at cell %d\n\n", (turn == 1) ? session->p1_name : session->p2_name, mark, cell);
        send_msg(session->p1_socket, msg);
        send_msg(session->p2_socket, msg);

        if (check_ttt_win(board))
        {
            display_ttt_board(board, display);
            send_msg(session->p1_socket, display);
            send_msg(session->p2_socket, display);
            sprintf(msg, "\n🎉 [WINNER] %s (%c) WINS! 🎉\n\n", (turn == 1) ? session->p1_name : session->p2_name, mark);
            send_msg(session->p1_socket, msg);
            send_msg(session->p2_socket, msg);
            break;
        }

        if (check_ttt_full(board))
        {
            display_ttt_board(board, display);
            send_msg(session->p1_socket, display);
            send_msg(session->p2_socket, display);
            send_msg(session->p1_socket, "\n🤝 [DRAW] Game ended in a draw! 🤝\n\n");
            send_msg(session->p2_socket, "\n🤝 [DRAW] Game ended in a draw! 🤝\n\n");
            break;
        }

        turn = (turn == 1) ? 2 : 1;
    }
}

void play_rps(GameSession *session)
{
    char msg[BUFFER_SIZE];
    char input[BUFFER_SIZE];
    int p1_score = 0, p2_score = 0;

    sprintf(msg, "\n========== ROCK-PAPER-SCISSORS (BEST OF 3) ==========\n");
    send_msg(session->p1_socket, msg);
    send_msg(session->p2_socket, msg);

    sprintf(msg, "Player 1: %s\nPlayer 2: %s\n\n", session->p1_name, session->p2_name);
    send_msg(session->p1_socket, msg);
    send_msg(session->p2_socket, msg);

    for (int round = 1; round <= 3; round++)
    {
        sprintf(msg, "\n=== ROUND %d ===\n", round);
        send_msg(session->p1_socket, msg);
        send_msg(session->p2_socket, msg);

        sprintf(msg, "Current Score: %s: %d | %s: %d\n\n", session->p1_name, p1_score, session->p2_name, p2_score);
        send_msg(session->p1_socket, msg);
        send_msg(session->p2_socket, msg);

        sendf(session->p1_socket, "[Waiting for both players...]\n%s, choose (rock/paper/scissors): ", session->p1_name);
        sendf(session->p2_socket, "[Waiting for both players...]\n%s, choose (rock/paper/scissors): ", session->p2_name);

        char p1_choice[20], p2_choice[20];

        if (recv_msg(session->p1_socket, input, BUFFER_SIZE) <= 0)
        {
            send_msg(session->p2_socket, "\nOther player disconnected.\n");
            break;
        }
        strncpy(p1_choice, input, sizeof(p1_choice) - 1);
        p1_choice[sizeof(p1_choice) - 1] = '\0';

        if (recv_msg(session->p2_socket, input, BUFFER_SIZE) <= 0)
        {
            send_msg(session->p1_socket, "\nOther player disconnected.\n");
            break;
        }
        strncpy(p2_choice, input, sizeof(p2_choice) - 1);
        p2_choice[sizeof(p2_choice) - 1] = '\0';

        p1_choice[strcspn(p1_choice, "\r\n")] = 0;
        p2_choice[strcspn(p2_choice, "\r\n")] = 0;

        for (int i = 0; p1_choice[i]; i++)
            p1_choice[i] = tolower((unsigned char)p1_choice[i]);
        for (int i = 0; p2_choice[i]; i++)
            p2_choice[i] = tolower((unsigned char)p2_choice[i]);

        sprintf(msg, "\n%s chose: %s\n%s chose: %s\n", session->p1_name, p1_choice, session->p2_name, p2_choice);
        send_msg(session->p1_socket, msg);
        send_msg(session->p2_socket, msg);

        int winner = 0;

        if (strcmp(p1_choice, p2_choice) == 0)
        {
            winner = 0;
        }
        else if (strcmp(p1_choice, "rock") == 0 && strcmp(p2_choice, "scissors") == 0)
        {
            winner = 1;
        }
        else if (strcmp(p1_choice, "paper") == 0 && strcmp(p2_choice, "rock") == 0)
        {
            winner = 1;
        }
        else if (strcmp(p1_choice, "scissors") == 0 && strcmp(p2_choice, "paper") == 0)
        {
            winner = 1;
        }
        else
        {
            winner = 2;
        }

        if (winner == 0)
        {
            sprintf(msg, "\n⚖️ [DRAW] Round %d is a Draw!\n", round);
            send_msg(session->p1_socket, msg);
            send_msg(session->p2_socket, msg);
        }
        else if (winner == 1)
        {
            p1_score++;
            sprintf(msg, "\n🏆 [ROUND %d] %s WINS this round!\n", round, session->p1_name);
            send_msg(session->p1_socket, msg);
            send_msg(session->p2_socket, msg);
        }
        else
        {
            p2_score++;
            sprintf(msg, "\n🏆 [ROUND %d] %s WINS this round!\n", round, session->p2_name);
            send_msg(session->p1_socket, msg);
            send_msg(session->p2_socket, msg);
        }
    }

    sprintf(msg, "\n\n========== FINAL RESULT ==========\n");
    send_msg(session->p1_socket, msg);
    send_msg(session->p2_socket, msg);

    sprintf(msg, "%s: %d\n%s: %d\n\n", session->p1_name, p1_score, session->p2_name, p2_score);
    send_msg(session->p1_socket, msg);
    send_msg(session->p2_socket, msg);

    if (p1_score > p2_score)
    {
        sprintf(msg, "🎉 %s WINS THE MATCH! 🎉\n\n", session->p1_name);
    }
    else if (p2_score > p1_score)
    {
        sprintf(msg, "🎉 %s WINS THE MATCH! 🎉\n\n", session->p2_name);
    }
    else
    {
        sprintf(msg, "🤝 The match is a TIE! 🤝\n\n");
    }
    send_msg(session->p1_socket, msg);
    send_msg(session->p2_socket, msg);
}

void *handle_game_session(void *arg)
{
    GameSession *session = (GameSession *)arg;
    char msg[BUFFER_SIZE];
    char choice1[32], choice2[32];

    sendf(session->p1_socket, "\nMatched with %s! Starting %s...\n", session->p2_name, session->game_type);
    sendf(session->p2_socket, "\nMatched with %s! Starting %s...\n", session->p1_name, session->game_type);

    while (1)
    {
        if (strcmp(session->game_type, "TTT") == 0)
        {
            play_ttt(session);
        }
        else
        {
            play_rps(session);
        }

        send_msg(session->p1_socket, "\n=== PLAY AGAIN? ===\nPlay again? (yes/no): ");
        send_msg(session->p2_socket, "\n=== PLAY AGAIN? ===\nPlay again? (yes/no): ");

        if (recv_msg(session->p1_socket, choice1, sizeof(choice1)) <= 0)
        {
            send_msg(session->p2_socket, "\nOther player disconnected.\n");
            break;
        }
        if (recv_msg(session->p2_socket, choice2, sizeof(choice2)) <= 0)
        {
            send_msg(session->p1_socket, "\nOther player disconnected.\n");
            break;
        }

        for (int i = 0; choice1[i]; i++)
            choice1[i] = tolower((unsigned char)choice1[i]);
        for (int i = 0; choice2[i]; i++)
            choice2[i] = tolower((unsigned char)choice2[i]);
        choice1[strcspn(choice1, "\r\n")] = 0;
        choice2[strcspn(choice2, "\r\n")] = 0;

        if ((strstr(choice1, "yes") || strstr(choice1, "y")) &&
            (strstr(choice2, "yes") || strstr(choice2, "y")))
        {
            continue;
        }
        else
        {
            send_msg(session->p1_socket, "\nThanks for playing! Goodbye.\n");
            send_msg(session->p2_socket, "\nThanks for playing! Goodbye.\n");
            break;
        }
    }

    close(session->p1_socket);
    close(session->p2_socket);
    free(session);
    return NULL;
}

void *handle_client_connection(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);

    char name[NAME_LEN];
    char choice[32];

    send_msg(client_socket, "=== WELCOME TO GAME HUB ===\n");
    send_msg(client_socket, "Enter your name: ");
    if (recv_msg(client_socket, name, NAME_LEN) <= 0)
    {
        close(client_socket);
        return NULL;
    }
    name[strcspn(name, "\r\n")] = 0;

    send_msg(client_socket, "\n=== GAME MENU ===\n1. Tic-Tac-Toe\n2. Rock-Paper-Scissors\n\nEnter choice (1 or 2): ");
    if (recv_msg(client_socket, choice, sizeof(choice)) <= 0)
    {
        close(client_socket);
        return NULL;
    }
    choice[strcspn(choice, "\r\n")] = 0;

    for (int i = 0; choice[i]; i++)
        choice[i] = tolower((unsigned char)choice[i]);

    pthread_mutex_lock(&clients_mutex);

    if (strcmp(choice, "1") == 0 || strstr(choice, "tic"))
    {
        if (waiting_ttt_count > 0)
        {
            int p1_sock = waiting_ttt_sockets[0];
            char p1_name[NAME_LEN];
            strncpy(p1_name, waiting_ttt_names[0], NAME_LEN);
            for (int i = 1; i < waiting_ttt_count; i++)
            {
                waiting_ttt_sockets[i - 1] = waiting_ttt_sockets[i];
                strncpy(waiting_ttt_names[i - 1], waiting_ttt_names[i], NAME_LEN);
            }
            waiting_ttt_count--;

            GameSession *session = malloc(sizeof(GameSession));
            session->p1_socket = p1_sock;
            session->p2_socket = client_socket;
            strncpy(session->p1_name, p1_name, NAME_LEN);
            strncpy(session->p2_name, name, NAME_LEN);
            strcpy(session->game_type, "TTT");
            pthread_mutex_unlock(&clients_mutex);

            pthread_t gthread;
            pthread_create(&gthread, NULL, handle_game_session, session);
            pthread_detach(gthread);
            return NULL;
        }
        else
        {
            waiting_ttt_sockets[waiting_ttt_count] = client_socket;
            strncpy(waiting_ttt_names[waiting_ttt_count], name, NAME_LEN);
            waiting_ttt_count++;
            pthread_mutex_unlock(&clients_mutex);

            send_msg(client_socket, "\nWaiting for an opponent who chose Tic-Tac-Toe...\n");
            return NULL;
        }
    }
    else if (strcmp(choice, "2") == 0 || strstr(choice, "rock"))
    {
        if (waiting_rps_count > 0)
        {
            int p1_sock = waiting_rps_sockets[0];
            char p1_name[NAME_LEN];
            strncpy(p1_name, waiting_rps_names[0], NAME_LEN);
            for (int i = 1; i < waiting_rps_count; i++)
            {
                waiting_rps_sockets[i - 1] = waiting_rps_sockets[i];
                strncpy(waiting_rps_names[i - 1], waiting_rps_names[i], NAME_LEN);
            }
            waiting_rps_count--;

            GameSession *session = malloc(sizeof(GameSession));
            session->p1_socket = p1_sock;
            session->p2_socket = client_socket;
            strncpy(session->p1_name, p1_name, NAME_LEN);
            strncpy(session->p2_name, name, NAME_LEN);
            strcpy(session->game_type, "RPS");
            pthread_mutex_unlock(&clients_mutex);

            pthread_t gthread;
            pthread_create(&gthread, NULL, handle_game_session, session);
            pthread_detach(gthread);
            return NULL;
        }
        else
        {
            waiting_rps_sockets[waiting_rps_count] = client_socket;
            strncpy(waiting_rps_names[waiting_rps_count], name, NAME_LEN);
            waiting_rps_count++;
            pthread_mutex_unlock(&clients_mutex);

            send_msg(client_socket, "\nWaiting for an opponent who chose Rock-Paper-Scissors...\n");
            return NULL;
        }
    }
    else
    {
        pthread_mutex_unlock(&clients_mutex);
        send_msg(client_socket, "\nInvalid choice. Disconnecting.\n");
        close(client_socket);
        return NULL;
    }
}

int main()
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    int yes = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_socket, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        return 1;
    }

    printf("🎮 GAME HUB SERVER STARTED on PORT %d 🎮\n", PORT);
    printf("Waiting for players...\n\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int *client_socket = (int *)malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);

        if (*client_socket < 0)
        {
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client_connection, client_socket);
        pthread_detach(thread);
    }

    close(server_socket);
    return 0;
}
