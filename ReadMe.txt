OS : Linux Ubuntu
fp.c  : main File of the project
keys/ : folder with files for keys generating

the program can be runned with 3 arguments :
1) Number of different items/products (up to 10)
2) Number of sale assistants (up to 3)
3) Number of customers (up to 10)

Default values :
1) Number of different items/products = 10;
2) Number of sale assistants = 2;
3) Number of customers = 4;

The program shows the multiproces relationships and solves reader-writer problem.
The program has:
	1. shared memory
	2. proceses "assistant"
	3. proceses "customer"

This program is working for 30 sec. 
Durin this time, subproceses can read or write data to shared memory using thread functions.
Processes access shared memory in random order.