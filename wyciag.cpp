#include "constants.h"

#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <climits>

struct QUEUE_DATA {
		int T_in;
		int m;
};

struct thread_receiving_data {
  pthread_mutex_t dataMutex;
  int* id;
  int* T;
  int* T_in;
  int* m;
  int* qCounter;
};

int maximum(int a, int b) {
  return (a > b ? a : b);
}

void* threadReceivingBehaviour(void* t_data) {
  struct thread_receiving_data *th_data = (struct thread_receiving_data*)t_data;
  int mpi_result;
  // printf("GIVEN DATA:\nid: %d, T: %d, T_in: %d, m: %d\n", *(*th_data).id, *(*th_data).T, *(*th_data).T_in, *(*th_data).m);

  int* msg_req = (int*)(malloc((MSG_REQUEST_SIZE + 1) * sizeof(int)));
  int* msg_acc = (int*)(malloc((MSG_ACCEPTANCE_SIZE + 1) * sizeof(int)));
  MPI_Status status;

  while (1) {
    mpi_result = MPI_Recv(msg_req, MSG_REQUEST_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_REQUEST, MPI_COMM_WORLD, &status);
    //printf("[REQ-RECV] %d: Od narciarza %d czas %d\n", *(*th_data).id, msg_req[MSG_ID], msg_req[MSG_T]);

    pthread_mutex_lock(&(*th_data).dataMutex);
    *(*th_data).T = maximum(*(*th_data).T, msg_req[MSG_T]) + 1;
    msg_acc[MSG_ID] = *(*th_data).id;
    msg_acc[MSG_M] = *(*th_data).m;
    if (*(*th_data).qCounter == 0) {
      msg_acc[MSG_T] = *(*th_data).T;
    } else {
      msg_acc[MSG_T] = *(*th_data).T_in;
    }
    //printf("[ACC-SEND] %d: Do narciarza %d czas %d i masa %d\n", *(*th_data).id, msg_req[MSG_ID], msg_acc[MSG_T], msg_acc[MSG_M]);
    mpi_result = MPI_Send(msg_acc, MSG_ACCEPTANCE_SIZE, MPI_INT, msg_req[MSG_ID], MSG_ACCEPTANCE, MPI_COMM_WORLD);

    pthread_mutex_unlock(&(*th_data).dataMutex);
  }
  // printf("Receiving thread terminated.\n");
  pthread_exit(NULL);
}

