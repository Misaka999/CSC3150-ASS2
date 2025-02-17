#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <map>

/* const numbers define */
#define ROW 17
#define COLUMN 49
#define NUM_WALLS 6
#define NUM_GOLDS 6
#define HORI_LINE '-'
#define VERT_LINE '|'
#define CORNER '+'
#define PLAYER '0'
#define WALL '='
#define GOLD '$'

pthread_mutex_t mutex;

/* global variables */
int player_x;
int player_y;
char map[ROW][COLUMN + 1];

int state = 0;  // track game state (quit:1, win:2, lose:3)
int isover = 0;	// control the game processes
int count = 0; // number of collected gold shards
int gold_directions[NUM_GOLDS];  // store the fixed movement direction of each gold shards
std::map<int, int> gold_positions;   // store the positions of each gold shard (x, y)

/* functions */
int kbhit(void);
void map_print(void);
void *move_gold(void *arg);
void *move_wall(void *t);
void *move_player(void *t);
void *screen_print(void *arg);

/* Determine a keyboard is hit or not.
 * If yes, return 1. If not, return 0. */
int kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}


void *move_wall(void *t){	
	//t:(0~5)
	int wall_length = 15;
	long row_index;
	int i, current;
    long wall_index = (long)t;

    // Determine the row for the current wall based on its index
    if (wall_index <= 2){
        row_index = (long)(wall_index*2+2);
    }
    else{
        row_index = (long)(wall_index*2+4);
    }

	srand(time(0) + (unsigned long)pthread_self());
	int start = rand() % 47 + 1; 

	while(!isover){
		pthread_mutex_lock(&mutex);

        // Clear the current row (except the position where player is)
		for (i = 1; i < 48; i++){
            if (map[row_index][i]!=PLAYER){
			    map[row_index][i] = ' ';
            }
		}

        // rows 4, 10, 14: move left
		if ((row_index == 4) or (row_index == 10) or (row_index == 14)){  
			if (row_index != player_x){ // player is not on the same row as the wall
				current = start;
				for (i = 0; i < wall_length; i++){
					map[row_index][current] = WALL;
					current = (current == 47) ? 1 : current + 1; // Loop around at edges
				}
			}
			else{  // player is on the same row as the wall, handle collisions
				current = start;
				for (i = 0; i < wall_length; i++){
					if (current == player_y){ // Player hit the wall
                        isover = 1;
                        state = 3;
						break;
					}
                    map[player_x][player_y] = PLAYER;
					map[row_index][current] = WALL;	

					current = (current == 47) ? 1 : current + 1; // Loop around at edges		
				}
			}
            start = (start == 1) ? 47 : start - 1;  // Loop around at edges
		}

		// row2, 6, 12: moves right
		else{	
			if (row_index != player_x){ // player is not on the same row as the wall
				current = start;
				for (i = 0; i < wall_length; i++){
					map[row_index][current] = WALL;
					current = (current == 1) ? 47 : current - 1;  // Loop around at edges
				}
			}
			else{ // player is on the same row as the wall, handle collisions
				current = start;
				for (i = 0; i < wall_length; i++){
					if (current == player_y){ // Player hit the wall
                        isover= 1;
                        state = 3;
						break;
					}
					map[player_x][player_y] = PLAYER;
					map[row_index][current] = WALL;	

					current = (current == 1) ? 47 : current - 1;  // Loop around at edges
				}
			}
			start = (start == 47) ? 1 : start + 1;  // Loop around at edges
		}

		pthread_mutex_unlock(&mutex);
		usleep(100000);
	}

	pthread_exit(NULL);
}


