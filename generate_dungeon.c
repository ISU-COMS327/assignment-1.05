#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <math.h>
#include <ncurses.h>

#include "priority_queue.h"

#define HEIGHT 105
#define WIDTH 160
#define IMMUTABLE_ROCK 255
#define ROCK 200
#define ROOM 0
#define CORRIDOR 0
#define MIN_NUMBER_OF_ROOMS 10
#define MAX_NUMBER_OF_ROOMS 50
#define MIN_ROOM_WIDTH 7
#define DEFAULT_MAX_ROOM_WIDTH 15
#define MIN_ROOM_HEIGHT 5
#define DEFAULT_MAX_ROOM_HEIGHT 10
#define DEFAULT_NUMBER_OF_MONSTERS 5

static char * TYPE_ROOM = "room";
static char * TYPE_CORRIDOR = "corridor";
static char * TYPE_ROCK = "rock";

struct Monster {
    uint8_t x;
    uint8_t y;
    uint8_t decimal_type;
    struct Coordinate last_known_player_location;
    uint8_t speed;
};

struct Available_Coords {
    struct Coordinate * coords;
    int length;
};

typedef struct {
    int tunneling_distance;
    int non_tunneling_distance;
    int hardness;
    char * type;
    uint8_t x;
    uint8_t y;
    uint8_t has_player;
    uint8_t has_monster;
    struct Monster monster;
} Board_Cell;

typedef struct {
    Board_Cell * cells;
    int length;
} Neighbors;

struct Room {
    uint8_t start_x;
    uint8_t end_x;
    uint8_t start_y;
    uint8_t end_y;
};

Board_Cell board[HEIGHT][WIDTH];
struct Coordinate placeable_areas[HEIGHT * WIDTH];
struct Room * rooms;
struct Monster * monsters;
struct Coordinate player;
char * RLG_DIRECTORY;
Queue * game_queue;

int PLAYER_IS_ALIVE = 1;
int DO_SAVE = 0;
int DO_LOAD = 0;
int SHOW_HELP = 0;
int NUMBER_OF_ROOMS = MIN_NUMBER_OF_ROOMS;
int MAX_ROOM_WIDTH = DEFAULT_MAX_ROOM_WIDTH;
int MAX_ROOM_HEIGHT = DEFAULT_MAX_ROOM_HEIGHT;
int NUMBER_OF_MONSTERS = DEFAULT_NUMBER_OF_MONSTERS;
int NUMBER_OF_PLACEABLE_AREAS = 0;

void print_usage();
void make_rlg_directory();
void update_number_of_rooms();
int random_int(int min_num, int max_num, int add_to_seed);
void initialize_board();
void initialize_immutable_rock();
void load_board();
void save_board();
void place_player();
void set_placeable_areas();
void set_tunneling_distance_to_player();
void set_non_tunneling_distance_to_player();
void generate_monsters();
void print_non_tunneling_board();
void print_tunneling_board();
void print_board();
void print_cell();
void dig_rooms(int number_of_rooms_to_dig);
void dig_room(int index, int recursive_iteration);
int room_is_valid_at_index(int index);
void add_rooms_to_board();
void dig_cooridors();
void connect_rooms_at_indexes(int index1, int index2);
int get_monster_index(struct Coordinate coord);
void move_player();
void move_monster_at_index(int index);
void kill_player_or_monster_at(struct Coordinate coord);

int main(int argc, char *args[]) {
    int player_x = -1;
    int player_y = -1;
    struct option longopts[] = {
        {"save", no_argument, &DO_SAVE, 1},
        {"load", no_argument, &DO_LOAD, 1},
        {"rooms", required_argument, 0, 'r'},
        {"nummon", required_argument, 0, 'm'},
        {"player_x", required_argument, 0, 'x'},
        {"player_y", required_argument, 0, 'y'},
        {"help", no_argument, &SHOW_HELP, 'h'},
        {0, 0, 0, 0}
    };
    int c;
    while((c = getopt_long(argc, args, "h:", longopts, NULL)) != -1) {
        switch(c) {
            case 'r':
                NUMBER_OF_ROOMS = atoi(optarg);
                break;
            case 'm':
                NUMBER_OF_MONSTERS =  atoi(optarg);
                if (NUMBER_OF_MONSTERS < 1) {
                    NUMBER_OF_MONSTERS = DEFAULT_NUMBER_OF_MONSTERS;
                    printf("Number of monsters cannot be less than 1\n");
                }
                break;
            case 'x':
                player_x = atoi(optarg);
                break;
            case 'y':
                player_y = atoi(optarg);
                break;
            case 'h':
                SHOW_HELP = 1;
                break;
            default:
                break;
        }
    }
    if (SHOW_HELP) {
        print_usage();
        exit(0);
    }
    if ((player_x != -1 || player_y != -1) && ((player_x <= 0 || player_x > WIDTH - 1) || (player_y <= 0 || player_y > HEIGHT - 1))) {
        printf("Invalid player coordinates. Note: both player_x and player_y must be provided as inputs\n");
        print_usage();
        exit(0);
    }
    if (player_y == -1) {
        player_y = 0;
    }
    if (player_x == -1) {
        player_x = 0;
    }
    player.x = player_x;
    player.y = player_y;
    printf("Received Parameters: Save: %d, Load: %d, #Rooms: %d, #NumMon: %d\n\n", DO_SAVE, DO_LOAD, NUMBER_OF_ROOMS, NUMBER_OF_MONSTERS);
    update_number_of_rooms();
    initialize_board();
    make_rlg_directory();

    if (DO_LOAD) {
        load_board();
    }
    else {
        printf("Generating dungeon... \n");
        printf("Making %d rooms.\n", NUMBER_OF_ROOMS);
        rooms = malloc(sizeof(struct Room) * NUMBER_OF_ROOMS);
        dig_rooms(NUMBER_OF_ROOMS);
        dig_cooridors();
    }
    game_queue = create_new_queue(NUMBER_OF_MONSTERS + 1);
    place_player();
    set_placeable_areas();
    set_non_tunneling_distance_to_player();
    set_tunneling_distance_to_player();
    generate_monsters();
    print_board();
    printf("Player location: (%d, %d) (x, y)\n", player.x, player.y);
    int count = 0;
    while(NUMBER_OF_MONSTERS && PLAYER_IS_ALIVE) {
        Node min = extract_min(game_queue);
        int speed;
        if (min.coord.x == player.x && min.coord.y == player.y) {
            speed = 10;
            move_player();
            min.coord.x = player.x;
            min.coord.y = player.y;
            print_board();
            usleep(83333);
            set_non_tunneling_distance_to_player();
            set_tunneling_distance_to_player();
        }
        else {
            int monster_index = get_monster_index(min.coord);
            if (monster_index == -1) {
                continue;
            }
            move_monster_at_index(monster_index);
            struct Monster monster = monsters[monster_index];
            speed = monster.speed;
            min.coord.x = monster.x;
            min.coord.y = monster.y;
        }
        count ++;
        insert_with_priority(game_queue, min.coord, (1000/speed) + min.priority);
    }

    if (!PLAYER_IS_ALIVE) {
        printf("You lost. The monsters killed you\n");
    }
    else if(!NUMBER_OF_MONSTERS) {
        printf("You won, killing all the monsters\n");
    }

    //print_non_tunneling_board();
    //print_tunneling_board();

    if (DO_SAVE) {
        save_board();
    }

    return 0;
}

