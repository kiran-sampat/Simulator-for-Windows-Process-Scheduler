## Simulator for Windows Process Scheduler

### About
- Process Scheduling, Operating System APIs, Threading, and Concurrency.
- A process simulator implemented in POSIX C.

### Compile
```
gcc -std=gnu99 -Wall -pedantic-errors -ggdb3 -o processSimulator coursework.c linkedlist.c processSimulator.c -pthread
```

### Run
```
./processSimulator > output.html
./processSimulator | grep "SVG:" | sed -e 's/SVG: //g' > svgTest.html
./processSimulator | grep "TXT:" | sed -e 's/TXT: //g' > txtTest.html
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./processSimulator > output.html
```
