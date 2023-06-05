#!/bin/bash

print_usage() {
  echo "Utilizzo: $0 [opzioni]"
  echo "Opzioni:" 
  echo "--row -r permette di impostare il numero delle righe"
  echo "--column -c permette di impostare il numero di colonne"
  echo "--steps -s permette di impostare il numero degli step dell'algoritmo"
  echo "--processors -p permette di impostare il numero di processori su cui eseguire il programma parallelo"
  echo "--help -h Mostra questo messaggio di aiuto"
}

row=''
column=''
steps=''
processors=''

if [ -e output_parallelo ]; then
    rm output_parallelo
fi

if [ -e output_sequenziale ]; then
    rm output_sequenziale
fi

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
    *)
    echo "Parametro non ammesso, utilizza -h o --help se non sai come utilizzarmi"
    exit 1
    ;;
esac
done

# Verifica che siano stati specificati esattamente 4 parametri con l'opzione "-p" o "--parametro"
if [ -z "$row" ] || [ -z "$column" ] || [ -z "$steps" ] || [ -z "$processors" ]; then
    echo "Errore: specificare esattamente 4 parametri per i dettagli su come utilizzare i parametri usa --help"
    exit 1
fi

echo "Ecco le impostazioni che hai scelto:"
echo "Numero di righe: $row"
echo "Numero di colonne: $column"
echo "Numero di step: $steps"
echo "Numero di processori: $processors"
echo

if [ ! -e seq ]
then
    gcc -o seq forest_fire_seq_test.c
fi
./seq $row $column $steps >/dev/null;

if [ ! -e parallel ]
then 
    mpicc -o parallel forest_fire_parallel_test.c
fi
mpirun --allow-run-as-root --mca btl_vader_single_copy_mechanism none -np $processors --oversubscribe  parallel $row $column $steps >/dev/null

if [[ -e output_parallelo ]] && [[ -e output_sequenziale ]]; then
    if diff output_parallelo output_sequenziale >/dev/null; then
        echo "Le foreste sono uguali!"
    else
        echo "Le foreste sono diverse!"
    fi
fi