void update_number_of_rooms() {
    if (NUMBER_OF_ROOMS < MIN_NUMBER_OF_ROOMS) {
        printf("Minimum number of rooms is %d\n", MIN_NUMBER_OF_ROOMS);
        NUMBER_OF_ROOMS = MIN_NUMBER_OF_ROOMS;
    }
    if (NUMBER_OF_ROOMS > MAX_NUMBER_OF_ROOMS) {
        printf("Maximum number of rooms is %d\n", MAX_NUMBER_OF_ROOMS);
        NUMBER_OF_ROOMS = MAX_NUMBER_OF_ROOMS;
    }
}

void make_rlg_directory() {
    char * home = getenv("HOME");
    char dir[] = "/.rlg327/";
    RLG_DIRECTORY = malloc(strlen(home) + strlen(dir));
    strcat(RLG_DIRECTORY, home);
    strcat(RLG_DIRECTORY, dir);
    mkdir(RLG_DIRECTORY, 0777);
}

void save_board() {
    char filename[] = "dungeon";
    char * filepath = malloc(strlen(filename) + strlen(RLG_DIRECTORY));
    strcat(filepath, RLG_DIRECTORY);
    strcat(filepath, filename);
    printf("Saving file to: %s\n", filepath);
    FILE * fp = fopen(filepath, "wb+");
    if (fp == NULL) {
        printf("Cannot save file\n");
        return;
    }
    char * file_marker = "RLG327-S2017";
    uint32_t version = htonl(0);
    uint32_t file_size = htonl(16820 + (NUMBER_OF_ROOMS * 4));

    fwrite(file_marker, 1, strlen(file_marker), fp);
    fwrite(&version, 1, 4, fp);
    fwrite(&file_size, 1, 4, fp);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            uint8_t num = board[y][x].hardness;
            fwrite(&num, 1, 1, fp);
        }
    }

    for (int i = 0; i < NUMBER_OF_ROOMS; i++) {
        struct Room room = rooms[i];
        uint8_t height = room.end_y - room.start_y + 1;
        uint8_t width = room.end_x - room.start_x + 1;
        fwrite(&room.start_x, 1, 1, fp);
        fwrite(&room.start_y, 1, 1, fp);
        fwrite(&(width), 1, 1, fp);
        fwrite(&(height), 1, 1, fp);
    }
    fclose(fp);
}

void load_board() {
    char filename[] = "dungeon";
    char * filepath = malloc(strlen(filename) + strlen(RLG_DIRECTORY));
    strcat(filepath, RLG_DIRECTORY);
    strcat(filepath, filename);
    printf("Loading dungeon: %s\n", filepath);
    FILE *fp = fopen(filepath, "r");
    if (fp == NULL) {
        printf("Cannot load '%s'\n", filepath);
        exit(1);
    }
    char title[13]; // one extra index for the null value at the end
    uint32_t version;
    uint32_t file_size;


    // Get title
    fread(title, 1, 12, fp);

    // Get version
    fread(&version, 4, 1, fp);
    version = ntohl(version);

    // Get file size
    fread(&file_size, 4, 1, fp);
    file_size = ntohl(file_size);

    printf("File Marker: %s :: Version: %d :: File Size: %d bytes\n", title, version, file_size);

    uint8_t num;
    int x = 0;
    int y = 0;
    for (int i = 0; i < 16800; i++) {
        fread(&num, 1, 1, fp);
        Board_Cell cell;
        cell.hardness = num;
        cell.has_monster = 0;
        cell.has_player = 0;
        if (num == 0) {
            cell.type = TYPE_CORRIDOR;
        }
        else {
            cell.type = TYPE_ROCK;
        }
        cell.x = x;
        cell.y = y;
        board[y][x] = cell;
        if (x == WIDTH - 1) {
            x = 0;
            y ++;
        }
        else {
            x ++;
        }
    }

    uint8_t start_x;
    uint8_t start_y;
    uint8_t width;
    uint8_t height;
    NUMBER_OF_ROOMS = (file_size - ftell(fp)) / 4;
    rooms = malloc(sizeof(struct Room) * NUMBER_OF_ROOMS);
    int counter = 0;
    while(ftell(fp) != file_size) {
        fread(&start_x, 1, 1, fp);
        fread(&start_y, 1, 1, fp);
        fread(&width, 1, 1, fp);
        fread(&height, 1, 1, fp);

        struct Room room;
        room.start_x = start_x;
        room.start_y = start_y;
        room.end_x = start_x + width - 1;
        room.end_y = start_y + height - 1;
        rooms[counter] = room;
        counter ++;
    }
    add_rooms_to_board();
    fclose(fp);
}

