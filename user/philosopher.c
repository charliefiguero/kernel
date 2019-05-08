#include "philosopher.h"

#define number_of_philosophers 16


// int hands[number_of_philosophers];
int hands[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int mutex = 1;

// void init_hands(int* array){
//   for (int i = 0; i < 5; i++){
//     hands[i] = 1;
//   }
// }

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void request_hands(int id){
  sem_wait(&hands[id - 1]);
  sem_wait(&hands[id % 16]);
}
void reject_hands(int id ){
  sem_post(&hands[id - 1]);
  sem_post(&hands[id % 16]);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void main_dining_philosophers(){
  // init_hands(&hands[0]);
  for (int i = 1; i < 16 + 1; i++){ //Initialses 16 philosophers

      if ( fork() == 0 ){ // if child process, then is a diner

        int id = i;
        char c[2]; itoa(c, id); //converts integer id to printable string

        write( STDOUT_FILENO, "Philosopher ", 12);
        write( STDOUT_FILENO, c, 2);
        write( STDOUT_FILENO, " has joined the table.\n", 23);


        while(1){

          sem_wait(&mutex); //Waiting for lock and hands to be free
          if ( hands[id - 1] && hands[id % 16] ){
            request_hands(id);
            sem_post(&mutex);
          }
          else {
            sem_post(&mutex);
            write( STDOUT_FILENO, "Philosopher ", 12);
            write( STDOUT_FILENO, c, 2);
            write( STDOUT_FILENO, " could not eat.\n", 16);
            sleep();
            continue;
          }

          //Eating
          write( STDOUT_FILENO, "Philosopher ", 12);
          write( STDOUT_FILENO, c, 2);
          write( STDOUT_FILENO, " is eating.\n", 12);
          sleep();

          //Stops eating
          sem_wait(&mutex);
          // reject_hands(id);
          sem_post(&hands[id -1]);
          sem_post(&hands[id %16]);
          sem_post(&mutex);
          write( STDOUT_FILENO, "Philosopher ", 12);
          write( STDOUT_FILENO, c, 2);
          write( STDOUT_FILENO, " stopped eating.", 17);
          write( STDOUT_FILENO, " He's thinking!!!.\n", 19);

        }
      }
  }
  exit(EXIT_SUCCESS);
}
