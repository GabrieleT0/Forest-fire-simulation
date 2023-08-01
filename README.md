# Forest-fire-simulation
## Index
1. [Introduction](#introduction)

2. [Description of the proposed solution](#description-of-the-proposed-solution)
3. [Implementation details](#implementation-details)
4. [Algorithm correctness](#algorithm-correctness)
5. [Benchmarks](#benchmarks)
6. [Conclusions](#conclusions)

## Introduction
This project is a possible implementation of the Forest Fire algorithm in C using Open MPI, an implementation of MPI. The problem is well-described here: https://en.wikipedia.org/wiki/Forest-fire_model. In short, this implementation parallelizes the simulation of a forest fire, with the following requirements:

1. A burnt cell becomes empty.
2. A tree burns if at least one of its neighbors is burning.
3. A tree catches fire with probability $b$ even if no neighbors are burning.
4. A vacant cell can have a tree growing with probability $p$.

The simulation can terminate when the entire forest is empty or when a specified number of iterations $S$ is reached (given as input by the user to the program).
## Description of the Proposed Solution
The proposed solution requires the user to input the size of the matrix ($N$ and $M$) and the number of algorithm steps ($S$). The master process ($rank$ $0$) initializes the matrix by randomly filling a cell with one of the following values:
- $T$: there is a tree in the cell.
- $E$: the cell is empty.
- $B$: the tree is burning.

The master node then divides the rows of the matrix evenly among the available processes, and each process will work on a portion of the matrix. Each process communicates with its neighboring processes to exchange the rows at the boundaries of its submatrix, in order to check if the tree should catch fire. If the received submatrix has at least 3 rows, the process can perform part of the simulation without communication (except when considering the edges), which speeds up the simulation. At each iteration, each process also keeps track of the empty cells and communicates this information to the master node, which will terminate the simulation when the number of empty cells is equal to the total number of cells in the matrix. Otherwise, the simulation will terminate after $S$ iterations.

## Implementation Details
In this section, the most significant parts of the implementation code will be detailed.
### Forest Initialization
```C
void forest_initialization(char *forest, int num_row, int num_col){
    for(int i = 0; i < num_row; i++){
        for(int j = 0; j < num_col; j++){
            int rand_num = 1 + (rand() % 100);
            if(rand_num > 70)
                forest[i * num_col + j] = 'T';
            else if(rand_num <= 50)
                forest[i * num_col + j] = 'E';
            else
                forest[i * num_col + j] = 'B';
        }
    }
}
```
The seed used by rand is the system time. For each cell, a random number between 0 and 100 is generated. If the number is greater than 70, a tree is placed in the cell. If the number is less than or equal to 50, the cell is empty, otherwise, the tree is set on fire.

### Asynchronous Communication of Submatrix Borders
```C
    if(myrank == 0){
        //send the last row to the next process
        MPI_Isend(&sub_forest[send_counts[myrank] - n], n, MPI_CHAR, myrank + 1, 0, MPI_COMM_WORLD, &request[0]);
        //receive the first row from the next process
        MPI_Irecv(bottom_row, n, MPI_CHAR, myrank + 1, 0, MPI_COMM_WORLD, &request[1]);
    } else if(myrank == numtasks - 1){
        //send the first row to the previous process
        MPI_Isend(sub_forest, n, MPI_CHAR, myrank - 1, 0, MPI_COMM_WORLD, &request[1]);
        //receive the last row from the previous process
        MPI_Irecv(top_row, n, MPI_CHAR, myrank - 1, 0, MPI_COMM_WORLD, &request[0]);
    } else {
        //send the first row to the previous process
        MPI_Isend(sub_forest, n, MPI_CHAR, myrank - 1, 0, MPI_COMM_WORLD, &request[1]);
        //send the last row to the next process
        MPI_Isend(&sub_forest[send_counts[myrank] - n], n, MPI_CHAR, myrank + 1, 0, MPI_COMM_WORLD, &request[0]);
    
        //receive the upper row from the previous process
        MPI_Irecv(top_row, n, MPI_CHAR, myrank - 1, 0, MPI_COMM_WORLD, &request[0]);
        //receive the lower row from the next process
        MPI_Irecv(bottom_row, n, MPI_CHAR, myrank + 1, 0, MPI_COMM_WORLD, &request[1]);
    }
```
The chosen communication is non-blocking because it allows the processes to perform computation while the messages are being exchanged. Each process, except the process with rank 0 and the last one (rank = numtasks - 1), sends its first row to the previous process, as it needs the lower boundary for neighbor checking. It also sends its last row to the next process, as it needs the upper boundary for neighbor checking. The process with rank 0 only receives the lower boundary and sends its last row to the next process. The last process sends its first row to the previous process and receives the last row from the previous process.
### Independent Computation of Received Rows
This part of the code will be executed without waiting for the send and receive operations to complete. In particular, each process can perform computation independently of other processes if the received submatrix has more than 3 rows. This mechanism allows faster execution of the simulation. The computation for the rows in the center of the submatrix does not depend on the edges, so it can be done without communication..
```C
int my_row_num = send_counts[myrank] / n;

if(my_row_num >= 3){ // computation for rows not at the edges
            for(int i = 1; i<my_row_num - 1; i++){
                for(int j = 0; j<n; j++){
                    if(sub_forest[i * n + j] == 'B')
                        sub_matrix[i * n + j] = 'E'; //If already burned, then now is empty
                    else if(sub_forest[i * n + j] == 'E'){
                        int rand_num = 1 + (rand() % 100); // if empty, then with prob_grown probability, can grow a tree in the cell
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
```
For each iteration, a support matrix ```sub_matrix``` is used to store the new values of the matrix. At the end of the iteration, the ```sub_matrix``` will be swapped with ```sub_forest``` for the next step. When considering a cell with a tree, the function ```check_neighbors``` is called
```C
void check_neighbors(char *forest,char *matrix2,int num_row,int num_col,int i,int j,int prob_burn){
    //check on the right and on the left

    //if look at the right and i'm already in the row (so the remainder from the division is 0 otherwise this mean that i'm at the last element)
    if(((i * num_col)  + j + 1) % num_col != 0 && forest[((i * num_col) + j + 1)] == 'B')
        matrix2[i * num_col + j] = 'B';
    ///if look at the left and i'm already in the row (so the remainder from the division is n-1 otherwise this mean that i'm at the first element)
    if(((i * num_col)  + j - 1) % num_col != num_col-1 && forest[((i * num_col) + j - 1)] == 'B')
        matrix2[i * num_col + j] = 'B';

    //check on top and at the bottom
    
    //I check if the neighbor on the lower row is burning
    if(((i * num_col) + j + num_col) < num_row * num_col && forest[((i * num_col) + j + num_col)] == 'B')
        matrix2[i * num_col + j] = 'B';

    //I check if the neighbor on the upper row is burning
    if(((i * num_col) + j - num_col) >=0 && forest[((i * num_col) + j - num_col)] == 'B')
        matrix2[i * num_col + j] = 'B';   

    //if no neighbor is burned then with probability prob_burn it can become burned
    int rand_num = 1 + (rand() % 100);
    if(matrix2[i * num_col + j] == 'B' || rand_num < prob_burn){ //I also check if in the previous ifs I haven't already burned it, otherwise then if the second value of the OR is not satisfied, I would insert a tree in a cell where one was burning
        matrix2[i * num_col + j] = 'B';
    } else{
        matrix2[i * num_col + j] = 'T';
    }
}
```
This function, for the cell we are considering, checks the state of the neighbors to the right, left, above, and below. If any of them is burning, then the cell will become $B$. Otherwise, if none of them is burning with a probability of ```prob_burn```, the tree will catch fire.
### Border Rows Control
For the computation of border rows, we need the rows from other processes. So, two ```MPI_Wait``` calls are used on ```request[0]``` and ```request[1]``` respectively, after which we can proceed to check the first and last rows of each process's submatrix. This is done using the ```check_borders``` function, which takes the received upper and lower rows as input and performs the checks.
```C
void check_borders(char *forest, char *matrix2,char *top_row,char *bottom_row,int i,int j, int num_row, int num_col,int prob_burn){
    // check to the right and left
    if(((i * num_col)  + j + 1) % num_col != 0 && forest[((i * num_col) + j + 1)] == 'B')
        matrix2[i * num_col + j] = 'B';
    if(((i * num_col)  + j - 1) % num_col != num_col-1 && forest[((i * num_col) + j - 1)] == 'B')
        matrix2[i * num_col + j] = 'B';
        
    // if considering the first row, then check with top_row
    if(i == 0){
        if(top_row[i * num_col + j] == 'B')
            matrix2[i * num_col + j] = 'B'; 
        if((i * num_col) + j + num_col < num_row * num_col){
            if (forest[((i * num_col) + j + num_col)] == 'B'){
                matrix2[i * num_col + j] = 'B';
            }
        }
        // if we have only one row, we need to check both top and bottom
        // also, ensure that we are not at the end of the matrix
        else if((bottom_row[i * num_col + j] == 'B'))
            matrix2[i * num_col + j] = 'B';   
        
    // if it's the last row, then check with bottom_row
    } else if(i == num_row - 1){
        if(bottom_row[j] == 'B')
            matrix2[i * num_col + j] = 'B';   
        // check if the neighbor of the upper row is burning
        if(((i * num_col) + j - num_col) >=0 && forest[((i * num_col) + j - num_col)] == 'B')
            matrix2[i * num_col + j] = 'B';  
    }

    // if no neighbor is burning, then with probability prob_burn it can catch fire
    int rand_num = 1 + (rand() % 100);
    if(matrix2[i * num_col + j] == 'B' || rand_num < prob_burn){ // check if it hasn't been burned in the previous conditions, otherwise, we would set a tree on fire in a cell where one was already burning
        matrix2[i * num_col + j] = 'B';
    } else{
        matrix2[i * num_col + j] = 'T';
    }
}
```
The function is similar to the previous one, and we also need to consider the case where the process received only one row. In this case, when reading row 0, we must compare the values in the cell with both the received upper row and the lower row.
### Termination of the simulation if the forest is empty
To terminate the simulation when the forest is empty, each process increments a counter of empty cells, which is then reset at each new iteration.
```C
MPI_Reduce(&empty_count,&empty_recv_count,1,MPI_INT,MPI_SUM,0,MPI_COMM_WORLD);
        if(myrank == 0){
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
```
The MPI_Reduce is used so that the sum of the counters received from each process (empty_count) is computed. If the sum is equal to the total number of elements in the matrix, then a flag all_empty is set to $true$, and the master process broadcasts this flag. Each process checks the value of the flag, and if it is 1, the process stops and ends the computation (the master process displays on the terminal after how many iterations the forest became empty).
### Output of the simulation
At the end of the steps or when the forest is empty, an MPI_Gather is executed, where the master process receives all the submatrices and prints both on the terminal and in a file the state of the forest at the end of the algorithm.

```C
MPI_Gatherv(sub_forest,send_counts[myrank],MPI_CHAR,forest,send_counts,displ,MPI_CHAR,0,MPI_COMM_WORLD);
```
## Algorithm correctness
To facilitate the correctness testing of the algorithm, a sequential version of the algorithm and a parallel version that prints the forest at each iteration were implemented. This way, we can check if in each step of the algorithm, the forests are always the same. Both programs output the state of the forest at each iteration to a file. To speed up the test, a small bash script was created that runs both programs and then uses the ```diff``` command to check if the two files are identical.

Example for checking correctness on a 50x50 matrix with 50 steps and 4 processors (for the parallel program). From the root directory of the project, execute the following commands.

```shell
 git clone https://github.com/GabrieleT0/Forest-fire-simulation.git
 cd Forest-fire-simulation/test_correttezza
 ./check_correctness.sh --row 50 --column 50 --steps 40 --processors 4
 #or
 ./check_correctness.sh -r 50 -c 50 -s 40 -p 4

```
The output of the program execution is stored in the files ```output_sequenzial```e and ```output_parallelo```. If the two files contain the same output, it means that in $S$ iterations, the state of the forest was always the same, both in the program executed with $p$ processors and in the sequential program. This assures us of the correctness of the parallel program.

## Benchmarks
Before going into detail in showing the benchmark data, it is important to make the following considerations.
The benchmarks were run on a cluster of virtual machines created on Google Cloud. Specifically, the cluster was created with 4 machines of type e2-standard-8. These machines have 8 vCPUs, but only 4 vCPUs are exposed to the operating system. Therefore, to obtain more realistic data and only use the real cores available on the machine, the test was run with processors ranging from 1 to 16 (without oversubscribe). All execution times shown below are the average of 10 consecutive simulation runs.
### Strong Scalability
In the strong scalability test, the execution times of the simulation on a matrix of size $3000*3000$, running 100 steps, and with the following probabilities were recorded:

```C
    int prob_burn = 50;  // probability that a tree catches fire
    int prob_grow = 50;  // probability that a tree grows in an empty cell
```

![Strong Scalability](img/strong_scalability.png?raw=true)

The table below provides detailed data, including the speedup as the number of processors increases. In calculating the time, the time taken by the MASTER node to initialize the matrix at the beginning of the simulation was excluded, which takes $8.3082s$ (calculated over 10 runs).

| PROCESSORI |  TEMPO (sec.)| SPEEDUP |
|------------|--------------|---------|
| 1          | 31,3127      | 1.00000 |
| 2          | 15,9797      | 1,9595  |
| 3          | 10,8956      | 2,8738  |
| 4          | 8,55186      | 3,6615  |
| 5          | 6,85979      | 4,5646  |
| 6          | 5,84798      | 5,3544  |
| 7          | 5,10273      | 6,1364  |
| 8          | 4,65547      | 6,7260  |
| 9          | 4,19364      | 7,4667  |
| 10         | 3,82968      | 8,1763  |
| 11         | 3,58176      | 8,7422  |
| 12         | 3,32661      | 9,4127  |
| 13         | 3,14370      | 9,9604  |
| 14         | 2,98696      | 10,483  |
| 15         | 3,58176      | 10,997  |
| 16         | 3,32661      | 11,708  |

### Weak Scalability
In the weak scalability test, the execution time was recorded as the number of rows and the number of processors used for the simulation increased. Specifically, the number of columns was fixed at 500, while the number of rows was set to np*200 (np = number of processors), with 100 steps in the algorithm.

![Scalabilità debole](img/weak_scalability.png?raw=true)

| PROCESSORI |  TEMPO (sec.) | EFFICIENZA  |
|------------|---------------|-------------|
| 1          | 0,0149        | 100         |
| 2          | 0,0296        | 50,3378     |
| 3          | 0,0443        | 33,6343     |
| 4          | 0,0595        | 25,0420     |
| 5          | 0,0738        | 10,1897     |
| 6          | 0,0895        | 16,6480     |
| 7          | 0,1037        | 14,3683     |
| 8          | 0,1197        | 12,4477     |
| 9          | 0,1372        | 11,2283     |
| 10         | 0,1485        | 10,0275     |
| 11         | 0,1623        | 9,18052     |
| 12         | 0,1774        | 8,39909     |
| 13         | 0,1913        | 7,78881     |
| 14         | 0,2059        | 7,23652     |
| 15         | 0,2216        | 6,72382     |
| 16         | 0,2366        | 6,29754     |

## Conclusions
As evident from the strong scalability test, the algorithm definitely benefits from the parallel execution of the simulation. However, note that the decrease in time is significant especially up to 8 processors. Beyond 8 processors, the decrease in time tends to flatten out. This is mainly due to the increase in communication as the number of processors increases, creating overhead. Specifically, more processors have to execute the MPI_Reduce and MPI_Bcast operations, which are blocking (unlike the send and receive operations used), negatively impacting the execution time. Therefore, for the given algorithm, a good compromise between time and the cost of investing in processors is certainly achieved by stopping at 8 processors. The weak scalability test confirms this hypothesis, as when increasing the number of processors from 8 onwards, the time starts to increase significantly, whereas up to 8 processors, the increase is acceptable.
