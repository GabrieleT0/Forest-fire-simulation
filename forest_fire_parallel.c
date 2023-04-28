#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mpi.h"
#define M 9
#define N 4
#define S 10 //step dell'algoritmo
#define TREE "🌲"
#define EMPTY "❌"
#define BURN "🔥"


//stampa della matrice di char
void printMatrix(char *matrix,int num_row,int num_col);

//inizializzo la foresta
void forest_initialization(char *forest,int num_row,int num_col);

//controlla se la cella deve bruciare
void check_neighbors(char *forest,char *matrix2,int num_row,int num_col,int i,int j,int prob_burn);

//controlla se la cella deve bruciare in base alle righe ricevute dagli altri processi
void check_borders(char *forest, char *matrix2, char *top_row, char *bottom_row, int i,int j, int num_row, int num_col,int prob_burn);

int main(int argc, char *argv[]){
    int myrank, numtasks;
    MPI_Status status;
    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
    int strip_size,remainder,empty_counter;
    int *displ,*send_counts;
    char *forest;
    int recv_count = M;
    int sum = 0;
    int prob_burn = 50;  // probabilità che un albero si incendi 0 <= prob_burn <= 100
    int prob_grow = 50;  //probabilità che un albero cresca nella cella vuota, 0 <= prob_tree <= 100    

    //Controllo se c'è resto
    remainder = M % numtasks;

    send_counts = malloc(numtasks * sizeof(int));
    displ = malloc(numtasks * sizeof(int));

    for(int i = 0; i<numtasks; i++){
        send_counts[i] = M / numtasks; //numero di righe che dovrà avere ogni processo
        if(remainder > 0){
            send_counts[i]++;  //distribuisco in maniera equa le righe se c'è resto
            remainder--;
        }
        send_counts[i] *= N; //ottenuto il numero di righe, moltiplico per il numero di colonne, ottentedo la quantità di elementi che in totale avrà ogni processo
        displ[i] = sum; //spiazzamento per scatterv
        sum += send_counts[i];
    }
       
    printf("Sono il processo %d, questo è il numero di elementi che riceverò: %d\n", myrank, send_counts[myrank]);

    if(myrank == 0){
        /*
        Allocazione continua di memoria. In questo modo, la matrice è allocata come un singolo blocco di memoria, dove ogni riga è collocata consecutivamente.
        */
        forest = (char*) malloc(M * N * sizeof(char));
        //forest = malloc(sizeof(char[M][N]));
        printf("Foresta iniziale:\n");
        forest_initialization(forest,M,N);
        printMatrix(forest,M,N);
    }

    //Dichiaro e inizializzo le sottomatrici dei processi
    char *sub_forest = malloc(send_counts[myrank] * sizeof(char));
    char *sub_matrix = malloc(send_counts[myrank] * sizeof(char));
    char *tmp = malloc(send_counts[myrank] * sizeof(char));
    char *top_row = malloc(N * sizeof(char));
    char *bottom_row = malloc(N * sizeof(char));


    MPI_Scatterv(forest,send_counts,displ,MPI_CHAR,sub_forest,send_counts[myrank],MPI_CHAR,0,MPI_COMM_WORLD);
    
    printf("Sono il processo %d, questa è la porzione di foresta che ho ricevuto:\n",myrank);
    printMatrix(sub_forest,send_counts[myrank]/N,N);

    int my_row_num = send_counts[myrank] / N;

    if(myrank == 0){ 
        //invio l'ultima riga al processo successivo
        MPI_Send(&sub_forest[send_counts[myrank] - N],N,MPI_CHAR,myrank + 1,0,MPI_COMM_WORLD);
        //ricevo la prima riga dal processo successivo
        MPI_Recv(bottom_row,N,MPI_CHAR,myrank + 1,0,MPI_COMM_WORLD,&status);
    } else if(myrank == numtasks - 1){
        //invio la prima riga al processo precedente
        MPI_Send(sub_forest,N,MPI_CHAR,myrank - 1,0,MPI_COMM_WORLD);
        //ricevo ultima riga dal processo precedente
        MPI_Recv(top_row,N,MPI_CHAR,myrank - 1,0,MPI_COMM_WORLD,&status);
    } else { // tutti gli altri processi
        //invio la prima riga al processo precedente
        MPI_Send(sub_forest,N,MPI_CHAR,myrank -1,0,MPI_COMM_WORLD);
        //invio l'ultima riga al successivo 
        MPI_Send(&sub_forest[send_counts[myrank] - N],N,MPI_CHAR,myrank + 1,0,MPI_COMM_WORLD);

        //ricevo la riga superiore dal processo precedente
        MPI_Recv(top_row,N,MPI_CHAR,myrank - 1,0,MPI_COMM_WORLD,&status);
        //ricevo la riga inferiore dal processo successivo
        MPI_Recv(bottom_row,N,MPI_CHAR,myrank + 1,0,MPI_COMM_WORLD,&status);
    }

    printf("-----------------Sono il processo %d ed ho riucevuto questa top row:---------------------\n",myrank);
    printMatrix(top_row,1,N);
    printf("-----------------Sono il processo %d ed ho ricevuto questa bottom row:-------------------\n",myrank);
    printMatrix(bottom_row,1,N);

    if(my_row_num >= 3){ // se ho più di 3 righe possso iniziare già a computare le righe non ai bordi
        for(int i = 1; i<my_row_num - 1; i++){
            for(int j = 0; j<N; j++){
                if(sub_forest[i * N + j] == 'B')
                    sub_matrix[i * N + j] = 'E'; //Se è già Burned allora ora la cella diventa vuota
                else if(sub_forest[i * N + j] == 'E'){
                    int rand_num = 1 + (rand() % 100); // se è vuota, allora con probabilità prob_grown può crescere un albero nella cella
                    if(rand_num <= prob_grow){
                        sub_matrix[i * N + j] = 'T';
                    } else
                        sub_matrix[i * N + j] = 'E';
                }else if(sub_forest[i * N + j] == 'T'){
                    check_neighbors(sub_forest,sub_matrix,my_row_num,N,i,j,prob_burn);
                }
            }
        }
    }
    //controllo le righe ai bordi
    for(int i = 0;;i = my_row_num - 1){
        for(int j = 0; j < N; j++){
            if(sub_forest[i * N + j] == 'B')
                sub_matrix[i * N + j] = 'E';
            else if(sub_forest[i * N + j] == 'E'){
                int rand_num = 1 + (rand() % 100); // se è vuota, allora con probabilità prob_grown può crescere un albero nella cella
                if(rand_num <= prob_grow){
                    sub_matrix[i * N + j] = 'T';
                } else 
                    sub_matrix[i * N + j] = 'E';
            }else if(sub_forest[i * N + j] == 'T'){
                check_borders(sub_forest,sub_matrix,top_row,bottom_row,i,j,my_row_num,N,prob_burn);
            }
        }
        if(i == my_row_num -1)
            break;
    }

    fflush(stdout);

    printf("Sono il processo %d ecco la mia foresta dopo il controllo:\n",myrank);
    printMatrix(sub_matrix,my_row_num,N);

    MPI_Finalize();
    return 0;
}

