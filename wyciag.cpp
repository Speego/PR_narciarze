#include "constants.h"

#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <climits>

// dane przechowywane w lokalnej kolejce procesu
struct QUEUE_DATA {
  int T_in; // czas zgloszenia requesta / wyslania zgody
  int m; // masa
};

// dane watku odpowiedzialnego za obsluge requestow
struct thread_receiving_data {
  pthread_mutex_t dataMutex; // mutex
  int* id; // id procesu
  int* T; // czas na podstawie zegara Lamporta
  int* T_in; // czas zgloszenia ostatnieego requesta
  int* m; // masa
  int* qCounter; // licznik odpowiedzi uzyskanych na ostatni request
};

int maximum(int a, int b) {
  return (a > b ? a : b);
}

// procedura watku obslugujacego przychodzace requesty
void* threadReceivingBehaviour(void* t_data) {
  struct thread_receiving_data *th_data = (struct thread_receiving_data*)t_data;
  int mpi_result;
  // printf("GIVEN DATA:\nid: %d, T: %d, T_in: %d, m: %d\n", *(*th_data).id, *(*th_data).T, *(*th_data).T_in, *(*th_data).m);

  // bufor odbieranego requesta
  int* msg_req = (int*)(malloc((MSG_REQUEST_SIZE + 1) * sizeof(int)));
  // bufor odsylanej odpowiedzi
  int* msg_acc = (int*)(malloc((MSG_ACCEPTANCE_SIZE + 1) * sizeof(int)));
  MPI_Status status;

  while (1) {
    // blokujacy odbior requesta od kogokolwiek
    mpi_result = MPI_Recv(msg_req, MSG_REQUEST_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_REQUEST, MPI_COMM_WORLD, &status);
    //printf("[REQ-RECV] %d: Od narciarza %d czas %d\n", *(*th_data).id, msg_req[MSG_ID], msg_req[MSG_T]);

    pthread_mutex_lock(&(*th_data).dataMutex);
      // inkrementacja zegara
      *(*th_data).T = maximum(*(*th_data).T, msg_req[MSG_T]) + 1;
      // wypelnienie bufora odpowiedzi
      msg_acc[MSG_ID] = *(*th_data).id;
      msg_acc[MSG_M] = *(*th_data).m;

      // jesli proces NIE stara sie o dostep do sekcji krytycznej
      if (*(*th_data).qCounter == 0) {
        // do bufora trafia czas aktualna wartosc zegara
        msg_acc[MSG_T] = *(*th_data).T;
      } else {
        // do bufora trafia czas requesta procesu wysylacego
        msg_acc[MSG_T] = *(*th_data).T_in;
      }
    pthread_mutex_unlock(&(*th_data).dataMutex);
    //printf("[ACC-SEND] %d: Do narciarza %d czas %d i masa %d\n", *(*th_data).id, msg_req[MSG_ID], msg_acc[MSG_T], msg_acc[MSG_M]);
    // odeslanie odpowiedzi
    mpi_result = MPI_Send(msg_acc, MSG_ACCEPTANCE_SIZE, MPI_INT, msg_req[MSG_ID], MSG_ACCEPTANCE, MPI_COMM_WORLD);
  }
  // printf("Receiving thread terminated.\n");
  pthread_exit(NULL);
}

// procedura tworzaca watek oblugujacy przychodzace requesty
// args: mutex, id procesu, lokalny zegar,
//       czas ostatniego requesta, masa, licznik odebranych zgod
void createRecevingThread(pthread_mutex_t dataMutex, int* id, int* T, int* T_in, int* m, int* qCounter) {
  int createResult; // wynik utworzenie watku
  pthread_t thread; // uchwyt watku
  struct thread_receiving_data* threadData;

  // alokacja pamieci i wpisanie danych watku
  threadData = (struct thread_receiving_data*)malloc(sizeof(struct thread_receiving_data));
  (*threadData).dataMutex = dataMutex;
  (*threadData).id = id;
  (*threadData).T = T;
  (*threadData).T_in = T_in;
  (*threadData).m = m;
  (*threadData).qCounter = qCounter;

  // utworzenie watku
  createResult = pthread_create(&thread, NULL, threadReceivingBehaviour, (void*)threadData);
  if (createResult) {
    printf("Error while creating receiving thread. Error code: %d\n", createResult);
		exit(-1);
  }
  // printf("Receiving thread created.\n");
}

// czekanie losowy czas
void wait() {
  float waitingTime;
  waitingTime = (rand() % MAX_WAITING_TIME_MS) * 1000;
  usleep(waitingTime);
}

// czyszczenie tablicy odebranych zgod
// args: tablica zgod, liczba elementow tablicy
void clearReceivedTable(bool* table, int n) {
  for (int i = 0; i < n; i++) {
    table[i] = false;
  }
}

// wstawianie otrzymanej zgody do tablic lokalnych
void pushToQueue(QUEUE_DATA* q, bool* received, int id, int T, int m) {
  received[id] = true;
  q[id].T_in = T;
  q[id].m = m;
}

// rozeslanie requesta do wszystkich procesow
void sendRequests(int id, int T, int m, int n, int* msg_req) {
  int mpi_result;
  // wypelnienie bufora wysylanej wiadomosci
  msg_req[MSG_ID] = id;
  msg_req[MSG_T] = T;

  // rozeslanie requesta
  for (int i = 0; i < n; i++) {
    if (i != id) {
      //printf("[REQ-SEND] %d: Do narciarza %d czas %d\n", id, i, T);
      mpi_result = MPI_Send(msg_req, MSG_REQUEST_SIZE, MPI_INT, i, MSG_REQUEST, MPI_COMM_WORLD);
    }
  }
}

