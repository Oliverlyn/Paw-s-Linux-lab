gcc -c z_msg.c -Wall -Wextra
gcc -c main.c -Wall -Wextra
gcc main.o z_msg.o -o main -pthread -Wall -Wextra
gcc msg_recv.c -o msg_recv -Wall -Wextra
