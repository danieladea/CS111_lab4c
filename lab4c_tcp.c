
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include "mraa.h"
#include <poll.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


void offFunc();
void commandFunc(char* input);
void printExit(char* message);
float readTemp();

int period = 1;
int celsFlag = 0;
//celsius = 0 means Farenheit
int logFlag=0;
int logFD;
mraa_gpio_context button;
mraa_aio_context sensor;
int printing=1;
int portNum = 0;
int sockfd = 0;
char* hostname =NULL;
char* id = NULL;


int main( int argc, char* argv[])
{
    static struct option long_options[] = {
        {"period", required_argument, 0, 'p'},
    	{"scale", required_argument, 0, 's'},
  	    {"log", required_argument, 0, 'l'},
        {"id", required_argument, 0, 'i'},
        {"host", required_argument, 0, 'h'},
    	{0, 0, 0, 0}
	};
    char* fileName;    
    int c;
    while((c=getopt_long(argc, argv, "", long_options, NULL)) != -1)
    {

        switch(c)
        {
            case 'p':
                period = atoi(optarg);
                break;
            case 'l':
                fileName = optarg;
                logFD= creat(fileName, 0666);
                if(logFD<0)
                {
                   printExit("Could not open logfile \n") ;
                }    
                logFlag=1;
                break;

            case 's':
                if(strcmp(optarg, "C") == 0)
                    celsFlag = 1;
                else if(strcmp(optarg, "F") == 0) 
                {
                }
                else
                {
                    printExit("Invalid argument for scale. Options are F and C.\n");
                }   
         	    break;
            case 'i':
                if(strlen(optarg) != 9)
                {
                    printExit("ID should be 9 digits\n");
                }
                id = optarg;
                break;
            case 'h':
                hostname = optarg;
                break;
            default:
                fprintf(stderr, "Error: Valid arguments are period, scale, and log.. \n");
                exit(1);
               
        }

        /*if(argv[optind] == NULL)
        {
              fprintf(stderr, "No port number was given \n");
              exit(1);
	    }*/
        portNum = atoi(argv[optind]);
        if(portNum<= 0)
        {
            printExit("Failed to get port number. Please give a valid port");
        }
        if(id==NULL || hostname==NULL || logFlag==0)
        {
            printExit("Missing mandatory parameter: please use --id,--host,--log");
        }

    }

    //init socket
    struct sockaddr_in serv_addr;
    struct hostent *serverHost;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        printExit("socket");
    }

    serverHost = gethostbyname(hostname);
    if (serverHost == NULL)
    {
        printExit("Error! No such host");
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portNum);
    bcopy((char *)(serverHost -> h_addr), (char *)&serv_addr.sin_addr.s_addr, serverHost->h_length);

    int connectTest = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if(connectTest< 0)
    {
        printExit("Failed to connect socket");
    }

    //use socket
    dprintf(sockfd, "ID=%s\n", id);
    if(logFlag)
        dprintf(logFD, "ID=%s\n", id);

    sensor=mraa_aio_init(1);
    if(sensor==NULL)
        printExit("Failed to init sensor");


  
    struct timeval oldT,  newT;    

    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    int readSize = 32;
    char buf[readSize];
    char command[readSize];
    char temp; 
    int cmdSize = 0;
    int pollCheck;

    while(1)
    {
        if(gettimeofday(&newT, NULL) < 0)
            printExit("Error getting the current time\n");

        if( newT.tv_sec >= oldT.tv_sec && printing) 
        {
            struct tm*  updateTime = localtime(& newT.tv_sec);

            float temperature = readTemp(); 
           
            dprintf(sockfd, "%02d:%02d:%02d %.1f\n",  updateTime->tm_hour,  updateTime->tm_min,  updateTime->tm_sec, temperature);
            if(logFlag)
            {
                dprintf(logFD, "%02d:%02d:%02d %.1f\n",  updateTime->tm_hour,  updateTime->tm_min,  updateTime->tm_sec, temperature);
            }
            gettimeofday(&oldT, NULL);
            oldT.tv_sec+=period;
        }    

        pollCheck = poll(fds, 1,0);
        if(pollCheck<0) 
        {
            printExit("Error polling\n");
        }
        else if(pollCheck > 0) 
        {
            if(fds[0].revents & POLLIN) 
            {
                int numRead = read(sockfd, buf, readSize);
                if(numRead < 0)
                    printExit("Error reading from stdin\n");
                int i;
                for (i = 0; i < numRead; ++i)
                {
                    temp = buf[i];
                    if(temp == '\n')
                    {
                        //do the given command
                        command[cmdSize] = '\0';
                        commandFunc(command);
                        cmdSize = 0; 
                    }
                    else
                    {
                        //still processing since no newline char
                        command[cmdSize] = temp;
                        cmdSize++; 
                    }
                }
                
            }
            else if(fds[0].revents & POLLERR)
                printExit("Read error\n");
  
        }
        
    }
    mraa_aio_close(sensor);

    close(logFD);

    exit(0);  

}

float readTemp() 
{
            
    int tmpVolt = mraa_aio_read(sensor);
    float R = 1023.0/tmpVolt-1.0;
    R = 100000*R;
    //get temp in celsius
    float temperature = 1.0/(log(R/100000)/4275+1/298.15)-273.15;

    if(celsFlag==0)
        temperature = temperature * 9/5 + 32;
    return temperature;
}


void printExit(char* message)
{
    fprintf(stderr, "Error: %s \n", message);
    exit(1);
} 

void commandFunc(char* input) 
{
    if(strcmp(input, "SCALE=F") == 0) 
    {
        celsFlag = 0;
    }
    else if(strcmp(input, "SCALE=C") == 0) 
    {
        celsFlag = 1;
    }
    else if(strncmp(input, "PERIOD=", 7) == 0) 
    {
        period = atoi(input + 7);
    }
    else if(strcmp(input, "START") == 0)
    {
            printing = 1;
    }
    else if(strcmp(input, "STOP") == 0) 
    {
        printing = 0;
    }
    else if(strcmp(input, "OFF") == 0) 
    {   
        if(logFlag)
        {
           dprintf(logFD, "%s\n",input);
        }
        offFunc();
    }
    else if(strncmp(input, "LOG", 3) == 0) 
    {
    }
    else 
    {
        //fprintf(stderr, "Invalid command\n");
        //exit(1);
    }
    if(logFlag)
    {
        dprintf(logFD, "%s\n",input);
    }
}

void offFunc() 
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm = localtime(&tv.tv_sec);
    dprintf(sockfd, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
    if(logFlag)   
        dprintf(logFD, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
    exit(0);
}