void *move_gold(void *t)
{	//t:（0~5）
	long row_index;
    long gold_index = (long)t;  // Convert void* to long
	int direction = gold_directions[gold_index]; 
   
    // Determine the row for the current gold based on its index
    if (gold_index <= 2){
        row_index = (long)(gold_index*2+1);
    }
    else{
        row_index = (long)(gold_index*2+5);
    }

	srand(time(0) + (unsigned long)pthread_self());
    pthread_mutex_lock(&mutex);
	gold_positions[row_index] = rand() % 47 + 1;
    pthread_mutex_unlock(&mutex);

	while(!isover){
        pthread_mutex_lock(&mutex);

        // Clear the current row (except the position where player is)
		for (int i=1; i < 48; i++){
            if (map[row_index][i]!=PLAYER){
               map[row_index][i] = ' ';
            }
		}

        if (gold_positions[row_index] != -1) { // gold is not yet consumed
            if (row_index == player_x && (gold_positions[row_index] == player_y)){  // Player collects the gold
                count ++;
                gold_positions[row_index] = -1;  // Gold is removed

                if (count == NUM_GOLDS) {  // Player collects all gold
                    state = 2;
                    isover = 1;
                }
            } 
            else {  // normal move
                map[row_index][gold_positions[row_index]] = GOLD;
            }
        }

		if (gold_positions[row_index] != -1) {
			if (direction == 0) {// coin moves left
				gold_positions[row_index] = (gold_positions[row_index] > 1) ? gold_positions[row_index] - 1 : 47;  
			} 
			else {// coin moves right
				gold_positions[row_index] = (gold_positions[row_index] < 47) ? gold_positions[row_index] + 1 : 1;  
			}
		}
        
        pthread_mutex_unlock(&mutex);
        usleep(100000);
	}
	pthread_exit(NULL);
}


void *move_player(void *arg){
	char input_;
	while(!isover){
		if(kbhit()){
			input_ = getchar();
			pthread_mutex_lock(&mutex);

			switch (input_) {
                case 'q':
                case 'Q':  // Quit
                    state = 1;
                    isover = 1;
                    break;

				case 'w':
                case 'W':  // Move up
                	if (player_x > 1) {
                        if (map[player_x - 1][player_y] == GOLD) { // Collect gold
                            count++;
                            player_x--;
                            map[player_x][player_y] = PLAYER;
                            map[player_x + 1][player_y] = ' ';
                            gold_positions[player_x] = -1;  // mark the gold as "consumed"

                            if (count == NUM_GOLDS) {  // Player collects all gold
                                state = 2;
                                isover = 1;
                            }
                        } 
						else if (map[player_x - 1][player_y] == WALL) { // Hit wall
                            isover = 1;
                            state = 3;
                        } 
						else {
                            player_x--;
                            map[player_x][player_y] = PLAYER;
                            map[player_x + 1][player_y] = ' ';
                        }
                    }
                    break;

				case 's':
                case 'S':  // Move down
                    if (player_x < 15) {
                        if (map[player_x + 1][player_y] == GOLD) { // Collect gold
                            count++;
                            player_x++;
                            map[player_x][player_y] = PLAYER;
                            map[player_x-1][player_y] = ' ';
                            gold_positions[player_x] = -1;  // mark the gold as "consumed"
                        
                            if (count == NUM_GOLDS) {  // Player collects all gold
                                state = 2;
                                isover = 1;
                            }
                        } 
						else if (map[player_x + 1][player_y] == WALL) { // Hit wall
                            isover = 1;
                            state = 3;
                        } 
						else {
                            player_x++;
							map[player_x][player_y] = PLAYER;
							map[player_x-1][player_y] = ' ';
                        }
                    }
                    break;

				case 'a':
                case 'A':  // Move left
                    if (player_y > 1) {
                        if (map[player_x][player_y - 1] == GOLD) { // Collect gold
                            count ++;
                            player_y--;
                            map[player_x][player_y] = PLAYER;
                            map[player_x][player_y+1] = ' ';
                            gold_positions[player_x] = -1;  // mark the gold as "consumed"
                            
                            if (count == NUM_GOLDS) {  // Player collects all gold
                                state = 2;
                                isover = 1;
                            }
                       } 
						else if (map[player_x][player_y - 1] == WALL) { // Hit wall
                            isover = 1;
                            state = 3;
                        } 
						else { //normal move
                            player_y--;
                            map[player_x][player_y] = PLAYER;
                            map[player_x][player_y+1] = ' ';
                        }
                    }
                    break;

				case 'd':
                case 'D':  // Move right
                    if (player_y < 47) {
                        if (map[player_x][player_y + 1] == GOLD) { // Collect gold
                            count ++;
                            player_y++;
                            map[player_x][player_y] = PLAYER;
                            map[player_x][player_y-1] = ' ';
                            gold_positions[player_x] = -1;  // mark the gold as "consumed"
                        
                            if (count == NUM_GOLDS) {  // Player collects all gold
                                state = 2;
                                isover = 1;
                            }
                        } 
						else if (map[player_x][player_y + 1] == WALL) { // Hit wall
                            isover = 1;
                            state = 3;
                        } 
						else { //normal move
                            player_y++;
                            map[player_x][player_y] = PLAYER;
                            map[player_x][player_y-1] = ' ';
                            
                        }
                    }
                    break;
				
				default:
                    break;  // Ignore other inputs
			}
			pthread_mutex_unlock(&mutex);
		}
	}
	pthread_exit(NULL);
} 


