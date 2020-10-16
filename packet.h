#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#define MAXPACKET 99

typedef struct packet{
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char *filename;
    char filedata[1000];
} packet;

void printPack(packet *a){
    printf("packet %d/%d\n",a->frag_no, a->total_frag);
    printf("size: %d \n",a->size);
    printf("file name: %s\n", a->filename);
    printf("content:\n%s\n", a->filedata);
}

bool file_exist(const char* filename){
    if(access(filename, F_OK)==0){
        return true;
    }
    return false;
}


int findPacketNum(FILE *file){
    fseek(file, 0 ,SEEK_END);
    long int size = ftell(file);
    int num = (int)(size/1000);
    if(num*1000 < size){
        num++;
    }
    return num;
}

//returns pointer to start of a packet array that holds all the packets 
packet* fileIntoPackets(char *filename){

    if(file_exist(filename)){
        char r = 'r';
        

        FILE *file = fopen(filename, &r);
        if(file ==NULL ){
            printf("file is NULL\n");

        }
        int numPacketsNeeded = findPacketNum(file);
        
        fseek(file, 0, SEEK_SET);
        //printf("Need %d packets\n", numPacketsNeeded );
        packet *pack = (packet *) malloc(numPacketsNeeded*sizeof(packet));

        for(int i=0;i<numPacketsNeeded;i++){

            pack[i].total_frag = numPacketsNeeded;
            //printf("total_frag %d\n", pack[i].total_frag);
            
            pack[i].frag_no = (unsigned int)(i+1);
           // printf("frag_no %d\n", pack[i].frag_no);
            
            pack[i].filename = filename;
            //printf("filename %s\n", pack[i].filename);
            int file_size = fread(pack[i].filedata, 1, 999, file);
            pack[i].size = file_size;
            //printf("file size is: %d\n", pack[i].size);
            pack[i].filedata[pack[i].size] = '\0';
        }
        fclose(file);
        
        return pack;
    }
    else{
        //error code;
        return NULL;
    }
}

char intToChar(int a){
    char b= 0;
    b = a + '0';
    return b;
 }

 //make sure a is a char representing 0-9
int charToInt(char a){
    return a-'0';
    
}



//str must point to a string of size 1500 bytes;
char* packettoString(packet a){
    char *p = (char *)malloc(12 + strlen(a.filename) + a.size);
    char COLON = ':';
    p[0] = intToChar(a.total_frag/10);
    p[1] = intToChar(a.total_frag%10);
    p[2] = COLON;
    p[3] = intToChar(a.frag_no/10);
    p[4] = intToChar(a.frag_no%10);
    p[5] = COLON;
    p[6] = intToChar(a.size/100);

    if(a.size>99){
        p[7] = intToChar((a.size/10)%10);
    }
    else {
        p[7] = intToChar(a.size/10);
    }
    p[8] = intToChar(a.size%10);
    p[9] = COLON;

    int index = 10;
    for(int itera=0; itera<strlen(a.filename); itera++){
        p[index] = a.filename[itera];
        index++;
    }
    p[index] = COLON;
    index++;
    for(int i=0; i<a.size; i++){
        p[index] = a.filedata[i];
        index++;
    }
    p[index] = '\0';
    return p;
}

//to is the target machine's IP address
void sendfile(int sockfd, const struct sockaddr *to, char * filename){
    packet *all = fileIntoPackets(filename);
    int totalFrag = all[0].total_frag;
    int placeHolder;

    for(int i=0; i<totalFrag; i++){
        int size_addr = sizeof(struct sockaddr_storage);  
        char *packet_string = packettoString(all[i]);
        printf("%s\n", packet_string);
        int numByte = sendto(sockfd, packet_string, strlen(packet_string), 0,
                            to, (socklen_t)size_addr);
        printf("%d/%d packet sent.\n", (all[i].frag_no), totalFrag);
        //wait for acknowledgement
        struct sockaddr_storage *from;
        int ackk;
        printf("waiting for ack\n");
        placeHolder = recvfrom(sockfd, &ackk, sizeof(int), 0,
                            (struct sockaddr *)from, (socklen_t *)&size_addr);
        printf("The other end received %d bytes of data\n\n", ackk);
        free(packet_string);
    }
    printf("File transfer complete.\n");
    free(all);

}

int writeToFile(FILE* file, packet *a){
    int size = a->size;
    int errorcode = fputs(a->filedata, file);
    return errorcode;
}

packet *parseIntoPacket(char *str){
    packet *pack = (packet *) malloc(sizeof(packet));
    memset(pack->filedata, '\0', 1000);
    pack->total_frag = (charToInt(str[0])*10) + (charToInt(str[1]));
    pack->frag_no = (charToInt(str[3])*10) + charToInt(str[4]);
    pack->size = (charToInt(str[6]) *100) + (charToInt(str[7]) *10) + charToInt(str[8]);
    
    char *name = (char *) malloc (40);
    int itera=0, index = 10;
    
    while(str[index] != ':'){
        name[itera] = str[index];    
        itera++;
        index++;
    }
    name[itera] = '\0';
    pack->filename = name;
    //add the :
    index++;
    int loop = 0;

    while(index < (strlen(str))){
        pack->filedata[loop] = str[index];
        loop++;
        index++;
    }
    //pack->filedata[loop] = '\0';
    printPack(pack);
    return pack;
}

char *convertToString(int a){
    char *p = malloc(5*sizeof(char));
    p[0] = intToChar((int)a/1000);
    p[1] = intToChar((int)(a/100)%10);
    p[2] = intToChar((int)(a/10)%10);
    p[3] = intToChar((int)a%10);
    p[4] = '\0';
    return p;
}
void receivefile(int sockfd){
    struct sockaddr_storage *from;
    int numBytes, totalFrag, placeHolder;
    int i=0;
    int size_addr = sizeof(struct sockaddr_storage);
    char buffer[1050];
    numBytes = recvfrom(sockfd, buffer, 1050, 0,
                        (struct sockaddr *)from, (socklen_t *)&size_addr);
    printf("%d bytes are received.\n", numBytes);
    packet *first_pack = parseIntoPacket(buffer);

    totalFrag = first_pack->total_frag;
    
    
    FILE* file = fopen(first_pack->filename, "w");
    if(file ==NULL){
        printf("file opened is NULL\n");
    }
    else {
        printf("Writing to file\n");
        writeToFile(file, first_pack);
    }

    int kill_time =0;
    while (kill_time<100000000)
    {
        kill_time++;
    }
    int ackk = numBytes;
    placeHolder = sendto(sockfd, &ackk, sizeof(int), 0,
                        (struct sockaddr *)from, (socklen_t)size_addr);

    printf("sent ack number %d  %d\n", ackk, placeHolder);
    free(first_pack->filename);
    free(first_pack);

    for(int i=1; i<totalFrag; i++){
        char buf[1050];
        memset(buf, 0, 1050);
        numBytes = recvfrom(sockfd, buf, 1050, 0, 
                        (struct sockaddr *)from, (socklen_t *)&size_addr);
        ackk = numBytes;
        packet *subsequent_packet = parseIntoPacket(buf);
        printf("\n\n");

        writeToFile(file, subsequent_packet);
        kill_time =0;
        while (kill_time<100000000)
        {
              kill_time++;
        }
        placeHolder = sendto(sockfd, &ackk, sizeof(int), 0,
                (struct sockaddr *)from, sizeof(struct sockaddr_storage));
        printf("sent ack number %d\n", ackk);
        free(subsequent_packet->filename);
        free(subsequent_packet);
    }
    fclose(file);

}
