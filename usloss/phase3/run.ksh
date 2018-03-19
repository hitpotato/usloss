#!/bin/ksh

resultsDirectory="/Users/rodrigo/Documents/UniversityOfArizona/Spring_18/CS_452/USLOSS_Project/usloss/usloss/phase3/testResults"

if [ "$#" -eq 0 ] 
then
    echo "Usage: ksh run.ksh <num>"
    echo "where <num> is 00, 01, 02, ... "
    exit 1
fi

num=$1
if [ -f test${num} ]
then
    /bin/rm test${num}
fi

if  make test${num} 
then
    ./test${num} > ./output/test${num}.txt 2> test${num}stderr.txt;
    if [ -s test${num}stderr.txt ]
    then
        cat test${num}stderr.txt >> test${num}.txt
    fi

    /bin/rm test${num}stderr.txt

    if diff --brief ${resultsDirectory}/test${num}.txt ./output/test${num}.txt
    then
        echo
        echo test${num} passed!
    else
        echo
        diff -C 1 ./output/test${num}.txt ${resultsDirectory}/test${num}.txt
    fi
fi
echo
