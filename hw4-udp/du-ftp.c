#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/stat.h>

#include "du-ftp.h"
#include "du-proto.h"


#define BUFF_SZ 65536
static char sbuffer[BUFF_SZ];
static char rbuffer[BUFF_SZ];
static char full_file_path[FNAME_SZ+1];

/*
 *  Helper function that processes the command line arguements.  Highlights
 *  how to use a very useful utility called getopt, where you pass it a
 *  format string and it does all of the hard work for you.  The arg
 *  string basically states this program accepts a -p or -c flag, the
 *  -p flag is for a "pong message", in other words the server echos
 *  back what the client sends, and a -c message, the -c option takes
 *  a course id, and the server looks up the course id and responds
 *  with an appropriate message. 
 */
static int initParams(int argc, char *argv[], prog_config *cfg){
    int option;
    //setup defaults if no arguements are passed
    static char cmdBuffer[64] = {0};

    //setup defaults if no arguements are passed
    cfg->prog_mode = PROG_MD_CLI;
    cfg->port_number = DEF_PORT_NO;
    strcpy(cfg->file_name, PROG_DEF_FNAME);
    strcpy(cfg->svr_ip_addr, PROG_DEF_SVR_ADDR);
    
    while ((option = getopt(argc, argv, ":p:f:a:csh")) != -1){
        switch(option) {
            case 'p':
                strncpy(cmdBuffer, optarg, sizeof(cmdBuffer));
                cfg->port_number = atoi(cmdBuffer);
                break;
            case 'f':
                strncpy(cfg->file_name, optarg, sizeof(cfg->file_name));
                break;
            case 'a':
                strncpy(cfg->svr_ip_addr, optarg, sizeof(cfg->svr_ip_addr));
                break;
            case 'c':
                cfg->prog_mode = PROG_MD_CLI;
                break;
            case 's':
                cfg->prog_mode = PROG_MD_SVR;
                break;
            case 'h':
                printf("USAGE: %s [-p port] [-f fname] [-a svr_addr] [-s] [-c] [-h]\n", argv[0]);
                printf("WHERE:\n\t[-c] runs in client mode, [-s] runs in server mode; DEFAULT= client_mode\n");
                printf("\t[-a svr_addr] specifies the servers IP address as a string; DEFAULT = %s\n", cfg->svr_ip_addr);
                printf("\t[-p portnum] specifies the port number; DEFAULT = %d\n", cfg->port_number);
                printf("\t[-f fname] specifies the filename to send or recv; DEFAULT = %s\n", cfg->file_name);
                printf("\t[-p] displays what you are looking at now - the help\n\n");
                exit(0);
            case ':':
                perror ("Option missing value");
                exit(-1);
            default:
            case '?':
                perror ("Unknown option");
                exit(-1);
        }
    }
    return cfg->prog_mode;
}

int server_loop(dp_connp dpc, void *sBuff, void *rBuff, int sbuff_sz, int rbuff_sz){
    int rcvSz;
    dp_ftp_pdu *inPdu = (dp_ftp_pdu*)rBuff;
    FILE *f = NULL;
    char filepath[FNAME_SZ];
    long timestamp;
    
    if (dpc->isConnected == false){
        perror("Expecting the protocol to be in connect state, but its not");
        exit(-1);
    }
    
    rcvSz = dprecv(dpc, rBuff, rbuff_sz);
    
    if (inPdu->mtype != DP_FTP_MDATA){
        printf("ERROR: Expected metadata PDU, got type %d\n", inPdu->mtype);
        exit(-1);
    }
    
    char *metadata = (char*)(rBuff + sizeof(dp_ftp_pdu));
    
    if (sscanf(metadata, "%150[^;];%ld", filepath, &timestamp) != 2){
        printf("ERROR: Failed to parse metadata\n");
        exit(-1);
    }
    
    printf("Receiving file from client: %s (modified: %ld)\n", filepath, timestamp);
    printf("Receiving file as: %s \n",full_file_path);
    
    f = fopen(full_file_path, "wb");
    if(f == NULL){
        printf("ERROR: Cannot open file %s for writing\n", filepath);
        exit(-1);
    }
    
    while(1) {
        rcvSz = dprecv(dpc, rBuff, rbuff_sz);
        
        if (rcvSz == DP_CONNECTION_CLOSED){
            fclose(f);
            printf("File transfer complete, client closed connection\n");
            return DP_CONNECTION_CLOSED;
        }
        
        if (inPdu->mtype != DP_FTP_FDATA){
            printf("ERROR: Expected file data PDU, got type %d\n", inPdu->mtype);
            fclose(f);
            return -1;
        }
        
        int data_size = inPdu->msize;
        if (data_size > 0){
            fwrite(rBuff + sizeof(dp_ftp_pdu), 1, data_size, f);
            printf("Received %d bytes of file data\n", data_size);
        }
    }
    
    return 0;
}