void print_usage() {
    printf("usage: generate_dungeon [--save] [--load] [--rooms=<number of rooms>] [--player_x=<player x position>] [--player_y=<player y position>] [--nummon=<number of monsters>]\n");
}

int random_int(int min_num, int max_num, int add_to_seed) {
    int seed = time(NULL);
    if (add_to_seed) {
        seed += add_to_seed;
    }
    max_num ++;
    int delta = max_num - min_num;
    srand(seed);
    return (rand() % delta) + min_num;
}

void initialize_board() {
    Board_Cell cell;
    cell.type = TYPE_ROCK;
    cell.has_monster = 0;
    cell.has_player = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            cell.x = x;
            cell.y = y;
            cell.hardness = random_int(1, 254, x + y);
            board[y][x] = cell;
        }
    }
    initialize_immutable_rock();
}

void initialize_immutable_rock() {
    int y;
    int x;
    int max_x = WIDTH - 1;
    int max_y = HEIGHT - 1;
    Board_Cell cell;
    cell.type = TYPE_ROCK;
    cell.hardness = IMMUTABLE_ROCK;
    cell.has_monster = 0;
    cell.has_player = 0;
    for (y = 0; y < HEIGHT; y++) {
        cell.y = y;
        cell.x = 0;
        board[y][0] = cell;
        cell.x = max_x;
        board[y][max_x] = cell;
    }
    for (x = 0; x < WIDTH; x++) {
        cell.y = 0;
        cell.x = x;
        board[0][x] = cell;
        cell.y = max_y;
        board[max_y][x] = cell;
    }
}

void place_player() {
    if (!player.x && !player.y) {
        struct Room room = rooms[0];
        int x = random_int(room.start_x, room.end_x, 100);
        int y = random_int(room.start_y, room.end_y, 500);
        player.x = x;
        player.y = y;
    }
    struct Coordinate coord;
    coord.x = player.x;
    coord.y = player.y;
    insert_with_priority(game_queue, coord, 1000/10);
}

void set_placeable_areas() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Board_Cell cell = board[y][x];
            if (cell.hardness == 0 && cell.x != player.x && cell.y != player.y) {
                struct Coordinate coord;
                coord.x = cell.x;
                coord.y = cell.y;
                placeable_areas[NUMBER_OF_PLACEABLE_AREAS] = coord;
                NUMBER_OF_PLACEABLE_AREAS++;
            }
        }
    }
}

int get_cell_weight(Board_Cell cell) {
    if (cell.hardness == 0) {
        return 1;
    }
    if (cell.hardness <= 84) {
        return 1;
    }
    if (cell.hardness <= 170) {
        return 2;
    }
    if (cell.hardness <= 254) {
        return 3;
    }
    return 1000;
}

int should_add_tunneling_neighbor(Board_Cell cell) {
    return cell.hardness < IMMUTABLE_ROCK;
}

void add_tunneling_neighbor(Neighbors * neighbors, Board_Cell cell) {
    if (!should_add_tunneling_neighbor(cell)) {
        return;
    }
    neighbors->cells[neighbors->length] = cell;
    neighbors->length ++;
}


Neighbors *get_tunneling_neighbors(struct Coordinate coord) {
    int can_go_right = coord.x < WIDTH -1;
    int can_go_up = coord.y > 0;
    int can_go_left = coord.x > 0;
    int can_go_down = coord.y < HEIGHT -1;
    Neighbors *neighbors = malloc(sizeof(Neighbors));
    neighbors->cells = malloc(sizeof(Board_Cell) * 8);

    if (can_go_right) {
        Board_Cell right = board[coord.y][coord.x + 1];
        add_tunneling_neighbor(neighbors, right);
        if (can_go_up) {
            Board_Cell top_right = board[coord.y - 1][coord.x + 1];
            add_tunneling_neighbor(neighbors, top_right);
        }
        if (can_go_down) {
            Board_Cell bottom_right = board[coord.y + 1][coord.x + 1];
            add_tunneling_neighbor(neighbors, bottom_right);
        }
    }
    if (can_go_left) {
        Board_Cell left = board[coord.y][coord.x - 1];
        add_tunneling_neighbor(neighbors, left);
        if (can_go_up) {
            Board_Cell top_left = board[coord.y - 1][coord.x - 1];
            add_tunneling_neighbor(neighbors, top_left);
        }
        if (can_go_down) {
            Board_Cell bottom_left = board[coord.y + 1][coord.x - 1];
            add_tunneling_neighbor(neighbors, bottom_left);
        }
    }

    if (can_go_up) {
        Board_Cell above = board[coord.y - 1][coord.x];
        add_tunneling_neighbor(neighbors, above);
    }
    if (can_go_down) {
        Board_Cell below = board[coord.y + 1][coord.x];
        add_tunneling_neighbor(neighbors, below);
    }

    return neighbors;
}


void set_tunneling_distance_to_player() {
    Queue * tunneling_queue = create_new_queue(HEIGHT * WIDTH);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            struct Coordinate coord;
            coord.x = x;
            coord.y = y;
            if (y == player.y && x == player.x) {
                board[y][x].tunneling_distance = 0;
            }
            else {
                board[y][x].tunneling_distance = INFINITY;
            }
            if (board[y][x].hardness < IMMUTABLE_ROCK) {
                insert_with_priority(tunneling_queue, coord, board[y][x].tunneling_distance);
            }
        }
    }
    int count = 0;
    while(tunneling_queue->length) {
        Node min = extract_min(tunneling_queue);
        Board_Cell min_cell = board[min.coord.y][min.coord.x];
        Neighbors * neighbors = get_tunneling_neighbors(min.coord);
        int min_dist = min_cell.tunneling_distance + get_cell_weight(min_cell);
        for (int i = 0; i < neighbors->length; i++) {
            Board_Cell neighbor_cell = neighbors->cells[i];
            Board_Cell cell = board[neighbor_cell.y][neighbor_cell.x];
            if (min_dist < cell.tunneling_distance) {
                struct Coordinate coord;
                coord.x = cell.x;
                coord.y = cell.y;
                board[cell.y][cell.x].tunneling_distance = min_dist;
                decrease_priority(tunneling_queue, coord, min_dist);
            }
        }
        count ++;
    }
};