void forest_initialization(char *forest,int num_row,int num_col){
    for(int i = 0; i<num_row; i++){
        for(int j = 0; j<num_col; j++){
            int rand_num = 1 + (rand() % 100);
            if(rand_num > 70)
                forest[i* N + j] = 'T';
            else if(rand_num <= 50)
                forest[i * N + j] = 'E';
            else
                forest[i * N + j] = 'B';
        }
    }
}

void printMatrix(char *matrix,int num_row,int num_col){
    for(int i = 0; i<num_row; i++){
        for(int j = 0; j<num_col; j++){
            printf("%c ",matrix[i* num_col + j]);
        }
        printf("\n");
    }
}

void check_neighbors(char *forest,char *matrix2,int num_row,int num_col,int i,int j,int prob_burn){
    //controllo a destra e a sinistra

    //se controllando l'elemento a destra sono ancora della riga (quindi il resto della divisione deve essere 0 altrimenti vuol dire che sono all'ultimo elemento)
    if(((i * num_col)  + j + 1) % num_col != 0 && forest[((i * num_col) + j + 1)] == 'B')
        matrix2[i * num_col + j] = 'B';
    //se controllando l'elemento a sinistra sono ancora della riga (quindi il resto della divisione deve essere diverso na n-1 altrimenti vuol dire che sono al primo elemento della riga)
    if(((i * num_col)  + j - 1) % num_col != num_col-1 && forest[((i * num_col) + j - 1)] == 'B')
        matrix2[i * num_col + j] = 'B';

    //controllo sopra e sotto
    
    //verifico se il vicino della riga inferiore sta bruciando
    if(((i * num_col) + j + num_col) < num_row * num_col && forest[((i * num_col) + j + num_col)] == 'B')
        matrix2[i * num_col + j] = 'B';

    //verifico se il vicino della riga superiore sta bruciando
    if(((i * num_col) + j - num_col) >=0 && forest[((i * num_col) + j - num_col)] == 'B')
        matrix2[i * num_col + j] = 'B';   

    //se nessun vicino è burned allora con probabilità prob_burn può diventare burned
    int rand_num = 1 + (rand() % 100);
    if(matrix2[i * num_col + j] == 'B'){ //verifico anche se negli if precedenti non l'ho già bruciato, altrimenti poi se non viene soddisfatto il secondo valore dell'OR inserirei un albero in una cella dove uno stava bruciando
        matrix2[i * num_col + j] = 'B';
    } else{
        matrix2[i * num_col + j] = 'T';
    }
}

