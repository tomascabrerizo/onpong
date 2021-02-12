//NOTE:If windows.h need to be included use this define to disable winsock 1.1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#include <winsock2.h>
#include <ws2tcpip.h>
//#include <iphlpapi.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#define PORT "27015"
#define MAX_NUMBER_OF_CLIENTS 2

struct v2
{
    float x;
    float y;
};

v2 operator+(const v2& v0, const v2& v1)
{
    return {v0.x+v1.x, v0.y+v1.y};
}

v2 operator-(const v2& v0, const v2& v1)
{
    return {v0.x-v1.x, v0.y-v1.y};
}

v2 operator-(const v2& v)
{
    return {-v.x, -v.y};
}

v2 operator*(const v2& v, float s)
{
    return {v.x*s, v.y*s};
}

float v2_leght(v2 v)
{
    return sqrtf(v.x*v.x+v.y*v.y);
}

struct Game
{
    bool quit = false;
    v2 player_size = {25.0f, 100.0f}; 
    v2 player_pos[MAX_NUMBER_OF_CLIENTS];
    v2 player_vel[MAX_NUMBER_OF_CLIENTS];
    
    v2 ball = {800/2, 600/2};
    float ball_rad = 10.0f;
    v2 ball_vel = {1.0f, 0.35f};
};

enum game_commads
{
    MOVE_UP = 0,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
};

struct Command
{
    bool is_valid;
    game_commads command;
};

struct Connection
{
    SOCKET socket;
    DWORD thread_id;
    HANDLE thread;
};

void connection_get_message(Connection* c, v2* player_vel)
{
    Command command;
    int rec_resul = recv(c->socket, (char*)&command, sizeof(Command), 0);
    if(rec_resul > 0)
    {
        if(command.is_valid)
        {
            switch(command.command)
            {
                case MOVE_UP:
                {
                    *player_vel = {0.0f, -1.0f};
                }break;
                case MOVE_DOWN:
                {
                    *player_vel = {0.0f, 1.0f};
                }break;
                case MOVE_LEFT:
                {
                    *player_vel = {-1.0f, 0.0f};
                }break;
                case MOVE_RIGHT:
                {
                    *player_vel = {1.0f, 0.0f};
                }break;
            }
        }
    }

}

struct Server 
{
    WSADATA wsa_data;
    addrinfo* address;
    SOCKET listen_socket;
    
    int number_clinet_connected = 0;
    Connection connection[MAX_NUMBER_OF_CLIENTS];
};

void accept_clients(Server* s)
{

    int* client = &(s->number_clinet_connected);
    while(*client < MAX_NUMBER_OF_CLIENTS)
    {
        Connection* c = &s->connection[*client];
        c->socket = accept(s->listen_socket, 0, 0);
        if(c->socket == INVALID_SOCKET)
        {
            printf("accept fail: %d\n", WSAGetLastError());
            closesocket(s->listen_socket);
            WSACleanup();
            return;
        }
        else
        {
            printf("client%d just connect\n", s->number_clinet_connected);
            *client = *client+1;
            //NOTE(tomi):Start thread with a queue to puts commands
            //c->thread = CreateThread(0, 0, init_client_thread, (void*)c, 0, &c->thread_id);
            //if (c->thread == NULL)
            //{
            //    printf("Error creating thread!\n");
            //    return;
            //}
        }
    }
}

Server server_create(const char* port)
{
    Server s = {};
    
    //NOTE:Init use of ws2_32.dll
    int error = WSAStartup(MAKEWORD(2, 2), &s.wsa_data);
    //NOTE:MAKEWORD(2, 2) set the version 2.2 of winsock
    if(error)
    {
        printf("WSAStarup faild: %d\n", error);
    }
    
    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    int i_resutl = 0;
    i_resutl = getaddrinfo(0, port, &hints, &s.address);
    if(i_resutl)
    {
        printf("Getaddrinfo faild: %d\n", i_resutl);
        WSACleanup();
    }
     
    s.listen_socket = socket(s.address->ai_family, s.address->ai_socktype, s.address->ai_protocol);
    if(s.listen_socket == INVALID_SOCKET)
    {
        printf("Error at socket: %d\n", WSAGetLastError());
        freeaddrinfo(s.address);
        WSACleanup();
    }

    //NOTE:Bind the socket to the created addr
    i_resutl = bind(s.listen_socket, s.address->ai_addr, (int)s.address->ai_addrlen);
    if(i_resutl == SOCKET_ERROR)
    {
        printf("Bind fail with error: %d\n", i_resutl);
        freeaddrinfo(s.address);
        WSACleanup();
    }

    return s;

}

void update_game(Game* game, Server* s)
{
    float speed = 15.0f; 
    float ball_speed = 20.0f;
    (void)ball_speed;
    //NOTE(tomi):Process queue commads
    for(int client = 0; client < MAX_NUMBER_OF_CLIENTS; client++)
    {
        game->player_vel[client] = {};
        connection_get_message(&s->connection[client], &game->player_vel[client]);
        game->player_pos[client].x += game->player_vel[client].x * speed; 
        game->player_pos[client].y += game->player_vel[client].y * speed;
    }
   

    if(game->ball.x-game->ball_rad < 0 || game->ball.x+game->ball_rad > 800)
        game->ball_vel.x = -game->ball_vel.x;
    if(game->ball.y-game->ball_rad < 0 || game->ball.y+game->ball_rad > 600)
        game->ball_vel.y = -game->ball_vel.y;

    game->ball = game->ball + game->ball_vel * ball_speed;

    //NOTE(tomi):Collision with the player
    for(int client = 0; client < MAX_NUMBER_OF_CLIENTS; client++)
    {
        int x0 = game->player_pos[client].x; 
        int x1 = game->player_pos[client].x + game->player_size.x;
        int y0 = game->player_pos[client].y;
        int y1 = game->player_pos[client].y + game->player_size.y;
        
        int bx0 = game->ball.x - game->ball_rad;
        int bx1 = game->ball.x + game->ball_rad;
        int by0 = game->ball.y - game->ball_rad;
        int by1 = game->ball.y + game->ball_rad;
        
        if((bx0 <= x1 && bx1 >= x0) && (by0 <= y1 && by1 >= y0))
            game->ball_vel.x = -game->ball_vel.x;

    }

}

void send_game(SOCKET socket, Game game)
{
    int send_result = send(socket, (const char*)&game, sizeof(Game), 0);
    if(send_result == SOCKET_ERROR)
    {
        printf("Send fail: %d\n", WSAGetLastError());
        closesocket(socket);
        WSACleanup();
    }
}

int main()
{
    //TODO(tomi):Probably remove clients threads
    //TODO(tomi):Fix Desconections/Reconnections to server

    Game game = {};
    Server server = {};
    game.player_pos[0] = {
        800/2 - game.player_size.x/2 - (800/2 - game.player_size.x - 20), 
        600/2 - game.player_size.y/2
    };
    game.player_pos[1] = {
        800/2 - game.player_size.x/2 + (800/2 - game.player_size.x - 20),
        600/2 - game.player_size.y/2};

    server = server_create(PORT);

    if(listen(server.listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        printf( "Listen failed with error: %d\n", WSAGetLastError() );
        closesocket(server.listen_socket);
        WSACleanup();
        return -1;
    }
     
    accept_clients(&server);
    while(server.number_clinet_connected == 2)
    {
        update_game(&game, &server);
        send_game(server.connection[0].socket, game);
        send_game(server.connection[1].socket, game);
        Sleep(16);
    }
}