int should_add_non_tunneling_neighbor(Board_Cell cell) {
    return cell.hardness < 1;
}

void add_non_tunneling_neighbor(Neighbors * neighbors, Board_Cell cell) {
    if (!should_add_non_tunneling_neighbor(cell)) {
        return;
    }
    neighbors->cells[neighbors->length] = cell;
    neighbors->length ++;
}


Neighbors *get_non_tunneling_neighbors(struct Coordinate coord) {
    int can_go_right = coord.x < WIDTH -1;
    int can_go_up = coord.y > 0;
    int can_go_left = coord.x > 0;
    int can_go_down = coord.y < HEIGHT -1;
    Neighbors *neighbors = malloc(sizeof(Neighbors));
    neighbors->cells = malloc(sizeof(Board_Cell) * 8);

    if (can_go_right) {
        Board_Cell right = board[coord.y][coord.x + 1];
        add_non_tunneling_neighbor(neighbors, right);
        if (can_go_up) {
            Board_Cell top_right = board[coord.y - 1][coord.x + 1];
            add_non_tunneling_neighbor(neighbors, top_right);
        }
        if (can_go_down) {
            Board_Cell bottom_right = board[coord.y + 1][coord.x + 1];
            add_non_tunneling_neighbor(neighbors, bottom_right);
        }
    }
    if (can_go_left) {
        Board_Cell left = board[coord.y][coord.x - 1];
        add_non_tunneling_neighbor(neighbors, left);
        if (can_go_up) {
            Board_Cell top_left = board[coord.y - 1][coord.x - 1];
            add_non_tunneling_neighbor(neighbors, top_left);
        }
        if (can_go_down) {
            Board_Cell bottom_left = board[coord.y + 1][coord.x - 1];
            add_non_tunneling_neighbor(neighbors, bottom_left);
        }
    }

    if (can_go_up) {
        Board_Cell above = board[coord.y - 1][coord.x];
        add_non_tunneling_neighbor(neighbors, above);
    }
    if (can_go_down) {
        Board_Cell below = board[coord.y + 1][coord.x];
        add_non_tunneling_neighbor(neighbors, below);
    }

    return neighbors;
}

void set_non_tunneling_distance_to_player() {
    Queue * non_tunneling_queue = create_new_queue(HEIGHT * WIDTH);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            struct Coordinate coord;
            coord.x = x;
            coord.y = y;
            if (y == player.y && x == player.x) {
                board[y][x].non_tunneling_distance = 0;
            }
            else {
                board[y][x].non_tunneling_distance = INFINITY;
            }
            if (board[y][x].hardness < 1) {
                insert_with_priority(non_tunneling_queue, coord, board[y][x].non_tunneling_distance);
            }
        }
    }
    while(non_tunneling_queue->length) {
        Node min = extract_min(non_tunneling_queue);
        Board_Cell min_cell = board[min.coord.y][min.coord.x];
        Neighbors * neighbors = get_non_tunneling_neighbors(min.coord);
        int min_dist = min_cell.non_tunneling_distance + 1;
        for (int i = 0; i < neighbors->length; i++) {
            Board_Cell neighbor_cell = neighbors->cells[i];
            Board_Cell cell = board[neighbor_cell.y][neighbor_cell.x];
            if (min_dist < cell.non_tunneling_distance) {
                struct Coordinate coord;
                coord.x = cell.x;
                coord.y = cell.y;
                board[cell.y][cell.x].non_tunneling_distance = min_dist;
                decrease_priority(non_tunneling_queue, coord, min_dist);
            }
        }
    }
}

struct Coordinate get_random_board_location(int seed) {
    int index = random_int(0, NUMBER_OF_PLACEABLE_AREAS, seed);
    return placeable_areas[index];
}

void generate_monsters() {
    monsters = malloc(sizeof(struct Monster) * NUMBER_OF_MONSTERS);
    struct Coordinate last_known_player_location;
    last_known_player_location.x = 0;
    last_known_player_location.y = 0;
    for (int i = 0; i < NUMBER_OF_MONSTERS; i++) {
        struct Monster m;
        struct Coordinate coordinate;
        while (1) {
            coordinate = get_random_board_location(i);
            int is_new = 1;
            for (int j = 0; j < i; j++) {
                if (monsters[j].x == coordinate.x && monsters[j].y == coordinate.y && monsters[j].x == player.x && monsters[j].y == player.y) {
                    is_new = 0;
                    break;
                }
            }
            if (is_new) {
                break;
            }
        }
        m.speed = random_int(5, 20, i);
        m.x = coordinate.x;
        m.y = coordinate.y;
        m.last_known_player_location = last_known_player_location;
        m.decimal_type = random_int(0, 15, i + 1);
        board[m.y][m.x].has_monster = 1;
        board[m.y][m.x].monster = m;
        printf("Made %dth monster;x: %d, y: %d, ability: %d, speed: %d\n", i, m.x, m.y, m.decimal_type, m.speed);
        monsters[i] = m;
        insert_with_priority(game_queue, coordinate, 1000/m.speed);
    }
}

void print_non_tunneling_board() {
    printf("Printing non-tunneling board\n");
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
           Board_Cell cell = board[y][x];
           if(cell.x == player.x && cell.y == player.y) {
               printf("@");
           }
           else {
               if (strcmp(cell.type, TYPE_ROCK) != 0) {
                   printf("%d", cell.non_tunneling_distance % 10);
               }
               else {
                    printf(" ");
               }
           }
        }
        printf("\n");
    }
}
void print_tunneling_board() {
    printf("Printing tunneling board\n");
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
           Board_Cell cell = board[y][x];
           if(cell.x == player.x && cell.y == player.y) {
               printf("@");
           }
           else {
               if (cell.hardness == IMMUTABLE_ROCK) {
                   printf(" ");
               }
               else {
                   printf("%d", cell.tunneling_distance % 10);
               }
           }
        }
        printf("\n");
    }

}


