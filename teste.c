#define FUSE_USE_VERSION 31
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
typedef char byte;


byte *disco;
FILE *Tmp_disco;


int main(){  
    disco = calloc(10, 10);  
    printf("Sucesso 00");
    if((Tmp_disco = fopen("Persistencia.txt","w+b"))!=NULL){
        printf("Sucesso 01");  
        Tmp_disco = fopen("Persistencia.txt","w");     
    }
    else{    
        printf("Sucesso 02");       
        Tmp_disco = fopen("Persistencia.txt","r+b");     
        }     
        fwrite(disco,1,sizeof(disco),Tmp_disco);     
        fclose(Tmp_disco); 
}