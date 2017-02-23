#include <stdlib.h>
#include <math.h>
#include <stdint.h>

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

Queue *create_new_queue(int max_size) {
   Queue *q = malloc(sizeof(Queue));
   q->length = 0;
   q->nodes = malloc(sizeof(Node) * max_size);
   return q;
}

void insert_with_priority(Queue *q, struct Coordinate coord, int priority) {
    Node node;
    node.coord = coord;
    node.priority = priority;
    if (q->length == 0) {
        q->nodes[0] = node;
        q->length ++;
        return;
    }
    int i;
    int added = 0;
    for (i = 0; i < q->length; i++) {
        Node existing_node = q->nodes[i];
        if (priority <= existing_node.priority) {
            for (int j = q->length; j >= i; j--) {
                Node node_to_shift = q->nodes[j];
                q->nodes[j+1] = node_to_shift;
            }
            q->nodes[i] = node;
            added = 1;
            break;
        }
    }
    if (!added) {
        q->nodes[q->length] = node;
    }
    q->length ++;
}

Node extract_min(Queue * q) {
    Node min = q->nodes[0];
    for(int i = 1; i < q->length; i++) {
        Node node = q->nodes[i];
        q->nodes[i-1] = node;
    }
    q->length --;
    return min;
}

int nodes_are_equal(Node node1, Node node2) {
    return node1.coord.x == node2.coord.x && node1.coord.y == node2.coord.y;
}

void decrease_priority(Queue *q, struct Coordinate coord, int priority) {
    Node node;
    node.coord = coord;
    node.priority = priority;
    int old_index;
    int prev_pri = 0;
    int found = 0;
    for (old_index = 0; old_index < q->length; old_index++) {
       Node existing_node = q->nodes[old_index];
       if (coord.x == existing_node.coord.x && coord.y == existing_node.coord.y){
           prev_pri = existing_node.priority;
           found = 1;
           break;
       }
    }
    if (!found) {
        return;
    }
    int new_index;
    for (new_index = 0; new_index < q->length; new_index++) {
        Node existing_node = q->nodes[new_index];
        if (priority < existing_node.priority) {
            for (int j = old_index - 1; j >= new_index; j--) {
                Node node_to_shift = q->nodes[j];
                q->nodes[j+1] = node_to_shift;
            }
            q->nodes[new_index] = node;
            break;
        }
    }

}
