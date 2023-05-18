#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mpi.h"

//stampa della matrice di char
void printMatrix(char *matrix,int num_row,int num_col);

//stampa la foresta su file
void print_on_file(char *matrix, int num_row, int num_col, FILE *file);

//inizializzo la foresta
void forest_initialization(char *forest,int num_row,int num_col);

//controlla se la cella deve bruciare
void check_neighbors(char *forest,char *matrix2,int num_row,int num_col,int i,int j,int prob_burn);

//controlla se la cella deve bruciare in base alle righe ricevute dagli altri processi
void check_borders(char *forest, char *matrix2, char *top_row, char *bottom_row, int i,int j, int num_row, int num_col,int prob_burn);

int main(int argc, char *argv[]){
    FILE *fptr;
    fptr = fopen("forrest_fire_output","w");
    int myrank, numtasks,strip_size,remainder,empty_counter,all_empty,m,n,s;
    int *displ,*send_counts;
    char *forest;
    int sum = 0;
    int prob_burn = 50;  // probabilità che un albero si incendi 0 <= prob_burn <= 100
    int prob_grow = 50;  //probabilità che un albero cresca nella cella vuota, 0 <= prob_tree <= 100    
    int empty_count = 0;
    int empty_recv_count = 0;
    double start, end;
    
    if(argc != 3){
        m = atoi(argv[1]);
        n = atoi(argv[2]);
        s = atoi(argv[3]);
    } else {
        printf("Inserisci il numero di righe, il numero di colonne e il numero di step dell'algoritmo (3 argomenti richiesti [row,col,step])\n");
        return 1;
    }

    MPI_Request request[2];
    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&myrank);

    //Controllo se c'è resto
    remainder = m % numtasks;

    send_counts = malloc(numtasks * sizeof(int));
    displ = malloc(numtasks * sizeof(int));

    for(int i = 0; i<numtasks; i++){
        send_counts[i] = m / numtasks; //numero di righe che dovrà avere ogni processo
        if(remainder > 0){
            send_counts[i]++;  //distribuisco in maniera equa le righe se c'è resto
            remainder--;
        }
        send_counts[i] *= n; //ottenuto il numero di righe, moltiplico per il numero di colonne, ottentedo la quantità di elementi che in totale avrà ogni processo
        displ[i] = sum; //spiazzamento per scatterv
        sum += send_counts[i];
    }
       
    //printf("Sono il processo %d, questo è il numero di elementi che riceverò: %d\n", myrank, send_counts[myrank]);

    if(myrank == 0){
        /*
        Allocazione continua di memoria. In questo modo, la matrice è allocata come un singolo blocco di memoria, dove ogni riga è collocata consecutivamente.
        */
        forest = (char*) malloc(m * n * sizeof(char));
        printf("Foresta iniziale:\n");
        forest_initialization(forest,m,n);
        printMatrix(forest,m,n);
        print_on_file(forest,m,n,fptr);
    }

    //Dichiaro e inizializzo le sottomatrici dei processi
    char *sub_forest = malloc(send_counts[myrank] * sizeof(char));
    char *sub_matrix = malloc(send_counts[myrank] * sizeof(char));
    char *tmp = malloc(send_counts[myrank] * sizeof(char));
    char *top_row = malloc(n * sizeof(char));
    char *bottom_row = malloc(n * sizeof(char));

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    MPI_Scatterv(forest,send_counts,displ,MPI_CHAR,sub_forest,send_counts[myrank],MPI_CHAR,0,MPI_COMM_WORLD);
    
    /*
    printf("Sono il processo %d, questa è la porzione di foresta che ho ricevuto:\n",myrank);
    printMatrix(sub_forest,send_counts[myrank]/n,n);


    printf("Sono il processo %d, questa è la porzione di foresta che ho ricevuto:\n",myrank);
    printMatrix(sub_forest,send_counts[myrank]/n,n);
    */

    int my_row_num = send_counts[myrank] / n;

    for(int i = 0; i<s; i++){
        empty_count = 0;
        if(myrank == 0){ 
            //invio l'ultima riga al processo successivo
            MPI_Isend(&sub_forest[send_counts[myrank] - n],n,MPI_CHAR,myrank + 1,0,MPI_COMM_WORLD,&request[0]);
            //ricevo la prima riga dal processo successivo
            MPI_Irecv(bottom_row,n,MPI_CHAR,myrank + 1,0,MPI_COMM_WORLD,&request[1]);
        } else if(myrank == numtasks - 1){
            //invio la prima riga al processo precedente
            MPI_Isend(sub_forest,n,MPI_CHAR,myrank - 1,0,MPI_COMM_WORLD,&request[1]);
            //ricevo ultima riga dal processo precedente
            MPI_Irecv(top_row,n,MPI_CHAR,myrank - 1,0,MPI_COMM_WORLD,&request[0]);
        } else { // tutti gli altri processi
            //invio la prima riga al processo precedente
            MPI_Isend(sub_forest,n,MPI_CHAR,myrank -1,0,MPI_COMM_WORLD,&request[1]);
            //invio l'ultima riga al successivo 
            MPI_Isend(&sub_forest[send_counts[myrank] - n],n,MPI_CHAR,myrank + 1,0,MPI_COMM_WORLD,&request[0]);

            //ricevo la riga superiore dal processo precedente
            MPI_Irecv(top_row,n,MPI_CHAR,myrank - 1,0,MPI_COMM_WORLD,&request[0]);
            //ricevo la riga inferiore dal processo successivo
            MPI_Irecv(bottom_row,n,MPI_CHAR,myrank + 1,0,MPI_COMM_WORLD,&request[1]);
        }

        //printf("-----------------Sono il processo %d ed ho riucevuto questa top row:---------------------\n",myrank);
        //printMatrix(top_row,1,n);
        //printf("-----------------Sono il processo %d ed ho ricevuto questa bottom row:-------------------\n",myrank);
        //printMatrix(bottom_row,1,n);

        if(my_row_num >= 3){ // se ho più di 3 righe possso iniziare già a computare le righe non ai bordi
            for(int i = 1; i<my_row_num - 1; i++){
                for(int j = 0; j<n; j++){
                    if(sub_forest[i * n + j] == 'B')
                        sub_matrix[i * n + j] = 'E'; //Se è già Burned allora ora la cella diventa vuota
                    else if(sub_forest[i * n + j] == 'E'){
                        int rand_num = 1 + (rand() % 100); // se è vuota, allora con probabilità prob_grown può crescere un albero nella cella
                        if(rand_num <= prob_grow){
                            sub_matrix[i * n + j] = 'T';
                        } else
                            sub_matrix[i * n + j] = 'E';
                    }else if(sub_forest[i * n + j] == 'T'){
                        check_neighbors(sub_forest,sub_matrix,my_row_num,n,i,j,prob_burn);
                    }
                }
            }
        }

        MPI_Wait(&request[0],MPI_STATUSES_IGNORE);
        MPI_Wait(&request[1],MPI_STATUSES_IGNORE);

        //controllo le righe ai bordi
        for(int i = 0;;i = my_row_num - 1){
            for(int j = 0; j < n; j++){
                if(sub_forest[i * n + j] == 'B')
                    sub_matrix[i * n + j] = 'E';
                else if(sub_forest[i * n + j] == 'E'){
                    int rand_num = 1 + (rand() % 100); // se è vuota, allora con probabilità prob_grown può crescere un albero nella cella
                    if(rand_num <= prob_grow){
                        sub_matrix[i * n + j] = 'T';
                    } else 
                        sub_matrix[i * n + j] = 'E';
                }else if(sub_forest[i * n + j] == 'T'){
                    check_borders(sub_forest,sub_matrix,top_row,bottom_row,i,j,my_row_num,n,prob_burn);
                }
                if(sub_matrix[i * n + j] == 'E'){
                    empty_count++;
                }
            }
            if(i == my_row_num -1)
                break;
        }

        //printf("Sono il processo %d, questa è la porzione di foresta che ho ricevuto:\n",myrank);
        //printMatrix(sub_forest,send_counts[myrank]/n,n);

        //printf("Sono il processo %d ecco la mia foresta dopo il controllo:\n",myrank);
        //printMatrix(sub_matrix,my_row_num,n);
        //printf("\n");

        tmp = sub_forest;
        sub_forest = sub_matrix;
        sub_matrix = tmp;

        MPI_Reduce(&empty_count,&empty_recv_count,1,MPI_INT,MPI_SUM,0,MPI_COMM_WORLD);
        if(myrank == 0){
            //printf("Contatore vuote: %d\n",empty_recv_count);
            if(empty_recv_count == m*n){
                all_empty = 1;
            }
        }

        MPI_Bcast(&all_empty,1,MPI_INT,0,MPI_COMM_WORLD);
        if(all_empty == 1){
            if(myrank == 0)
                printf("Foresta vuota, numero di iterazioni: %d\n",i);
            break;
        }
    }

    MPI_Gatherv(sub_forest,send_counts[myrank],MPI_CHAR,forest,send_counts,displ,MPI_CHAR,0,MPI_COMM_WORLD);

    if(myrank == 0){
        printf("Ecco la foresta finale:\n");
        printMatrix(forest,m,n);
        print_on_file(forest,m,n,fptr);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    if(myrank == 0){
         printf("Time in ms = %f\n", end-start);
    }
    MPI_Finalize();

    return 0;
}

