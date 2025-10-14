#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>

void printargs(int argc, char** argv){
   printf("args: ");
   for(int i = 0; i<argc; i++){
      printf("%s ", argv[i]);
   }
   printf("\n");
}

int main(int argc, char** argv){
   printargs(argc, argv);
   
   char opt = getopt(argc, argv, "abc:");
   printargs(argc, argv);
   printf("ret: %c, arg: %s\n", opt, optarg);
   
   opt = getopt(argc, argv, "abc:");
   printargs(argc, argv);
   printf("ret: %c, arg: %s\n", opt, optarg);
   
   opt = getopt(argc, argv, "abc:");
   printargs(argc, argv);
   printf("ret: %c, arg: %s\n", opt, optarg);
   
   opt = getopt(argc, argv, "abc:");
   printargs(argc, argv);
   printf("ret: %c, arg: %s\n", opt, optarg);
}
