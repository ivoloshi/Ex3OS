#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/sem.h>

#define WAITING "waiting for client request\n"
#define FIFONAME "fifo_clientTOserver"
#define KEYNAME "Ex31.c"
#define SEMKEY "Ex32.c"
#define MEMSIZE 1024

int rowNum;
int columnNum;
int semid;
struct sembuf sb;
int memKey;


union semun {
	int val;
	struct semid_ds *buf;
	ushort *array;
};

int readInput() { 
	/*Reads a line from the user*/
	int curNumber = 0;
	int ch;
	do {
		ch = getc(stdin);
	} while (ch == '\n');

	for (; ch != EOF && ch != '\n'; ch = getc(stdin)) {
		curNumber *= 10;
		curNumber += ch - '0';
	}
	return curNumber;
}
void removeFromBoard(int row, int column, char* board, int * numOfChars) {
	/*Receives: current board, current number of left chars, row and column.
	The function removes all 'X' which are placed above and to the right of the given
	location (row and column) and updates the board and number of chars accordingly.*/
	int i, j;
	for (i = 0; i <= columnNum * row; i += columnNum) {
		for (j = i; j <= (i + column); ++j) {
			if (board[j] != '\0') {
				board[j] = '\0';
				(*numOfChars)--;
			}
		}
	}
}
void updateBoard(char * board, char * turnMade, int * numOfChars) {
	/*Receives: current board, the new turn which was made by the player, current number of left chars.
	The function splits the given turn string into two chars (row and column) and uses them to update the server's board*/
	int row, column, i, switchToColumns;
	i = 1;
	row = 0;
	column = 0;
	switchToColumns = 0;
	while (turnMade[i] != '\0') {
		if (turnMade[i] == ',') {
			i++;
			switchToColumns = 1;
		}
		if (switchToColumns == 0) {
			row *= 10;
			row += turnMade[i] - '0';
		} else {
			column *= 10;
			column += turnMade[i] - '0';
		}
		i++;
	}
	removeFromBoard(row, column, board, numOfChars);
}
int checkFinish(int* numOfChars) {
	/*Check the amount of chars which are non null. If there are none- the game is over*/
	if ((*numOfChars) == 0) {
		return 1;
	}
	return 0;
}
void initSemaphore() {
	/*Initializes the semaphore*/
	key_t semKey = ftok(SEMKEY, 'r');
	semid = semget(semKey, 1, IPC_CREAT | 0666);
	union semun arg;
	arg.val = 1;
	semctl(semid, 0, SETVAL, arg);
	/*semaphore now set*/
}
void finish(char * board, int * numOfChars) {
	/*This function is called when game ending conditions are met.
	The function prints the string GAME OVER, frees relevant memory ,waits 2 seconds (to ensure that the second client
	has read the turn), deletes the semaphore and shared memory and exists */
	write(1, "GAME OVER\n", strlen("GAME OVER\n"));
	free(board);
	free(numOfChars);
	sleep(2);
	shmctl(memKey, IPC_RMID, NULL);
	sb.sem_op = 1;
	union semun arg;
	arg.val = 0;
	semctl(semid, 0, IPC_RMID, arg);
	exit(0);
}
void lock() {
	/*Locks the semaphore*/
	sb.sem_op = -1;
	semop(semid, &sb, 1);
}
void unlock() {
	/*Unlocks the semaphore*/
	sb.sem_op = 1;
	semop(semid, &sb, 1);
}
int main(void) {
	int fd;
	sb.sem_num = 0;
	sb.sem_op = -1;
	sb.sem_flg = 0;

	key_t currentKey = ftok(KEYNAME, 'j');
	memKey = shmget(currentKey, MEMSIZE, IPC_CREAT | 0666); /*create shared memory*/
	char* memVar = (char*) shmat(memKey, NULL, 0);
	initSemaphore(); /*intitialize memory*/
	lock();
	mkfifo(FIFONAME, 0777);
	printf("opened fifo\n");
	write(1, WAITING, strlen(WAITING));
	fd = open(FIFONAME, O_RDONLY);
	pid_t fClient, sClient;
	read(fd, &fClient, sizeof(pid_t));
	write(1, WAITING, strlen(WAITING));
	while (read(fd, &sClient, sizeof(pid_t)) <= 0) { /*read until new information is present (the second client's pid)*/
		sleep(0); 
	}
	close(fd);
	unlink(FIFONAME); /*delete the fifo*/

	write(1, "Please choose number of rows and columns\n",
			strlen("Please choose number of rows and columns\n"));
	scanf("%d%d", &rowNum, &columnNum);
	int * numOfChars = malloc(sizeof(int));
	*numOfChars = rowNum * columnNum;
	char* board = malloc(sizeof(char) * rowNum * columnNum);
	int i;
	for (i = 0; i < rowNum * columnNum; ++i) {
		board[i] = 'X';
	}
	char tmpForBonus[1024] = { '\0' };
	sprintf(memVar, "%d,%d", rowNum, columnNum);
	sprintf(tmpForBonus, "%d,%d", rowNum, columnNum);
	unlock(); 
	/*Logically, unlocking and locking will place the next lock at the end of the semaphore queue which 
	ensures both clients have read the number of rows and columns*/
	lock();
	sprintf(memVar, "p%d", fClient);
	unlock();
	int lastTurn = 0;
	char tmp[1024] = { '\0' };
	while (1) {
		lock();
		strcpy(tmp, memVar);
		unlock();
		if (tmp[0] != 'p') {
			updateBoard(board, tmp, numOfChars);
			if (checkFinish(numOfChars) == 1) {
				finish(board, numOfChars);
			}
			break;
		}
	}
	lastTurn = 1;
	while (1) {
		lock();
		strcpy(tmp, memVar);
		unlock();
		if ((tmp[0] - '0') != lastTurn) {
			updateBoard(board, tmp, numOfChars);
			if (checkFinish(numOfChars) == 1) {
				finish(board, numOfChars);
			}
		}
		sleep(1);
	}
	return 0;
}
