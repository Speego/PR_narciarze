#include "constants.h"

#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct QUEUE_DATA
{
		int T_in;
		int m;
};

struct thread_receiving_data {
  pthread_mutex_t dataMutex;
  int* id;
  int* T;
  int* T_in;
  int* m;
};

void* threadReceivingBehaviour(void* t_data) {
  struct thread_receiving_data *th_data = (struct thread_receiving_data*)t_data;
  // printf("GIVEN DATA:\nid: %d, T: %d, T_in: %d, m: %d\n", *(*th_data).id, *(*th_data).T, *(*th_data).T_in, *(*th_data).m);

  int msg[MSG_REQUEST_SIZE];
  MPI_Status status;

  while (1) {
    MPI_Recv(msg, MSG_REQUEST_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_REQUEST, MPI_COMM_WORLD, &status);
    printf("%d: Od narciarza o id %d otrzymalem czas %d\n", *(*th_data).id, msg[MSG_ID], msg[MSG_T]);
  }
//     pthread_mutex_lock(&(*th_data).messagesMutex);
//     pthread_mutex_unlock(&(*th_data).messagesMutex);
//
  // printf("Receiving thread terminated.\n");
  pthread_exit(NULL);
}

void createRecevingThread(pthread_mutex_t dataMutex, int* id, int* T, int* T_in, int* m) {
  int createResult;
  pthread_t thread;
  struct thread_receiving_data* threadData;

  threadData = (struct thread_receiving_data*)malloc(sizeof(struct thread_receiving_data));
  (*threadData).dataMutex = dataMutex;
  (*threadData).id = id;
  (*threadData).T = T;
  (*threadData).T_in = T_in;
  (*threadData).m = m;

  createResult = pthread_create(&thread, NULL, threadReceivingBehaviour, (void*)threadData);
  if (createResult) {
    printf("Error while creating receiving thread. Error code: %d\n", createResult);
		exit(-1);
  }
  // printf("Receiving thread created.\n");
}

void wait() {
  float waitingTime;
  waitingTime = (rand() % MAX_WAITING_TIME_MS) * 1000;
  usleep(waitingTime);
}

void clearReceivedTable(bool* table, int n) {
  for (int i = 0; i < n; i++) {
    table[i] = false;
  }
}

void pushToQueue(QUEUE_DATA* q, bool* received, int id, int T, int m) {
  received[id] = true;
  q[id].T_in = T;
  q[id].m = m;
}

void sendRequests(int id, int T, int m, int n) {
  int msg[MSG_REQUEST_SIZE];

  msg[MSG_ID] = id;
  msg[MSG_T] = T;

  for (int i = 0; i < n; i++) {
    if (i != id) {
      MPI_Send(msg, MSG_REQUEST_SIZE, MPI_INT, i, MSG_REQUEST, MPI_COMM_WORLD);
    }
  }
}

int main(int argc, char** argv)
{
	int id; // unikalne id (rank) narciarza
	int T; // aktualny zegar skalarny Lamporta
	int T_in; // etykieta czasowa, w ktorej proces ostatnio chcial wejsc na wyciag
	int m; // masa narciarza
	int n; // jak duzo narciarzy udalo sie uruchomic

	MPI_Status status;

  T = 0;
  T_in = 0;

  srand(time(NULL));

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	MPI_Comm_size(MPI_COMM_WORLD, &n);

  QUEUE_DATA q[n]; // kolejka informujaca o zadaniach innych narciarzy
  bool is_received[n]; // informacja, czy odpowiadajaca dana z q zostala odebrana
  pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;

  m = rand() % (M_MAX + 1 - M_MIN) + M_MIN;

  createRecevingThread(dataMutex, &id, &T, &T_in, &m);

  while (1) {
    wait();

    // chce wsiasc
    clearReceivedTable(is_received, n);
    T_in = T;
    pushToQueue(q, is_received, id, T_in, m);
    if (id == 0)
    sendRequests(id, T_in, m, n);
    sleep(10);
  }

	MPI_Finalize();
}