void createRecevingThread(pthread_mutex_t dataMutex, int* id, int* T, int* T_in, int* m, int* qCounter) {
  int createResult;
  pthread_t thread;
  struct thread_receiving_data* threadData;

  threadData = (struct thread_receiving_data*)malloc(sizeof(struct thread_receiving_data));
  (*threadData).dataMutex = dataMutex;
  (*threadData).id = id;
  (*threadData).T = T;
  (*threadData).T_in = T_in;
  (*threadData).m = m;
  (*threadData).qCounter = qCounter;

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

void sendRequests(int id, int T, int m, int n, int* msg_req) {
  int mpi_result;

  msg_req[MSG_ID] = id;
  msg_req[MSG_T] = T;

  for (int i = 0; i < n; i++) {
    if (i != id) {
      //printf("[REQ-SEND] %d: Do narciarza %d czas %d\n", id, i, T);
      mpi_result = MPI_Send(msg_req, MSG_REQUEST_SIZE, MPI_INT, i, MSG_REQUEST, MPI_COMM_WORLD);
    }
  }
}

void sendReleases(int id, int n) {
  int mpi_result;
  int* msg_acc = (int*)(malloc((MSG_ACCEPTANCE_SIZE+1) * sizeof(int)));
  msg_acc[MSG_ID] = id;
  msg_acc[MSG_M] = 0;
  msg_acc[MSG_T] = INT_MAX;

  for (int i = 0; i < n; i++) {
    if (i != id) {
      //printf("[REL-SEND] %d: Do narciarza %d czas MAX\n", id, i);
      mpi_result = MPI_Send(msg_acc, MSG_ACCEPTANCE_SIZE, MPI_INT, i, MSG_ACCEPTANCE, MPI_COMM_WORLD);
    }
  }
}

int weightsSum(int n, int id, QUEUE_DATA* q){
  int myTime = q[id].T_in;
  int sum = 0;
  for (int i = 0; i < n; i++){
    if ((q[i].T_in < myTime) || ((q[i].T_in == myTime) &&  (i <= id))){
      sum += q[i].m;
    }
  }
  //printf("%d SUM: %d\n", id, sum);
  return(sum);
}

int countAcc(bool* rec, int n){
  int sum = 0;
  for (int i = 0; i < n; i++)
  {
    rec[i] ? sum++ : true ;
  }
  //printf("COUNTER: %d\n", sum);
  return(sum);
}


void receiveAcceptances(QUEUE_DATA* q, bool* received, int* qCounter, int n, int id, pthread_mutex_t* mutex, int* msgAcc, MPI_Status* status) {
  int id_j;
  int mpi_result;

  while ( ((*qCounter = countAcc(received, n)) < n) || (weightsSum(n, id, q) > N)) {
    mpi_result = MPI_Recv(msgAcc, MSG_ACCEPTANCE_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_ACCEPTANCE, MPI_COMM_WORLD, status);
    //printf("[ACC-RECV] %d: Od narciarza %d czas %d i masa %d\n", id, msgAcc[MSG_ID], msgAcc[MSG_T], msgAcc[MSG_M]);
    id_j = msgAcc[MSG_ID];
    received[id_j] = true;
    q[id_j].T_in = msgAcc[MSG_T];
    q[id_j].m = msgAcc[MSG_M];
    pthread_mutex_lock(mutex);
    pthread_mutex_unlock(mutex);
  }
  //printf("%d: ALL ACCEPTANCES RECEIVED\n", id);
}

int main(int argc, char** argv)
{
  int id; // unikalne id (rank) narciarza
  int T; // aktualny zegar skalarny Lamporta
  int T_in; // etykieta czasowa, w ktorej proces ostatnio chcial wejsc na wyciag
  int m; // masa narciarza
  int n; // jak duzo narciarzy udalo sie uruchomic
  int qCounter = 0; // ile acceptance dostalismy
  int threadSupportLevel; // zapewniony poziom wsparcia watkow

  T = 0;
  T_in = 0;

  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &threadSupportLevel);
  MPI_Comm_rank(MPI_COMM_WORLD, &id);
  MPI_Comm_size(MPI_COMM_WORLD, &n);

  MPI_Status status;
  pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;
  QUEUE_DATA* q = (QUEUE_DATA*)(malloc(2 * (n+1) * sizeof(int))); // kolejka informujaca o zadaniach innych narciarzy
  bool* is_received = (bool*)malloc((n+1) * sizeof(bool)); // informacja, czy odpowiadajaca dana z q zostala odebrana
  int* msg_req = (int*)(malloc((MSG_REQUEST_SIZE+1) * sizeof(int)));
  int* msg_acc = (int*)(malloc((MSG_ACCEPTANCE_SIZE+1) * sizeof(int)));

  srand(time(NULL) + id);
  m = rand() % (M_MAX + 1 - M_MIN) + M_MIN;
  printf("%d: moja masa to %d\n", id, m);

  createRecevingThread(dataMutex, &id, &T, &T_in, &m, &qCounter);
  qCounter = 0;

  while (1) {
    wait();
      // chce wsiasc
      clearReceivedTable(is_received, n);
      pthread_mutex_lock(&dataMutex);
      T_in = T;
      pushToQueue(q, is_received, id, T_in, m);
      pthread_mutex_unlock(&dataMutex);
      sendRequests(id, T_in, m, n, msg_req);
      receiveAcceptances(q, is_received, &qCounter, n, id, &dataMutex, msg_acc, &status);
      // wsiada
      printf("---> %d: WYCIAG +%d\n" , id, m);
      wait();
      // wysiada
      printf("<--- %d: WYCIAG -%d\n" , id, m);
      qCounter = 0;
      //printf("[[%d]]: COUNTER 0\n" , id);
      sendReleases(id, n);
      wait();
  }

	MPI_Finalize();
}
