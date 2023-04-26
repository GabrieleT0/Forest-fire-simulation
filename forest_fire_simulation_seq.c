#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define M 4
#define N 3
#define S 10 //step dell'algoritmo
#define TREE "🌲"
#define EMPTY "❌"
#define BURN "🔥"

/*
B = Burned
T = Tree
E = Empty

S = Algorithm steps
*/

//stampa della matrice di char
void printMatrix(char *matrix,int num_row,int num_col);

//stampa la matrice su un file utlizzando le emoji
void print_graphic_matrix(char *matrix,int num_row, int num_col,FILE *file);

//controllo se gli alberi vicini sono in fiamme
void check_neighbors(char *forest,char *matrix2,int num_row,int num_col,int i,int j,int prob_burn);

//inizializzo la foresta
void forest_initialization(char *forest,int num_row,int num_com);

int main (int argc, char *argv[]){
    FILE *fptr;
    fptr = fopen("output","w");

    int prob_burn = 50;  // probabilità che un albero si incendi 0 <= prob_burn <= 100
    int prob_grow = 50;  //probabilità che un albero cresca nella cella vuota, 0 <= prob_tree <= 100
    char *forest, *matrix2, *tmp;
    int empty_counter;
    srand(time(NULL)); // usiamo l'ora corrente come seme per il generatore di numeri random

    forest = malloc(sizeof(char[M][N]));
    matrix2 = malloc(sizeof(char[M][N]));

    //inizializzo matrice
    forest_initialization(forest,M,N);

    printf("Foresta iniziale:\n");
    printMatrix(forest,M,N);
    print_graphic_matrix(forest,M,N,fptr);

    //Ciclo per quanti sono gli step della simulazione
    for(int k = 0; k<S; k++){

        empty_counter = 0;

        //Scorro la foresta
        for(int i = 0; i<M; i++){
            for(int j = 0; j<N; j++){
                if(forest[i * N + j] == 'B')
                    matrix2[i * N + j] = 'E';  //Se è già Burned alla diventa vuota la cella ora
                else if(forest[i * N +j] == 'E'){ 
                    int rand_num = 1 + (rand() % 100); //se è vuota, allora con probabilità prob_grown può crescere un albero nella cella
                    if(rand_num <= prob_grow){
                        matrix2[i * N + j] = 'T';
                    } else
                        matrix2[i * N + j] = 'E';
                }else if(forest[i * N +j] == 'T'){ //allora abbiamo un albero nella cella e dobbiamo controllare se deve bruciare
                    
                    check_neighbors(forest,matrix2,M,N,i,j,prob_burn);
                
                }
                //se nella matrice di appoggio alla fine di tutti i controlli la cella è vuota incremento il contatore per poi controllare se per caso alla fine della s-esima iterazione la foresta è vuota
                if (matrix2[i * N + j] == 'E'){
                    empty_counter++;
                }
            }
        }
        //scambio le matrici per la prossima iterazione
        tmp = forest;
        forest = matrix2;
        matrix2 = tmp;

        printf("Foresta iterazione %d\n",k);
        printMatrix(forest,M,N);
        print_graphic_matrix(forest,M,N,fptr);

        //controllo se la foresta è vuota e quindi devo fermarmi.
        if(empty_counter == M*N)
            break;
    }
    
    printf("Foresta finale:\n");
    printMatrix(forest,M,N);

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
    if(((i * N) + j - num_col) >=0 && forest[((i * num_col) + j - num_col)] == 'B')
        matrix2[i * num_col + j] = 'B';   

    //se nessun vicino è burned allora con probabilità prob_burn può diventare burned
    int rand_num = 1 + (rand() % 100);
    if(matrix2[i * num_col + j] == 'B' || rand_num < prob_burn){ //verifico anche se negli if precedenti non l'ho già bruciato, altrimenti poi se non viene soddisfatto il secondo valore dell'OR inserirei un albero in una cella dove uno stava bruciando
        matrix2[i * num_col + j] = 'B';
    } else{
        matrix2[i * num_col + j] = 'T';
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

void print_graphic_matrix(char *matrix,int num_row, int num_col,FILE *file){
    for(int i=0; i<num_row; i++){
        for(int j=0; j<num_col; j++){
            if(matrix[(i*num_col) + j] == 'T'){
                fprintf(file, "[%s]", TREE);
            } else if (matrix[(i*num_col) + j] == 'B'){
                fprintf(file, "[%s]", BURN);
            } else if (matrix[(i*num_col) + j] == 'E'){
                fprintf(file, "[%s]", EMPTY);
            }
            fprintf(file, " ");
            
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n");
}