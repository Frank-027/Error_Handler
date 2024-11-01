#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "MQTTClient.h"
#include "mqtt_settings.h"

// #define DEBUG

#define TOPIC       SUB_TOPIC
#define TIMEOUT     10000L

#define LANG_LEN            5
#define FILENAME_LEN        120
#define FILENAME            "./Error_input1_%s.txt"
#define MAX_RECORDS         10000

#define DELAY_TIME          1
#define MAX_TRANSMISSIONS   5

struct node {
   char sev_code[ SEVCODE_LEN ];
   char error_code[ERRCODE_LEN + 1 ];
   char error_param[ ERRPARAM_LEN + 1 ];
   struct node *next;
};
struct node *head = NULL;
struct node *current = NULL;

void delay(int number_of_seconds)
{
    double start_time, end_time, act_time;

	// Storing start & end time
	start_time = ( double) clock() / CLOCKS_PER_SEC;
    end_time = start_time + number_of_seconds;

	// looping till required time is not achieved
    act_time = (double) clock() / CLOCKS_PER_SEC;
	while (act_time < end_time )
		act_time = (double) clock() / CLOCKS_PER_SEC;
}

int format_time(char *timestamp ) {
    time_t t ;
    struct tm *tmp ;
    
    time( &t );
    tmp = localtime( &t );
     
    //Format my_time string
    sprintf( timestamp, "%02d-%02d-%02d %02d:%02d:%02d", 
        tmp->tm_year-100, tmp->tm_mon+1, tmp->tm_mday, // YEAR-MONTH_DAY
        tmp->tm_hour, tmp->tm_min, tmp->tm_sec ); // HOUR:MIN:SEC
     
    return( EXIT_SUCCESS );
}

// display the list
void printList(){
   struct node *p = head;
   int count = 1;
   
   printf("\nError list:\n" );
   printf( "==================================\n" );

   //start from the beginning
   while(p != NULL) {
      printf("%3d\t<%s>\t<%s>\t<%s>\n",count, p->sev_code, p->error_code, p->error_param );
      p = p->next;
      count++;
      
      if ( count > MAX_RECORDS ) {
         printf( "Fatal Error: Max nr of records reached\n" );
         exit( EXIT_FAILURE );
      }   
   }
   printf("\nEnd of Error list\n");
   printf( "==================================\n\n\n" );
   
}

//insert the first struct in linked list
void insert_first(char *sev_code, char *error_code, char *error_param ){

   //create a link and fill with incoming data
   struct node *lk = (struct node*) malloc(sizeof(struct node));
   
   strcpy( lk->sev_code, sev_code );
   strcpy( lk->error_code, error_code );
   strcpy( lk->error_param, error_param );

   // point it to old first node
   lk->next = NULL;

   //point first to new first node
   head = lk;
}


//insert struct at the end of current linked list
void insert_next(struct node *list, char *sev_code, char *error_code, char *error_param ){
   struct node *lk = (struct node*) malloc(sizeof(struct node));
   
   strcpy( lk->sev_code, sev_code );
   strcpy( lk->error_code, error_code );
   strcpy( lk->error_param, error_param );
   
   lk->next = NULL;
   list->next = lk;
}

// get the sev code from input line and validate the sev code
int get_sev_code( char *line, char *sev_code ) {
   int i = 0;
   
   while ( ( line[i] != '\t' ) && (line[i] != '\n' ) ) {
      sev_code[i] = line[i];
      i++;
      if ( i > SEVCODE_LEN )
         return( 0 );
   }
   sev_code[i] = '\0';
   
   #ifdef DEBUG
        printf( "SEV Code = <%s> - %d\n", sev_code, strlen( sev_code) );
   #endif
   
   if ( strlen( sev_code ) != 1 )
      return( 0 );
   
   return( 1 );
}   

// get the error code from input line      
int get_err_code( char *line, char *err_code ) {
   int i = 1 + 1;
   int j = 0;
   
   while ( ( line[i] != '\t' ) && ( line[i] != '\n' ) ) {
      err_code[j] = line[i];
      
      if ( j > ERRCODE_LEN )
         return( 0 );
      
      i++;
      j++;
   }
   err_code[j] = '\0';
   
   #ifdef DEBUG
        printf( "Err Code = <%s> - %d\n", err_code, strlen( err_code) );
   #endif
   
   return( 1 );
}   

// get the error param from input line      
int get_err_param( char *line, char *err_param ) {
   int i = ERRCODE_LEN + 3;
   int j = 0;
   
   while ( line[i] != '\n' ) {
      err_param[j] = line[i];
      
      if ( j > ERRPARAM_LEN )
         return( 0 );
      i++;
      j++;
   }
   err_param[j] = '\0';
   
   #ifdef DEBUG
        printf( "Err Param = <%s> - %d\n", err_param, strlen( err_param) );
   #endif
   
   return( 1 );
}   
    
