//NOTE:If windows.h need to be included use this define to disable winsock 1.1
//#ifndef WIN32_LEAN_AND_MEAN
//#define WIN32_LEAN_AND_MEAN
//#endif
//#include <windows.h>
#define _WIN32_WINNT 0x501
#include <winsock2.h>
#include <ws2tcpip.h>
//#include <iphlpapi.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <math.h>

#define PORT "27015"
#define DEFAULT_BUFFER_SIZE 512
#define MAX_NUMBER_OF_CLIENTS 2

struct v2
{
    float x;
    float y;
};

float v2_leght(v2 v)
{
    return sqrtf(v.x*v.x+v.y*v.y);
}

struct Game
{
    bool quit = false;
    v2 player_size; 
    v2 player_pos[MAX_NUMBER_OF_CLIENTS];
    v2 player_vel[MAX_NUMBER_OF_CLIENTS];
    
    v2 ball = {800/2, 600/2};
    float ball_rad = 15.0f;
    v2 ball_vel = {1.0f, 0.0f};
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
    WSADATA wsa_data;
    addrinfo* address;
    SOCKET socket;

};

Connection connection_create(const char* ip, const char* port)
{
    Connection c = {};
    int error = WSAStartup(MAKEWORD(2, 2), &c.wsa_data);
    if(error)
    {
        printf("WSAStarup faild: %d\n", error);
    }

    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP; 
    int i_result = 0;
    i_result = getaddrinfo(ip, port, &hints, &c.address);
    if(i_result)
    {
        printf("getaddrinfo failed: %d\n", i_result);
        WSACleanup();
    }

    c.socket = socket(c.address->ai_family, c.address->ai_socktype, c.address->ai_protocol);
    if(c.socket == INVALID_SOCKET)
    {
        printf("Error at socket(): %d\n", WSAGetLastError());
        freeaddrinfo(c.address);
    }

    //NOTE:Connect to server
    i_result = connect(c.socket, c.address->ai_addr, (int)c.address->ai_addrlen);
    if(i_result == SOCKET_ERROR)
    {
        closesocket(c.socket);
        c.socket = INVALID_SOCKET;
    }
    if (c.socket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
    }
    return c;
}

void send_command(SOCKET socket, Command command)
{
    int send_result = send(socket, (const char*)&command, sizeof(Command), 0);
    if(send_result == SOCKET_ERROR)
    {
        printf("send failed: %d\n", WSAGetLastError());
        closesocket(socket);
        WSACleanup();
    }
}

void connection_send_messages(Connection c, bool* keys)
{
    //TODO(tomi):Try to no send invalid command every frame
    if(keys[SDL_SCANCODE_UP])
    {
        printf("up command sent\n");
        send_command(c.socket, {true, MOVE_UP});
        printf("message up\n");
    }
    else if(keys[SDL_SCANCODE_DOWN])
    {
        send_command(c.socket, {true, MOVE_DOWN});
        printf("message down\n");
    }
    //else if(keys[SDL_SCANCODE_RIGHT])
    //{
    //    send_command(c.socket, {true, MOVE_RIGHT});
    //    printf("message right\n");
    //}
    //else if(keys[SDL_SCANCODE_LEFT])
    //{
    //    send_command(c.socket, {true, MOVE_LEFT});
    //    printf("message left\n");
    //}
    else
    {
        send_command(c.socket, {false, MOVE_UP});
    }
}

void connection_update_game(Connection c, Game* game)
{
    int rec_result = recv(c.socket, (char*)game, sizeof(Game), 0);
    if(rec_result < 0)
    {
        printf("Connection clossing\n");
    }

}

void draw_circle(SDL_Renderer* renderer, v2 pos, float rad)
{
    
    
    int w = pos.x + rad;
    int h = pos.y + rad;
    for(int y = pos.y - rad; y < h; y++)
    {
        for(int x = pos.x - rad; x < w; x++)
        {
            v2 distance = {abs(pos.x - x), abs(pos.y - y)};
            if(v2_leght(distance) <= rad)
            {
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
}

void render_game(SDL_Renderer* renderer, Game* game)
{
    SDL_Rect player;
    player.x = game->player_pos[0].x;
    player.y = game->player_pos[0].y;
    player.w = game->player_size.x;
    player.h = game->player_size.y;
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderDrawRect(renderer, &player);

    SDL_Rect player1;
    player1.x = game->player_pos[1].x;
    player1.y = game->player_pos[1].y;
    player1.w = game->player_size.x;
    player1.h = game->player_size.y;
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawRect(renderer, &player1);

    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    draw_circle(renderer, game->ball, game->ball_rad);
}

int main(int argc, char* argv[])
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool keys[256] = {};

    Connection con = connection_create(argv[1], PORT);
    Game game = {};

    window = SDL_CreateWindow("online game", 400, 50, 800, 600, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 0, 0, 10, 255);
    
    bool window_close = false;

    while(!window_close)
    {
        SDL_Event e;
        while(SDL_PollEvent(&e))
        {
            switch(e.type)
            {
                case SDL_QUIT:
                {
                    window_close = true;
                    send_command(con.socket, {true, QUIT});
                    shutdown (con.socket, SD_SEND);
                    closesocket(con.socket);
                    WSACleanup();

                }break;
                case SDL_KEYDOWN:
                {
                    keys[e.key.keysym.scancode] = true;
                }break;
                case SDL_KEYUP:
                {
                    keys[e.key.keysym.scancode] = false;
                }break;
            }
        }
        connection_send_messages(con, keys);
        
        //Recive game_state from server
       connection_update_game(con, &game);
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 10, 255);
        SDL_RenderClear(renderer);
        
        //Render game_state 
        render_game(renderer, &game);
        
        SDL_RenderPresent(renderer);
    }
    
    return 0;
}