void print_board() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (PLAYER_IS_ALIVE && y == player.y && x == player.x) {
                printf("@");
            }
            else if (board[y][x].has_monster == 1) {
                struct Coordinate coord;
                coord.x = x;
                coord.y = y;
                int index = get_monster_index(coord);
                printf("%x", monsters[index].decimal_type);
            }
            else {
                print_cell(board[y][x]);
            }
        }
        printf("\n");
    }
}

void print_cell(Board_Cell cell) {
    if (strcmp(cell.type, TYPE_ROCK) == 0) {
        printf(" ");
    }
    else if (strcmp(cell.type, TYPE_ROOM) == 0) {
        printf(".");
    }
    else if (strcmp(cell.type, TYPE_CORRIDOR) == 0) {
        printf("#");
    }
    else {
        printf("F");
    }
}

void dig_rooms(int number_of_rooms_to_dig) {
    for (int i = 0; i < number_of_rooms_to_dig; i++) {
        dig_room(i, 0);
    }
    add_rooms_to_board();
}

void dig_room(int index, int recursive_iteration) {
    // The index + recusrive_iteration is just a way to gain variety in the
    // random number. The hope is that it makes the program run faster.
    int start_x = random_int(1, WIDTH - MIN_ROOM_WIDTH - 1, index + recursive_iteration);
    int start_y = random_int(1, HEIGHT - MIN_ROOM_HEIGHT - 1, index + recursive_iteration);
    int room_height = random_int(MIN_ROOM_HEIGHT, MAX_ROOM_HEIGHT, index + recursive_iteration);
    int room_width = random_int(MIN_ROOM_WIDTH, MAX_ROOM_WIDTH, index + recursive_iteration);
    int end_y = start_y + room_height;
    if (end_y >= HEIGHT - 1) {
        end_y = HEIGHT - 2;

    }
    int end_x = start_x + room_width;
    if (end_x >= WIDTH - 1) {
        end_x = WIDTH - 2;

    }
    int height = end_y - start_y;
    int height_diff = MIN_ROOM_HEIGHT - height;
    if (height_diff > 0) {
        start_y -= height_diff + 1;
    }

    int width = end_x - start_x;
    int width_diff = MIN_ROOM_WIDTH - width;
    if (width_diff > 0) {
        start_x -= width_diff;
    }
    rooms[index].start_x = start_x;
    rooms[index].start_y = start_y;
    rooms[index].end_x = end_x;
    rooms[index].end_y = end_y;
    if (!room_is_valid_at_index(index)) {
        dig_room(index, recursive_iteration + 1);
    }
}

int room_is_valid_at_index(int index) {
    struct Room room = rooms[index];
    int width = room.end_x - room.start_x;
    int height = room.end_y - room.start_y;
    if (height < MIN_ROOM_HEIGHT || width < MIN_ROOM_WIDTH) {
        return 0;
    }
    for (int i = 0; i < index; i++) {
        struct Room current_room = rooms[i];
        int start_x = current_room.start_x - 1;
        int start_y = current_room.start_y - 1;
        int end_x = current_room.end_x + 1;
        int end_y = current_room.end_y + 1;
        if ((room.start_x >= start_x  && room.start_x <= end_x) ||
                (room.end_x >= start_x && room.end_x <= end_x)) {
            if ((room.start_y >= start_y && room.start_y <= end_y) ||
                    (room.end_y >= start_y && room.end_y <= end_y)) {
                return 0;
            }
        }
    }
    return 1;
}

void add_rooms_to_board() {
    Board_Cell cell;
    cell.type = TYPE_ROOM;
    cell.hardness = ROOM;
    cell.has_monster = 0;
    cell.has_player = 0;
    for(int i = 0; i < NUMBER_OF_ROOMS; i++) {
        struct Room room = rooms[i];
        for (int y = room.start_y; y <= room.end_y; y++) {
            for(int x = room.start_x; x <= room.end_x; x++) {
                cell.x = x;
                cell.y = y;
                board[y][x] = cell;
            }
        }
    }
}

void dig_cooridors() {
    for (int i = 0; i < NUMBER_OF_ROOMS; i++) {
        int next_index = i + 1;
        if (next_index == NUMBER_OF_ROOMS) {
            next_index = 0;
        }
        connect_rooms_at_indexes(i, next_index);
    }
}

void connect_rooms_at_indexes(int index1, int index2) {
    struct Room room1 = rooms[index1];
    struct Room room2 = rooms[index2];
    int start_x = ((room1.end_x - room1.start_x) / 2) + room1.start_x;
    int end_x = ((room2.end_x - room2.start_x) / 2) + room2.start_x;
    int start_y = ((room1.end_y - room1.start_y) / 2) + room1.start_y;
    int end_y = ((room2.end_y - room2.start_y) / 2) + room2.start_y;
    int x_incrementer = 1;
    int y_incrementer = 1;
    if (start_x > end_x) {
        x_incrementer = -1;
    }
    if (start_y > end_y) {
        y_incrementer = -1;
    }
    int cur_x = start_x;
    int cur_y = start_y;
    while(1) {
        int random_num = random_int(0, RAND_MAX, cur_x + cur_y) >> 3;
        int move_y = random_num % 2 == 0;
        if (strcmp(board[cur_y][cur_x].type,  TYPE_ROCK) != 0) {
            if (cur_y != end_y) {
                cur_y += y_incrementer;
            }
            else if(cur_x != end_x) {
                cur_x += x_incrementer;
            }
            else if(cur_y == end_y && cur_x == end_x) {
                break;
            }
            continue;
        }
        Board_Cell corridor_cell;
        corridor_cell.type = TYPE_CORRIDOR;
        corridor_cell.hardness = CORRIDOR;
        corridor_cell.has_monster = 0;
        corridor_cell.has_player = 0;
        corridor_cell.x = cur_x;
        corridor_cell.y = cur_y;
        board[cur_y][cur_x] = corridor_cell;
        if ((cur_y != end_y && move_y) || (cur_x == end_x)) {
            cur_y += y_incrementer;
        }
        else if ((cur_x != end_x && !move_y) || (cur_y == end_y)) {
            cur_x += x_incrementer;
        }
        else {
            break;
        }
    }
}

