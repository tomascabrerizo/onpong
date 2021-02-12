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
#include <queue>
#include <stdlib.h>

#define PORT "27015"
#define MAX_NUMBER_OF_CLIENTS 2

struct v2
{
    float x;
    float y;
};

struct Game
{
    bool quit = false;
    v2 player_size = {20.0f, 70.0f}; 
    v2 player_pos[MAX_NUMBER_OF_CLIENTS];
    v2 player_vel[MAX_NUMBER_OF_CLIENTS];
    float ball_rad = 15.0f;
    v2 ball = {800/2, 600/2};
};

enum game_commads
{
    MOVE_UP = 0,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
    
    QUIT,
};

struct Command
{
    bool is_valid;
    game_commads command;
};

struct TS_queue
{
    CRITICAL_SECTION critical_section;
    std::queue<Command> queue;
    
    TS_queue()
    {
        // Initialize the critical section one time only.
        if (!InitializeCriticalSectionAndSpinCount(&critical_section, 
            0x00000400) ) 
            return;
    }
    
    ~TS_queue()
    {
        DeleteCriticalSection(&critical_section);
    }

    void push(Command command)
    {
        EnterCriticalSection(&critical_section); 
        queue.push(command);
        LeaveCriticalSection(&critical_section);
    }
    
    Command pop()
    {
        EnterCriticalSection(&critical_section); 
        Command c = queue.front();
        queue.pop();
        LeaveCriticalSection(&critical_section);
        return c;
    }

    int size()
    {
        EnterCriticalSection(&critical_section); 
        int size = queue.size();
        LeaveCriticalSection(&critical_section);
        return size;
    }
};


struct Connection
{
    SOCKET socket;
    TS_queue command_queue; //NOTE(tomi):Thread safe command queue
    DWORD thread_id;
    HANDLE thread;
};

DWORD WINAPI init_client_thread(LPVOID lpParam) // LPVOID you should cast a data struct
{
    for(;;)
    {
        Connection* connection = (Connection*)lpParam;
        Command command;
        int rec_resul = recv(connection->socket, (char*)&command, sizeof(Command), 0);
        if(rec_resul > 0)
        {

            if(command.is_valid)
            {
                if(command.command == QUIT)
                {
                    printf("thread end asd\n");
                    connection->command_queue.push(command);
                    shutdown(connection->socket, SD_SEND);
                    closesocket(connection->socket);
                    return 0;
                }
                else
                {
                    connection->command_queue.push(command);
                }
            }
        }
    }
    return 0;
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
            //NOTE(tomi):Start thread with a queue to puts commands
            c->thread = CreateThread(
                    0, 
                    0,
                    init_client_thread,
                    (void*)c, 
                    0,
                    &c->thread_id);
            
            if (c->thread == NULL)
            {
                printf("Error creating thread!\n");
                return;
            }
        }
        *client = *client+1;
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
    float speed = 10.0f; 
    //NOTE(tomi):Process queue commads
    for(int client = 0; client < MAX_NUMBER_OF_CLIENTS; client++)
    {
        if(s->connection[client].command_queue.size() > 0 )
        {
            Command command = s->connection[client].command_queue.pop();
            if(command.is_valid)
            {
                switch(command.command)
                {
                    case MOVE_UP:
                    {
                        game->player_vel[client]= {0.0f, -1.0f};
                    }break;
                    case MOVE_DOWN:
                    {
                        game->player_vel[client]= {0.0f, 1.0f};
                    }break;
                    case MOVE_LEFT:
                    {
                        game->player_vel[client]= {-1.0f, 0.0f};
                    }break;
                    case MOVE_RIGHT:
                    {
                        game->player_vel[client]= {1.0f, 0.0f};
                    }break;
                    case QUIT:
                    {
                        if(s->number_clinet_connected > 0)
                            s->number_clinet_connected = s->number_clinet_connected-1; 
                    }break;
                }
                game->player_pos[client].x += game->player_vel[client].x * speed; 
                game->player_pos[client].y += game->player_vel[client].y * speed;
            }
        }
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