void forest_initialization(char *forest,int num_row,int num_col){
    for(int i = 0; i<num_row; i++){
        for(int j = 0; j<num_col; j++){
            int rand_num = 1 + (rand() % 100);
            if(rand_num > 70)
                forest[i* num_col + j] = 'T';
            else if(rand_num <= 50)
                forest[i * num_col + j] = 'E';
            else
                forest[i * num_col + j] = 'B';
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
    //se controllando l'elemento a sinistra sono ancora della riga (quindi il resto della divisione deve essere diverso da n-1 altrimenti vuol dire che sono al primo elemento della riga)
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
    if(matrix2[i * num_col + j] == 'B' || rand_num < prob_burn){ //verifico anche se negli if precedenti non l'ho già bruciato, altrimenti poi se non viene soddisfatto il secondo valore dell'OR inserirei un albero in una cella dove uno stava bruciando
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
        if(top_row[i * num_col + j] == 'B')
            matrix2[i * num_col + j] = 'B'; 
        if((i * num_col) + j + num_col < num_row * num_col){
            if (forest[((i * num_col) + j + num_col)] == 'B'){
                matrix2[i * num_col + j] = 'B';
            }
        }
            //se abbiamo una sola riga dobbiamo fare il confreonto sia con top che con bottom
            //ci assicuriamo anche che non siamo alla fine della matrice
        else if((bottom_row[i * num_col + j] == 'B'))
            matrix2[i * num_col + j] = 'B';   
        
    //se sono all'ultima allora controllo con bottom_row
    } else if(i == num_row - 1){
        if(bottom_row[j] == 'B')
            matrix2[i * num_col + j] = 'B';   
        //verifico se il vicino della riga superiore sta bruciando
        if(((i * num_col) + j - num_col) >=0 && forest[((i * num_col) + j - num_col)] == 'B')
            matrix2[i * num_col + j] = 'B';  
    }

    //se nessun vicino è burned allora con probabilità prob_burn può diventare burned
    int rand_num = 1 + (rand() % 100);
    if(matrix2[i * num_col + j] == 'B' || rand_num < prob_burn){ //verifico anche se negli if precedenti non l'ho già bruciato, altrimenti poi se non viene soddisfatto il secondo valore dell'OR inserirei un albero in una cella dove uno stava bruciando
        matrix2[i * num_col + j] = 'B';
    } else{
        matrix2[i * num_col + j] = 'T';
    }
}

void print_on_file(char *matrix, int num_row, int num_col, FILE *file){
    for(int i = 0; i<num_row; i++){
        for(int j = 0; j<num_col; j++){
            fprintf(file," [%c] ",matrix[i* num_col + j]);
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n");
}