#!/bin/bash

print_usage() {
  echo "Utilizzo: $0 [opzioni]"
  echo "Opzioni:" 
  echo "--row -r permette di impostare il numero delle righe"
  echo "--column -c permette di impostare il numero di colonne"
  echo "--steps -s permette di impostare il numero degli step dell'algoritmo"
  echo "--processors -p permette di impostare il numero di processori su cui eseguire il programma parallelo"
  echo "--num_run -n permette di specificare quante volte eseguire il programma"
  echo "--help -h Mostra questo messaggio di aiuto"
}

row=''
column=''
steps=''
processors=''
run=''

# legge i parametri dalla riga di comando scorrendoli tutti
while [[ $# -gt 0 ]] 
do
key="$1"

case $key in
    -h|--help)
    print_usage
    exit 0
    ;;
    -r|--row)
    row="$2"
    shift
    shift
    ;;
    -c|--column)
    column="$2"
    shift
    shift
    ;;
    -s|--steps)
    steps="$2"
    shift
    shift
    ;;
    -p|--processors)
    processors="$2"
    shift
    shift
    ;;
    -n|--num_run)
    run="$2"
    shift
    shift
    ;;
    *)
    echo "Parametro non ammesso, utilizza -h o --help se non sai come utilizzarmi"
    exit 1
    ;;
esac
done

# Verifica che siano stati specificati esattamente 4 parametri con l'opzione "-p" o "--parametro"
if [ -z "$row" ] || [ -z "$column" ] || [ -z "$steps" ] || [ -z "$processors" ] || [ -z "$run" ] ; then
    echo "Errore: specificare esattamente 5 parametri per i dettagli su come utilizzare i parametri usa --help"
    exit 1
fi

echo "Ecco le impostazioni che hai scelto:"
echo "Numero di righe: $row"
echo "Numero di colonne: $column"
echo "Numero di step dell'algoritmo: $steps"
echo "Numero di processori: $processors"
echo "Numero di esecuzioni del programma: $run"
echo

if [ -e time.log ]
then 
    rm time.log
fi

if [ ! -e forrest_fire_simulation ]
then 
    mpicc -o forrest_fire_simulation forest_fire_parallel.c
fi

num=$((run))

for ((i=0; i<$num; i++))
do
    mpirun --allow-run-as-root -np $processors --oversubscribe forrest_fire_simulation $row $column $steps >> time.log
done

echo "Esecuzione della simulazione terminata. Ho eseguito il progreamma $num volte. Procedo a calcolare la media del tempo di esecuzione con $processors processori."

file="time.log"
sum=0
count=0

while IFS= read -r line
do
    if [[ $line =~ [0-9]+([.][0-9]+)? ]]; then
        time=${BASH_REMATCH[0]}
        echo "Tempo impiegato nell'esecuzione numero $count: $time"
        sum=$(awk "BEGIN {print $sum + $time}")
        count=$((count + 1))
    fi
done < "$file"

if ((count > 0)); then
    average=$(awk "BEGIN {print $sum / $count}")
    echo "Tempo medio impiegato nelle $num esecuzioni: $average"
    echo "Tempo medio impiegato nelle $num esecuzioni su $processors processori: $average" >> average_times.txt 
else
    echo "Il file analizzato non contiene alcun valore."
fi