void check_borders(char *forest, char *matrix2,char *top_row,char *bottom_row,int i,int j, int num_row, int num_col,int prob_burn){
    //controllo a destra e a sinistra
    if(((i * num_col)  + j + 1) % num_col != 0 && forest[((i * num_col) + j + 1)] == 'B')
        matrix2[i * num_col + j] = 'B';
    if(((i * num_col)  + j - 1) % num_col != num_col-1 && forest[((i * num_col) + j - 1)] == 'B')
        matrix2[i * num_col + j] = 'B';
        
    //se sto considerando la prima riga, allora faccio il controllo con top_row
    if(i == 0){
        if(((i * num_col) + j - num_col) >=0 && top_row[((i * num_col) + j - num_col)] == 'B')
            matrix2[i * num_col + j] = 'B'; 
        if(((i * num_col) + j + num_col) < num_row * num_col && forest[((i * num_col) + j + num_col)] == 'B')
            matrix2[i * num_col + j] = 'B';
        
    //se sono all'ultima allora controllo con bottom_row
    } else if(i == num_row - 1){
        if(((i * num_col) + j + num_col) < num_row * num_col && bottom_row[((i * num_col) + j + num_col)] == 'B')
            matrix2[i * num_col + j] = 'B';   
        //verifico se il vicino della riga superiore sta bruciando
        if(((i * num_col) + j - num_col) >=0 && forest[((i * num_col) + j - num_col)] == 'B')
            matrix2[i * num_col + j] = 'B';  
    }

    //se nessun vicino è burned allora con probabilità prob_burn può diventare burned
    int rand_num = 1 + (rand() % 100);
    if(matrix2[i * num_col + j] == 'B'){ //verifico anche se negli if precedenti non l'ho già bruciato, altrimenti poi se non viene soddisfatto il secondo valore dell'OR inserirei un albero in una cella dove uno stava bruciando
        matrix2[i * num_col + j] = 'B';
    } else{
        matrix2[i * num_col + j] = 'T';
    }
}