// rozeslanie release'a po wyjsciu z sekcji krytycznej
void sendReleases(int id, int n) {
  MPI_Request req;
  int mpi_result;

  // alokacja pamieci i wypelnienie bufora wysylanej wiadomosci
  int* msg_acc = (int*)(malloc((MSG_ACCEPTANCE_SIZE+1) * sizeof(int)));
  msg_acc[MSG_ID] = id;
  msg_acc[MSG_M] = 0;
  msg_acc[MSG_T] = INT_MAX; // release jest wysylany jako acceptance,
                            // dlatego INT_MAX spowoduje najnizszy priorytet

  for (int i = 0; i < n; i++) {
    if (i != id) {
      //printf("[REL-SEND] %d: Do narciarza %d czas MAX\n", id, i);
      // nieblokujace rozeslanie wirtualnego release'a jako acceptance
      mpi_result = MPI_Isend(msg_acc, MSG_ACCEPTANCE_SIZE, MPI_INT, i, MSG_ACCEPTANCE, MPI_COMM_WORLD, &req);
      // mpi_result = MPI_Send(msg_acc, MSG_ACCEPTANCE_SIZE, MPI_INT, i, MSG_ACCEPTANCE, MPI_COMM_WORLD);
    }
  }
}

// zliczenie mas na wyciagu + masy procesu + mas bedacych blizej czola kolejki
int weightsSum(int n, int id, QUEUE_DATA* q){
  int myTime = q[id].T_in;
  int sum = 0;
  for (int i = 0; i < n; i++){
    // jesli lepszy czas lub taki sam czas, ale nizsze id
    if ((q[i].T_in < myTime) || ((q[i].T_in == myTime) &&  (i <= id))){
      // dodanie masy jako bedacej przed procesem
      sum += q[i].m;
    }
  }
  //printf("%d SUM: %d\n", id, sum);
  return(sum);
}

// zliczenie liczby uzyskanych odpowiedzi na request
int countAcc(bool* rec, int n){
  int sum = 0;
  for (int i = 0; i < n; i++)
  {
    rec[i] ? sum++ : true ;
  }
  //printf("COUNTER: %d\n", sum);
  return(sum);
}

// procedura odiboru zgod do momentu mozliwosci wejscia na wyciag
void receiveAcceptances(QUEUE_DATA* q, bool* received, int* qCounter, int n, int id, pthread_mutex_t* mutex, int* msgAcc, MPI_Status* status) {
  int id_j;
  int mpi_result;
  pthread_mutex_lock(mutex);
    // poczatkowe podliczenie uzyskanych zgod
    *qCounter = countAcc(received, n);
  pthread_mutex_unlock(mutex);

  // dopoki nie uzyskano zgod od wszystkich procesow lub nie ma miejsca na wyciagu
  while ((*qCounter < n) || (weightsSum(n, id, q) > N)) {
    // blokujacy odbior odpowiedzi od kogokolwiek
    mpi_result = MPI_Recv(msgAcc, MSG_ACCEPTANCE_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_ACCEPTANCE, MPI_COMM_WORLD, status);
    // printf("[ACC-RECV] %d: Od narciarza %d czas %d i masa %d\n", id, msgAcc[MSG_ID], msgAcc[MSG_T], msgAcc[MSG_M]);
    id_j = msgAcc[MSG_ID];
    // odpwiedz jest oznaczona jako uzyskana tylko wtedy,
    // jesli przeslany czas jest rozny od MAX;
    // w ten sposob "stare" komunikaty release nie zagrazaja bezpieczenstwu
    if ((msgAcc[MSG_T] != INT_MAX)){
      received[id_j] = true;
    }
    q[id_j].T_in = msgAcc[MSG_T];
    q[id_j].m = msgAcc[MSG_M];
    pthread_mutex_lock(mutex);
      *qCounter = countAcc(received, n);
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
 
  // utworzenie watku odpowiedzialnego za obsluge przychodzacych requestow
  createRecevingThread(dataMutex, &id, &T, &T_in, &m, &qCounter);
  qCounter = 0;

  wait();
  while (1) {
    // --chce wsiasc--

    // czyszczenie tablicy uzyskanych zgod
    clearReceivedTable(is_received, n);
    pthread_mutex_lock(&dataMutex);
      // rejestracja czasu requesta
      T_in = T;
      // dodanie wlasnej zgody do lokalnej kolejki
      pushToQueue(q, is_received, id, T_in, m);
    pthread_mutex_unlock(&dataMutex);
    // rozeslanie requestow
    sendRequests(id, T_in, m, n, msg_req);
    // odebranie odpowiedzi wystarczajacych do wejscia na wyciag
    receiveAcceptances(q, is_received, &qCounter, n, id, &dataMutex, msg_acc, &status);
    // --wsiada--
    printf("---> %d WYCIAG %d %d +%d\n", id, T_in, T, m);
    // --jest na wyciagu--
    wait();
    // --wysiada--
    printf("<--- %d WYCIAG %d %d -%d\n", id, T_in, T, m);
    qCounter = 0;
    //printf("[[%d]]: COUNTER 0\n" , id);
    // rozeslanie wirtualnych release'ow jako accpetance
    sendReleases(id, n);
    wait();
  }
  MPI_Finalize();
}