int get_monster_index(struct Coordinate coord) {
    int index = -1;
    for (int i = 0; i < NUMBER_OF_MONSTERS; i++) {
        struct Monster m = monsters[i];
        if (m.x == coord.x && m.y == coord.y) {
            index = i;
            break;
        }
    }
    return index;
}

struct Available_Coords get_non_tunneling_available_coords_for(struct Coordinate coord) {
    int x = coord.x;
    int y = coord.y;
    struct Available_Coords available_coords;
    struct Coordinate new_coord;
    int size = 0;
    available_coords.length = 0;
    available_coords.coords = malloc(sizeof(struct Coordinate) * 8);
    if (board[y - 1][x].hardness == 0) {
        new_coord.y = y - 1;
        new_coord.x = x;
        available_coords.coords[size] = new_coord;
        size++;
    }
    if (board[y - 1][x - 1].hardness == 0) {
        new_coord.y = y - 1;
        new_coord.x = x - 1;
        available_coords.coords[size] = new_coord;
        size++;
    }
    if(board[y - 1][x + 1].hardness == 0) {
        new_coord.y = y - 1;
        new_coord.x = x + 1;
        available_coords.coords[size] = new_coord;
        size ++;
    }
    if(board[y + 1][x].hardness == 0) {
        new_coord.y = y + 1;
        new_coord.x = x;
        available_coords.coords[size] = new_coord;
        size ++;
    }
    if(board[y + 1][x - 1].hardness == 0) {
        new_coord.y = y + 1;
        new_coord.x = x - 1;
        available_coords.coords[size] = new_coord;
        size ++;
    }
    if(board[y + 1][x + 1].hardness == 0) {
        new_coord.y = y + 1;
        new_coord.x = x + 1;
        available_coords.coords[size] = new_coord;
        size++;
    }
    if(board[y][x - 1].hardness == 0) {
        new_coord.y = y;
        new_coord.x = x - 1;
        available_coords.coords[size] = new_coord;
        size ++;
    }
    if (board[y][x + 1].hardness == 0) {
        new_coord.y = y;
        new_coord.x = x + 1;
        available_coords.coords[size] = new_coord;
        size++;
    }
    available_coords.length = size;
    return available_coords;
}

struct Coordinate get_random_new_non_tunneling_location(struct Coordinate coord) {
    struct Coordinate new_coord;
    struct Available_Coords coords = get_non_tunneling_available_coords_for(coord);
    int new_coord_index = random_int(0, coords.length - 1, coord.x + coord.y);
    struct Coordinate temp_coord = coords.coords[new_coord_index];
    new_coord.x = temp_coord.x;
    new_coord.y = temp_coord.y;
    return new_coord;
}

struct Coordinate get_random_new_tunneling_location(struct Coordinate coord) {
    struct Coordinate new_coord;
    new_coord.x = coord.x;
    new_coord.y = coord.y;
    int max_x = coord.x + 1;
    if (coord.x + 1 >= WIDTH - 1) {
        max_x = coord.x;
    }
    int min_x = coord. x - 1;
    if (min_x <= 1) {
        min_x = coord.x;
    }
    int min_y = coord.y - 1;
    if (min_y <= 1) {
        min_y = coord.y;
    }
    int max_y = coord.y + 1;
    if (max_y >= WIDTH - 1) {
        max_y = coord.y;
    }
    int local_counter = 0;
    while(1) {
        new_coord.x = random_int(min_x, max_x, local_counter);
        new_coord.y = random_int(min_y, max_y, local_counter);
        if (coord.x == new_coord.x && coord.y == new_coord.y) {
            continue;
        }
        if (board[new_coord.y][new_coord.x].hardness != IMMUTABLE_ROCK) {
            break;
        }
        local_counter ++;
    }
    return new_coord;
}


void move_player() {
    int found_monster = 0;
    struct Coordinate new_coord;
    struct Available_Coords coords = get_non_tunneling_available_coords_for(player);
    for (int i = 0; i < coords.length; i++) {
        struct Coordinate current_coord = coords.coords[i];
        if (board[current_coord.y][current_coord.x].has_monster) {
            found_monster = 1;
            new_coord = current_coord;
            break;
        }
    }
    if (!found_monster) {
        new_coord = get_random_new_non_tunneling_location(player);
    }
    if (new_coord.x != player.x || new_coord.y != player.y) {
        kill_player_or_monster_at(new_coord);
    }
    player.x = new_coord.x;
    player.y = new_coord.y;
}

Board_Cell * get_surrounding_cells(struct Coordinate c) {
    Board_Cell * cells = malloc(sizeof(Board_Cell) * 8);
    cells[0] = board[c.y + 1][c.x];
    cells[1] = board[c.y + 1][c.x - 1];
    cells[2] = board[c.y + 1][c.x + 1];
    cells[3] = board[c.y - 1][c.x];
    cells[4] = board[c.y - 1][c.x + 1];
    cells[5] = board[c.y - 1][c.x - 1];
    cells[6] = board[c.y][c.x + 1];
    cells[7] = board[c.y][c.x - 1];
    return cells;
}

Board_Cell get_cell_on_tunneling_path(struct Coordinate c) {
    Board_Cell *cells = get_surrounding_cells(c);
    Board_Cell cell = board[c.y][c.x];
    for (int i = 0; i < 8; i++) {
        Board_Cell current_cell = cells[i];
        if (current_cell.tunneling_distance < cell.tunneling_distance) {
            cell = current_cell;
        }
    }
    return cell;
}


Board_Cell get_cell_on_non_tunneling_path(struct Coordinate c) {
    Board_Cell *cells = get_surrounding_cells(c);
    Board_Cell cell = board[c.y][c.x];
    int min = cell.non_tunneling_distance;
    for (int i = 0; i < 8; i++) {
        Board_Cell my_cell = cells[i];
        if (my_cell.non_tunneling_distance < min) {
            cell = my_cell;
            min = my_cell.non_tunneling_distance;
        }
    }
    return cell;
}

