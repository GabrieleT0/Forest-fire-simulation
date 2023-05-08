#include <stdio.h>
#include <stdlib.h>
#include <time.h>
/*
B = Burned
T = Tree
E = Empty

S = Algorithm steps
*/

//stampa della matrice di char
void printMatrix(char *matrix,int num_row,int num_col);

//stampa la foresta su file
void print_on_file(char *matrix, int num_row, int num_col, FILE *file);

//controllo se gli alberi vicini sono in fiamme
void check_neighbors(char *forest,char *matrix2,int num_row,int num_col,int i,int j,int prob_burn);

//inizializzo la foresta
void forest_initialization(char *forest,int num_row,int num_com);

int main (int argc, char *argv[]){
    FILE *fptr, *fptr2;
    //fptr = fopen("output_sequenziale","w");
    fptr2 = fopen("output_sequenziale","w");

    int prob_burn = 50;  // probabilità che un albero si incendi 0 <= prob_burn <= 100
    int prob_grow = 50;  //probabilità che un albero cresca nella cella vuota, 0 <= prob_tree <= 100
    char *forest, *matrix2, *tmp;
    int empty_counter;
    //srand(time(NULL)); // usiamo l'ora corrente come seme per il generatore di numeri random

    int m,n,s;
    if(argc != 3){
        m = atoi(argv[1]);
        n = atoi(argv[2]);
        s = atoi(argv[3]);
    } else{
        printf("Inserisci il numero di righe, il numero di colonne e il numero di step dell'algoritmo (3 argomenti richiesti [row,col,step])\n");
        return 1;
    }

    forest = (char*) malloc(m * n * sizeof(char));
    matrix2 = (char*) malloc(m * n * sizeof(char));

    //inizializzo matrice
    forest_initialization(forest,m,n);

    printf("Foresta iniziale:\n");
    printMatrix(forest,m,n);
    //print_graphic_matrix(forest,m,n,fptr);
    print_on_file(forest,m,n,fptr2);

    //Ciclo per quanti sono gli step della simulazione
    for(int k = 0; k<s; k++){

        empty_counter = 0;

        //Scorro la foresta
        for(int i = 0; i<m; i++){
            for(int j = 0; j<n; j++){
                if(forest[i * n + j] == 'B')
                    matrix2[i * n + j] = 'E';  //Se è già Burned alla diventa vuota la cella ora
                else if(forest[i * n +j] == 'E'){ 
                    /*
                    int rand_num = 1 + (rand() % 100); //se è vuota, allora con probabilità prob_grown può crescere un albero nella cella
                    if(rand_num <= prob_grow){
                        matrix2[i * n + j] = 'T';
                    } else
                        matrix2[i * n + j] = 'E';
                    */
                   matrix2[i * n + j] = 'T';
                }else if(forest[i * n +j] == 'T'){ //allora abbiamo un albero nella cella e dobbiamo controllare se deve bruciare
                    
                    check_neighbors(forest,matrix2,m,n,i,j,prob_burn);
                
                }
                //se nella matrice di appoggio alla fine di tutti i controlli la cella è vuota incremento il contatore per poi controllare se per caso alla fine della s-esima iterazione la foresta è vuota
                if (matrix2[i * n + j] == 'E'){
                    empty_counter++;
                }
            }
        }
        //scambio le matrici per la prossima iterazione
        tmp = forest;
        forest = matrix2;
        matrix2 = tmp;

        printf("Foresta iterazione %d\n",k);
        printMatrix(forest,m,n);
        //print_graphic_matrix(forest,m,n,fptr);
        print_on_file(forest,m,n,fptr2);

        //controllo se la foresta è vuota e quindi devo fermarmi.
        if(empty_counter == m*n)
            break;
    }
    
    printf("Foresta finale:\n");
    printMatrix(forest,m,n);

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

void printMatrix(char *matrix,int num_row,int num_col){
    for(int i = 0; i<num_row; i++){
        for(int j = 0; j<num_col; j++){
            printf("%c ",matrix[i* num_col + j]);
        }
        printf("\n");
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