void *screen_print(void *arg){
	while (!isover){
		pthread_mutex_lock(&mutex);
		printf("\033[H\033[2J");  
		for(int i = 0;i <= ROW - 1; i ++){
			puts(map[i]);   
		}
		pthread_mutex_unlock(&mutex);
		usleep(100000);
	}
	pthread_exit(NULL);
}


/* print the map */
void map_print(void)
{
    printf("\033[H\033[2J"); 
    for (int i = 0; i <= ROW - 1; i++)
        puts(map[i]);   
}


/* main function */
int main(int argc, char *argv[])
{
    pthread_t gold_thread[NUM_GOLDS];
    pthread_t wall_thread[NUM_WALLS];
    pthread_t screen_thread;
	pthread_t player_thread;
    int rc;
	long t1, t2;

	pthread_mutex_init(&mutex, NULL);
    srand(time(NULL));

	// Initialize each gold's direction: 0 for left, 1 for right
    for (int i = 0; i < NUM_GOLDS; i++) {
        gold_directions[i] = rand() % 2; 
    }

    int i, j;
    /* Initialize the map */
    memset(map, 0, sizeof(map));
    for (i = 1; i <= ROW - 2; i++)
    {
        for (j = 1; j <= COLUMN - 2; j++)
        {
            map[i][j] = ' ';
        }
    }
    for (j = 1; j <= COLUMN - 2; j++)
    {
        map[0][j] = HORI_LINE;
        map[ROW - 1][j] = HORI_LINE;
    }
    for (i = 1; i <= ROW - 2; i++)
    {
        map[i][0] = VERT_LINE;
        map[i][COLUMN - 1] = VERT_LINE;
    }
    map[0][0] = CORNER;
    map[0][COLUMN - 1] = CORNER;
    map[ROW - 1][0] = CORNER;
    map[ROW - 1][COLUMN - 1] = CORNER;

    player_x = ROW / 2;
    player_y = COLUMN / 2;

    map[player_x][player_y] = PLAYER;

    map_print();

    // Create threads
    for(t1 = 0; t1 < NUM_GOLDS; t1++){
		rc = pthread_create(&gold_thread[t1], NULL, move_gold, (void*)t1);
		if(rc){
			printf("ERROR: return code from pthread_create() for move_gold() is %d\n", rc);
            exit(-1);
		}
	}

    for(t2 = 0; t2 < NUM_WALLS; t2++){
		rc = pthread_create(&wall_thread[t2], NULL, move_wall, (void*)t2);
		if(rc){
			printf("ERROR: return code from pthread_create() for move_wall() is %d\n", rc);
            exit(-1);
		}
	}

    rc = pthread_create(&screen_thread, NULL, screen_print, NULL);
	if(rc){
		printf("ERROR: return code from pthread_create() for screen_print is %d\n", rc);
        exit(-1);
	}

	rc = pthread_create(&player_thread, NULL, move_player, NULL);
	if(rc){
		printf("ERROR in creating thread for move_player: return error number is %d", rc);
	}

    // Join threads
    for(t1 = 0; t1 < NUM_GOLDS; t1++){
		pthread_join(gold_thread[t1], NULL);
	}
	for(t2 = 0; t2 < NUM_WALLS; t2++){
		pthread_join(wall_thread[t2], NULL);
	}
	pthread_join(screen_thread, NULL);
	pthread_join(player_thread, NULL);

	printf("\033[H\033[2J");
	if (state == 1){		
		printf("You exit the game.\n");
	}
	else if (state == 2){		
		printf("You win the game!!\n");
	}
	else if (state == 3){		
		printf("You lose the game!!\n");
	}

    return 0;
}
