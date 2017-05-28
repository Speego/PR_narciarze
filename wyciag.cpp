#include "mpi.h"
#include "constants.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct QUEUE_DATA
{
    int id;
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

  printf("GIVEN DATA:\nid: %d, T: %d, T_in: %d, m: %d\n", *(*th_data).id, *(*th_data).T, *(*th_data).T_in, *(*th_data).m);

//   char* buffer;
//   int receiverID;
//   GameManager* gameManager = (*th_data).gameManager;
//   vector<int>* clientsDescriptors = (*th_data).clientsDescriptors;
//   int messageLength;
//
//   while (1) {
//     pthread_mutex_lock(&(*th_data).messagesMutex);
//     // mutex pobierający z kolejki oczekujących
//     gameManager->update();
//     receiverID = gameManager->getReceiverID();
//     buffer = gameManager->getMessage();
//     // ten sam mutex (w ogóle jeden i ten sam)
//     pthread_mutex_unlock(&(*th_data).messagesMutex);
//
//     if (buffer != NULL) {
//       printf("server.cpp: Message sent to player with ID %d: %s", receiverID, buffer);
//       messageLength = write((*clientsDescriptors)[receiverID], buffer, strlen(buffer));
//       if (messageLength < 0)
//         printf("server.cpp: Error while writing to socket. Client id: %d, message: %s\n", receiverID, buffer);
//     }
//     free(buffer);
//   }
//
  printf("Receiving thread terminated.\n");
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
  printf("Receiving thread created.\n");
}

int main(int argc, char** argv)
{
	int id; // unikalne id (rank) narciarza
	int T; // aktualny zegar skalarny Lamporta
	int T_in; // etykieta czasowa, w ktorej proces ostatnio chcial wejsc na wyciag
	int m; // masa narciarza
	int n; // jak duzo narciarzy udalo sie uruchomic

	MPI_Status status;

  srand(time(NULL));

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	MPI_Comm_size(MPI_COMM_WORLD, &n);

  QUEUE_DATA q[n]; // kolejka informujaca o zadaniach innych narciarzy
  bool is_received[n]; // informacja, czy odpowiadajaca dana z q zostala odebrana
  pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;

  m = rand() % (M_MAX + 1 - M_MIN) + M_MIN;

  createRecevingThread(dataMutex, &id, &T, &T_in, &m);
	// int receiver = (rank + 1) % n;
	// int sender = (rank + n - 1) % n;

	// if (rank == root)
	// {
	// 	MPI_Comm_size( MPI_COMM_WORLD, &size );
	// 	printf("%d: Wysylam %d do %d\n", rank, msg, receiver);
	// 	MPI_Send(&msg, MSG_SIZE, MPI_INT, receiver, MSG_HELLO, MPI_COMM_WORLD );
	// }
	// while (msg < max - n) {
	// 	MPI_Recv(&msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD, &status);
	// 	printf("%d: Otrzymalem %d, ", rank, msg);
	// 	msg += 1;
	// 	MPI_Comm_size( MPI_COMM_WORLD, &size );
	// 	printf("%d: Wysylam %d do %d\n", rank, msg, receiver);
	// 	MPI_Send(&msg, MSG_SIZE, MPI_INT, receiver, MSG_HELLO, MPI_COMM_WORLD );
	// }

	MPI_Finalize();
}