struct Room get_room_player_is_in() {
    struct Room room;
    room.start_x = 0;
    room.end_x = 0;
    room.start_y = 0;
    room.end_y = 0;
    for (int i = 0; i < NUMBER_OF_ROOMS; i++) {
        struct Room current_room = rooms[i];
        if (current_room.start_x <= player.x && player.x <= current_room.end_x) {
            if (current_room.start_y <= player.y && player.y <= current_room.end_y) {
                room = current_room;
                break;
            }
        }
    }
    return room;
}

int monster_is_in_same_room_as_player(int index) {
    struct Monster m = monsters[index];
    struct Room room = get_room_player_is_in();
    if (room.start_x <= m.x && m.x <= room.end_x) {
        if (room.start_y <= m.y && m.y <= room.end_y) {
            return 1;
        }
    }
    return 0;
}

int should_do_erratic_behavior(int index) {
    return random_int(0, 1, index);
}

int monster_knows_last_player_location(int index) {
    struct Monster m = monsters[index];
    struct Coordinate last_known_player_location = m.last_known_player_location;
    return last_known_player_location.x != 0 && last_known_player_location.y != 0;
}

struct Coordinate get_straight_path_to(int index, struct Coordinate coord) {
    struct Monster m = monsters[index];
    struct Coordinate new_coord;
    if (m.x == coord.x) {
        new_coord.x = m.x;
    }
    else if (m.x < coord.x) {
        new_coord.x = m.x + 1;
    }
    else {
        new_coord.x = m.x - 1;
    }

    if (m.y == coord.y) {
        new_coord.y = m.y;
    }
    else if (m.y < coord.y) {
        new_coord.y = m.y + 1;
    }
    else {
        new_coord.y = m.y - 1;
    }

    return new_coord;
}

void kill_monster_at(int index) {
    struct Monster m = monsters[index];
    board[m.y][m.x].has_monster = 0;
    for (int i = index + 1; i < NUMBER_OF_MONSTERS; i++) {
        monsters[i - 1] = monsters[i];
    }
    NUMBER_OF_MONSTERS --;
}

void kill_player_or_monster_at(struct Coordinate coord) {
    int index = get_monster_index(coord);
    if (index >= 0) {
        printf("Monster with ability %d was killed!\n", monsters[index].decimal_type);
        kill_monster_at(index);
    }
    if (player.x == coord.x && player.y == coord.y) {
        PLAYER_IS_ALIVE = 0;
        printf("The player was killed!\n");
    }
}