void start_client(dp_connp dpc){
    static char sBuff[BUFF_SZ];
    dp_ftp_pdu *outPdu = (dp_ftp_pdu*)sBuff;
    outPdu->proto_ver = 1;

    if(!dpc->isConnected) {
        printf("Client not connected\n");
        return;
    }

   struct stat st;
   if (stat(full_file_path, &st) != 0) {
      printf("ERROR: Cannot stat file %s\n", full_file_path);
      exit(-1);
   }

    outPdu->mtype=DP_FTP_MDATA;
   int metadata_len = sprintf((char*)(sBuff + sizeof(dp_ftp_pdu)), "%s;%ld", full_file_path, st.st_mtim.tv_sec);
    outPdu->msize = metadata_len + 1;
   dpsend(dpc, sBuff, metadata_len + sizeof(dp_ftp_pdu));



    FILE *f = fopen(full_file_path, "rb");
    if(f == NULL){
        printf("ERROR:  Cannot open file %s\n", full_file_path);
        exit(-1);
    }

    int bytes = 0;

    outPdu->mtype=DP_FTP_FDATA;


    while ((bytes = fread(sBuff + sizeof(dp_ftp_pdu), 1, BUFF_SZ - sizeof(dp_ftp_pdu), f )) > 0){
       outPdu->msize=bytes;
       dpsend(dpc, sBuff, bytes + sizeof(dp_ftp_pdu));
    }

    fclose(f);
    dpdisconnect(dpc);
}

void start_server(dp_connp dpc){
    server_loop(dpc, sbuffer, rbuffer, sizeof(sbuffer), sizeof(rbuffer));
}


int main(int argc, char *argv[])
{
    prog_config cfg;
    int cmd;
    dp_connp dpc;
    int rc;


    //Process the parameters and init the header - look at the helpers
    //in the cs472-pproto.c file
    cmd = initParams(argc, argv, &cfg);

    printf("MODE %d\n", cfg.prog_mode);
    printf("PORT %d\n", cfg.port_number);
    printf("FILE NAME: %s\n", cfg.file_name);

    switch(cmd){
        case PROG_MD_CLI:
            //by default client will look for files in the ./outfile directory
            int full_size = snprintf(full_file_path, sizeof(full_file_path), "./outfile/%s", cfg.file_name);
            full_file_path[full_size]=0;

            dpc = dpClientInit(cfg.svr_ip_addr,cfg.port_number);
            rc = dpconnect(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_client(dpc);
            exit(0);
            break;

        case PROG_MD_SVR:
            //by default server will look for files in the ./infile directory
            int ffull_size = snprintf(full_file_path, sizeof(full_file_path), "./infile/%s", cfg.file_name);
            full_file_path[ffull_size]=0;

            dpc = dpServerInit(cfg.port_number);
            rc = dplisten(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_server(dpc);
            break;
        default:
            printf("ERROR: Unknown Program Mode.  Mode set is %d\n", cmd);
            break;
    }
}
