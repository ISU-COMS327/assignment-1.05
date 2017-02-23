struct Coordinate {
    uint8_t x;
    uint8_t y;
};

typedef struct {
    int distance;
    int priority;
    struct Coordinate coord;
} Node;

typedef struct {
    int length;
    Node * nodes;
} Queue;

Queue * create_new_queue(int max_size);
void insert_with_priority(Queue *q, struct Coordinate coord, int priority);
Node extract_min(Queue * q);
void decrease_priority(Queue *q, struct Coordinate coord, int priority);