int read_errorfile( char *file_name ) {
   FILE *fp;
   int   count = 0;
   char  line[ sizeof( struct node ) ] = "";
   char  sev_code[ SEVCODE_LEN + 1 ]   = "";
   char  err_code[ ERRCODE_LEN + 1 ]   = "";
   char  err_param[ ERRPARAM_LEN + 1 ] = "";
   
   //create a first struct in the linked list
   struct node *lk = (struct node*) malloc(sizeof(struct node));
   head = lk;
   head->next = NULL;
   
   // open the input file
   fp = fopen( file_name, "r" );
   if ( fp == NULL ) {
      printf( "Can't open Error Msg file %s÷\n", file_name );
      return( 0 );
   }
   
   // read line by line and extrapolate sev code, error code and error param
   // and put these in the linked list
   while (fgets( line, sizeof( line ), fp ) != NULL ) {
       if ( line[0] != '#' ) { // Exclude comment lines
            count++;
            if ( get_sev_code( line, sev_code ) == 0 ) {
                printf( "Wrong sev code on line %d in file %s\n", count, file_name );
                return( 0 );
            }
      
            if ( get_err_code( line, err_code ) == 0 ) {
                printf( "Wrong error code on line %d in file %s\n", count, file_name );
                return( 0 );
            }
      
            if ( get_err_param( line, err_param ) == 0 ) {
                printf( "Wrong error message on line %d in file %s\n", count, file_name );
                return( 0 );
            }
      
            if ( count == 1 ) {
                insert_first( sev_code, err_code, err_param );
                current = head;
            }   
            else {
                insert_next( current, sev_code, err_code, err_param );
                current = current->next;
            }    
      }   
   }   
      
   fclose( fp );  
   
   return( 1 );
  }  
			   

int format_error_msg( int sev_code, char *subsystem, char *err_code, char *err_param, char *err_msg_out ) {
    
    // Initialize the output msg
    strcpy( err_msg_out, "" );
    
    // Validate Sev Code. If incorrect, set to SEV 4
    if ( ( sev_code < 1 ) || ( sev_code > 4 ) )
      sev_code = 4;
      
    // validate Subsystem; if invalid, set to default
    if ( ( subsystem[0] == '\0' ) || ( strlen( subsystem ) > SUBSYSTEM_LEN ) )
        strcpy( subsystem, "XXXXXXXX" );
    
    // Validate error code
    if ( ( err_code[0] == '\0' ) || ( strlen( err_code ) > ERRCODE_LEN ) )
        strcpy( err_code, "ABCXXXX" );
     
    // format message and send to Error Topic on MQTT
    sprintf( err_msg_out, "%d;%s;%s;%s", sev_code, subsystem, err_code, err_param );
    
    return( EXIT_SUCCESS );
}   

int main(int argc, char* argv[]) {
    char filename[ FILENAME_LEN ] = "";
    struct node *p;
    char err_out_msg[ ERR_OUT_LEN ] = "";
    int count;
    int transmissions;
    
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    
    
    if ( argc == 1 )
         sprintf( filename, FILENAME, "EN" ); // default language
    else
         sprintf( filename, FILENAME, argv[1] );
   
    if( read_errorfile( filename ) == 0 ) {
      printf( "Fatal error\n" );
      exit( EXIT_FAILURE );
    }   
   
    #ifdef DEBUG
        // print list
        printList();
    #endif    

    // Create an MQTT client
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    
    // Set connection options
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Connect to the MQTT broker
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    
    // Prepare the message to publish
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    
    for( transmissions = 1 ; transmissions <= MAX_TRANSMISSIONS ; transmissions++ ) { 
        printf( "\nTransmission # %3d ===============================================\n", transmissions );
        count = 1;
        p = head;
        while ( p != NULL ) {
        
            format_error_msg( atoi( p->sev_code ), argv[0], p->error_code, p->error_param, err_out_msg );
            
            pubmsg.payload = err_out_msg;
            pubmsg.payloadlen = strlen(err_out_msg);
            #ifdef DEBUG
                printf( "MAIN: <%p>\t<%p>\t<%s>\t%5d\n", pubmsg.payload, err_out_msg, err_out_msg, pubmsg.payloadlen );
            #endif    
        
            // Publish the message
            MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
            
            #ifdef DEBUG
                printf("Waiting for up to %d seconds for publication of <%s>\n"
                "on topic %s for client with ClientID: %s\n",
                (int)(TIMEOUT/1000), err_out_msg, TOPIC, CLIENTID);
            #endif
            
            // Wait for the message to be delivered
            rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
            #ifdef DEBUG
                printf("Message %d with delivery token %d delivered\n\n", count, token);
            #endif
            
            printf( "Message %3d => <%s>\n", count, err_out_msg );
            delay( DELAY_TIME );
            
            // go for next msg
            count++;
            p = p->next;
        }
    }
    
    // Disconnect from the broker
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return rc;
}