void move_monster_at_index(int index) {
    struct Monster monster = monsters[index];
    Board_Cell cell = board[monster.y][monster.x];
    struct Coordinate monster_coord;
    monster_coord.x = monster.x;
    monster_coord.y = monster.y;
    struct Coordinate new_coord;
    new_coord.x = monster.x;
    new_coord.y = monster.y;
    board[new_coord.y][new_coord.x].has_monster = 0;
    switch(monster.decimal_type) {
        case 0: // nothing
            if (monster_is_in_same_room_as_player(index)) {
                new_coord = get_straight_path_to(index, player);
            }
            else {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            break;
        case 1: // intelligent
            if (monster_is_in_same_room_as_player(index)) {
                monsters[index].last_known_player_location = player;
                new_coord = get_straight_path_to(index, player);
            }
            else if(monster_knows_last_player_location(index)) {
                new_coord = get_straight_path_to(index, monster.last_known_player_location);
                if (new_coord.x == monster.last_known_player_location.x && new_coord.y == monster.last_known_player_location.y) {
                    monsters[index].last_known_player_location.x = 0;
                    monsters[index].last_known_player_location.y = 0;
                }
            }
            else {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            break;
        case 2: // telepathic
            new_coord = get_straight_path_to(index, player);
            if (board[new_coord.y][new_coord.x].hardness > 0) {
                new_coord.x = monster_coord.x;
                new_coord.y = monster_coord.y;
            }
            break;
        case 3: // telepathic + intelligent
            cell = get_cell_on_non_tunneling_path(new_coord);
            new_coord.x = cell.x;
            new_coord.y = cell.y;
            break;
        case 4: // tunneling
            if (monster_is_in_same_room_as_player(index)) {
                new_coord = get_straight_path_to(index, player);
            }
            else {
                new_coord = get_random_new_tunneling_location(monster_coord);
            }
            cell = board[new_coord.y][new_coord.x];
            if (cell.hardness > 0) {
                board[cell.y][cell.x].hardness -= 85;
                if (board[cell.y][cell.x].hardness <= 0) {
                    board[cell.y][cell.x].hardness = 0;
                    board[cell.y][cell.x].type = TYPE_CORRIDOR;
                    set_non_tunneling_distance_to_player();
                }
                else {
                    new_coord.x = monster.x;
                    new_coord.y = monster.y;
                }
                set_tunneling_distance_to_player();
            }
            break;
        case 5: // tunneling + intelligent
            if (monster_is_in_same_room_as_player(index)) {
                monsters[index].last_known_player_location = player;
                new_coord = get_straight_path_to(index, player);
            }
            else if(monster_knows_last_player_location(index)) {
                new_coord = get_straight_path_to(index, monster.last_known_player_location);
                if (new_coord.x == monster.last_known_player_location.x && new_coord.y == monster.last_known_player_location.y) {
                    monsters[index].last_known_player_location.x = 0;
                    monsters[index].last_known_player_location.y = 0;
                }
            }
            else {
                new_coord = get_random_new_tunneling_location(monster_coord);
                cell = board[new_coord.y][new_coord.x];
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                        set_non_tunneling_distance_to_player();
                    }
                    else {
                        new_coord.x = monster.x;
                        new_coord.y = monster.y;
                    }
                    set_tunneling_distance_to_player();
                }
            }
            break;
        case 6: // tunneling + telepathic
            new_coord = get_straight_path_to(index, player);
            cell = board[new_coord.y][new_coord.x];
            if (cell.hardness > 0) {
                board[cell.y][cell.x].hardness -= 85;
                if (board[cell.y][cell.x].hardness <= 0) {
                    board[cell.y][cell.x].hardness = 0;
                    board[cell.y][cell.x].type = TYPE_CORRIDOR;
                    set_non_tunneling_distance_to_player();
                }
                else {
                    new_coord.x = monster.x;
                    new_coord.y = monster.y;
                }
                set_tunneling_distance_to_player();
            }
            break;
        case 7: // tunneling + telepathic + intelligent
            cell = get_cell_on_tunneling_path(new_coord);
            new_coord.x = cell.x;
            new_coord.y = cell.y;
            if (cell.hardness > 0) {
                board[cell.y][cell.x].hardness -= 85;
                if (board[cell.y][cell.x].hardness <= 0) {
                    board[cell.y][cell.x].hardness = 0;
                    board[cell.y][cell.x].type = TYPE_CORRIDOR;
                    set_non_tunneling_distance_to_player();
                }
                else {
                    new_coord.x = monster.x;
                    new_coord.y = monster.y;
                }
                set_tunneling_distance_to_player();
            }
            break;
        case 8: // erratic
            if (should_do_erratic_behavior(index)) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                if (monster_is_in_same_room_as_player(index)) {
                    new_coord = get_straight_path_to(index, player);
                }
                else {
                    new_coord = get_random_new_non_tunneling_location(monster_coord);
                }
            }
            break;
        case 9: // erratic + intelligent
            if (should_do_erratic_behavior(index)) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                if (monster_is_in_same_room_as_player(index)) {
                    monsters[index].last_known_player_location = player;
                    new_coord = get_straight_path_to(index, player);
                }
                else if(monster_knows_last_player_location(index)) {
                    new_coord = get_straight_path_to(index, monster.last_known_player_location);
                    if (new_coord.x == monster.last_known_player_location.x && new_coord.y == monster.last_known_player_location.y) {
                        monsters[index].last_known_player_location.x = 0;
                        monsters[index].last_known_player_location.y = 0;
                    }
                }
                else {
                    new_coord = get_random_new_non_tunneling_location(monster_coord);
                }
            }
            break;
        case 10: // erratic + telepathic
            if (should_do_erratic_behavior(index)) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                new_coord = get_straight_path_to(index, player);
                if (board[new_coord.y][new_coord.x].hardness != 0) {
                    new_coord.x = monster.x;
                    new_coord.y = monster.y;
                }
            }
            break;
        case 11: // erratic + intelligent + telepathic
            if (should_do_erratic_behavior(index)) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                new_coord = get_straight_path_to(index, player);
                cell = board[new_coord.y][new_coord.x];
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                        set_non_tunneling_distance_to_player();
                    }
                    else {
                        new_coord.x = monster.x;
                        new_coord.y = monster.y;
                    }
                    set_tunneling_distance_to_player();
                }
            }
            break;
        case 12: // erratic + tunneling
            if (should_do_erratic_behavior(index)) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                if (monster_is_in_same_room_as_player(index)) {
                    new_coord = get_straight_path_to(index, player);
                }
                else {
                    new_coord = get_random_new_tunneling_location(monster_coord);
                }
                cell = board[new_coord.y][new_coord.x];
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                        set_non_tunneling_distance_to_player();
                    }
                    else {
                        new_coord.x = monster.x;
                        new_coord.y = monster.y;
                    }
                    set_tunneling_distance_to_player();
                }
            }
            break;
        case 13: // erratic + tunneling + intelligent
            if (should_do_erratic_behavior(index)) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                if (monster_is_in_same_room_as_player(index)) {
                    monsters[index].last_known_player_location = player;
                    new_coord = get_straight_path_to(index, player);
                }
                else if(monster_knows_last_player_location(index)) {
                    new_coord = get_straight_path_to(index, monster.last_known_player_location);
                    if (new_coord.x == monster.last_known_player_location.x && new_coord.y == monster.last_known_player_location.y) {
                        monsters[index].last_known_player_location.x = 0;
                        monsters[index].last_known_player_location.y = 0;
                    }
                }
                else {
                    new_coord = get_random_new_tunneling_location(monster_coord);
                    cell = board[new_coord.y][new_coord.x];
                    if (cell.hardness > 0) {
                        board[cell.y][cell.x].hardness -= 85;
                        if (board[cell.y][cell.x].hardness <= 0) {
                            board[cell.y][cell.x].hardness = 0;
                            board[cell.y][cell.x].type = TYPE_CORRIDOR;
                            set_non_tunneling_distance_to_player();
                        }
                        else {
                            new_coord.x = monster.x;
                            new_coord.y = monster.y;
                        }
                        set_tunneling_distance_to_player();
                    }
                }
            }
            break;
        case 14: // erratic + tunneling + telepathic
            if (should_do_erratic_behavior(index)) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                new_coord = get_straight_path_to(index, player);
                cell = board[new_coord.y][new_coord.x];
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                        set_non_tunneling_distance_to_player();
                    }
                    else {
                        new_coord.x = monster.x;
                        new_coord.y = monster.y;
                    }
                    set_tunneling_distance_to_player();
                }
            }
            break;
        case 15: // erratic + tunneling + telepathic + intelligent
            if (should_do_erratic_behavior(index)) {
                new_coord = get_random_new_non_tunneling_location(monster_coord);
            }
            else {
                cell = get_cell_on_tunneling_path(new_coord);
                new_coord.x = cell.x;
                new_coord.y = cell.y;
                if (cell.hardness > 0) {
                    board[cell.y][cell.x].hardness -= 85;
                    if (board[cell.y][cell.x].hardness <= 0) {
                        board[cell.y][cell.x].hardness = 0;
                        board[cell.y][cell.x].type = TYPE_CORRIDOR;
                        set_non_tunneling_distance_to_player();
                    }
                    else {
                        new_coord.x = monster.x;
                        new_coord.y = monster.y;
                    }
                    set_tunneling_distance_to_player();
                }
            }
            break;
        default:
            printf("Invalid decimal type, %d\n", monster.decimal_type);
            break;
    }
    if (new_coord.x != monster.x || new_coord.y != monster.y) {
        kill_player_or_monster_at(new_coord);
    }
    monsters[index].x = new_coord.x;
    monsters[index].y = new_coord.y;
    board[new_coord.y][new_coord.x].has_monster = 1;
}
