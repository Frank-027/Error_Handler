// -------------------------------------------------------------------------
// message_handler.c
//
// This program receives through mqtt a severity code, an error code and an optional extra parameter
// Then the corresponding error message is serached for in the correct language table
// The outgoing error message is formatted nd send out on mqtt-broker
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// F.Demonie 10/7/24 Initial release
//
// --------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <MQTTClient.h>
#include "mqtt_settings.h" // This file contains local settings

// uncomment this DEBUG if the program needs to be started up in DEBUG mode
// #define DEBUG

#define SEVCODE_FIELD   0
#define SUBSYSTEM_FIELD 1
#define ERRCODE_FIELD   2
#define PARAM_FIELD     3

#define SEVCODE_DEFAULT     "4"
#define SEV1                '1'
#define SEV4                '4'
#define SUBSYSTEM_DEFAULT   "XX-XX-XX"
#define ERRCODE_DEFAULT     "AAAXXXX"
#define ERRMSG_DEFAULT      "xx-xx-xx"

#define FILENAME        "././Error_msg_"
#define FILENAME_LEN    120
#define MAX_RECORDS     10000

#define TIMEOUT     	500L

// this mqtt token is set as global var to ease up this program
volatile MQTTClient_deliveryToken deliveredtoken;

// A struct that contains error code + corresponding error message
struct tbl {
   char error_code[ERRCODE_LEN + 1 ];
   char error_msg[ ERRMSG_LEN + 1 ];
   struct tbl *next;
};
struct tbl *head = NULL;
struct tbl *current = NULL;

// This is a function that provides an readable time stamp of te actual data/time
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

// This function is a delay function when needed
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

// This function displays the errorlist
void printList(){
   struct tbl *p = head;
   int count = 1;
   
   printf("\nError list:\n" );
   printf( "==================================\n" );

   //start from the beginning
   while(p != NULL) {
      printf("%3d\t%s\t%s\n",count, p->error_code, p->error_msg );
      p = p->next;
      count++;
      
      if ( count > MAX_RECORDS ) {
         printf( "Fatal Error: Max nr of records reached\n" );
         exit( EXIT_FAILURE );
      }   
   }
   printf("\nEnd of Error list\n");
   printf( "==================================\n" );
}

//This function insert the first struct in linked list
void insert_first(char *error_code, char *error_msg ){

   //create a link and fill with incoming data
   struct tbl *lk = (struct tbl*) malloc(sizeof(struct tbl));
   
   strcpy( lk->error_code, error_code );
   strcpy( lk->error_msg, error_msg );

   // point it to old first node
   lk->next = NULL;

   //point first to new first node
   head = lk;
}


//This function inserts a struct at the end of current linked list
void insert_next(struct tbl *list, char *error_code, char *error_msg ){
   struct tbl *lk = (struct tbl*) malloc(sizeof(struct tbl));
   
   strcpy( lk->error_code, error_code );
   strcpy( lk->error_msg, error_msg );
   
   lk->next = NULL;
   list->next = lk;
}


// This function searches in the linked list for a specific error code
int search_list( struct tbl **list, char *err_code ){
   struct tbl *temp = head;
   while(temp != NULL) {
      if ( strcmp( temp->error_code, err_code ) == 0 ) {
         *list = temp;
         return 1;
      }
      temp=temp->next;
   }
   return 0;
} 

// This function extracts the error code from input line and validate the error code
int get_err_code( char *line, char *err_code ) {
   int i = 0;
   
   while ( ( line[i] != '\t' ) && (line[i] != '\n' ) ) {
      err_code[i] = line[i];
      i++;
      if ( i > ERRCODE_LEN )
         return( 0 );
   }
   err_code[i] = '\0';
   #ifdef DEBUG
        printf( "get_err_code: err_code = <%s>\n", err_code );
   #endif
   
   if ( strlen( err_code ) != ERRCODE_LEN )
      return( 0 );
   
   return( 1 );
}   


// This function extracts the error message from input line      
int get_err_msg( char *line, char *err_msg ) {
   int i = ERRCODE_LEN + 1;
   int j = 0;
   
   while ( line[i] != '\n' ) {
      err_msg[j] = line[i];
      i++;
      j++;
      
      if ( i > ERRMSG_LEN )
         return( 0 );
   }
   err_msg[j] = '\0';
   
   #ifdef DEBUG
        printf( "get_err_msg: err_msg = <%s>\n", err_msg );
   #endif
   
   return( 1 );
}   

// This function reads the error translation file: error code + error message    
int read_errorfile( char *file_name ) {
   FILE *fp;
   int   count = 0;
   char  line[ 1024 ] = "";
   char  err_code[ ERRCODE_LEN + 1 ]   = "";
   char  err_msg[ ERRMSG_LEN + 1 ]     = "";
   
   //create a first struct in the linked list
   struct tbl *lk = (struct tbl*) malloc(sizeof(struct tbl));
   head = lk;
   head->next = NULL;
   
   // open the input file
   fp = fopen( file_name, "r" );
   if ( fp == NULL ) {
      printf( "Can't open Error Msg file %s÷\n", file_name );
      return( 0 );
   }
   
   // read line by line and extrapolate error code and error message
   // and put these in the linked list
   count = 0;
   while (fgets( line, sizeof( line ), fp ) != NULL ) {
       #ifdef DEBUG
            printf( "read_errorfile: START\tline %3d = <%s>\n", count, line );
       #endif     
       if( line[0] != '#' ) { // Exclude comments line beginning with #
           count++;
           if ( get_err_code( line, err_code ) == 0 ) {
               printf( "Wrong error code on line %d in file %s\n", count, file_name );
               return( 0 );
            }
         
            if ( get_err_msg( line, err_msg ) == 0 ) {
                printf( "Wrong error message on line %d in file %s\n", count, file_name );
                return( 0 );
            }
      
            if ( count == 1 ) {
                insert_first( err_code, err_msg );
                current = head;
            }   
            else {
                insert_next( current, err_code, err_msg );
                current = current->next;
            }
            
            #ifdef DEBUG
                printf( "read_errorfile: END\tline %3d\terr_code = <%s>\terr_msg = <%s>\n\n", count, err_code, err_msg );
            #endif    
       }        
   }   
   fclose( fp );  
   return( 1 );
  }  
  
  // this function checks if there is a parameter to be included in the error message
  int contains_param( char *str ) {
	int i = 0;
	
	while ( str[i] != '\0' ) {
		if ( ( str[i] == '%' ) && ( str[ i+1] == 's' ) )
			return( 1 );
		i++;
	}
	
	return( 0 );
}

// This function extracts the error fields from input line and validates them
int obtain_err_fields( char *line, char err_field[][ERRMSG_LEN ], int nr_fields ) {
   int i, j, field_nr;
   
   #ifdef DEBUG
        printf( "obtain_err_fields: line = <%s>\n", line );
   #endif     
   for( i = 0 ; i < nr_fields ; i++ ) {
        strcpy( err_field[i], "" );
    }    
   
   // Obtain error fields and put into array
   i = 0;
   j = 0;
   field_nr = 0;
   while ( ( line[i] != '\0' ) && ( line[i] != '\n' ) && ( field_nr < nr_fields ) ){
       if ( line[i] == ';' ) {
           err_field[field_nr][j] = '\0';
           j = 0;
           field_nr++;
       } 
       else {
           err_field[field_nr][j] = line[i];
           j++;
       } 
       i++;
   }
   err_field[field_nr][j] = '\0';
   
   #ifdef DEBUG
   for( i = 0 ; i < nr_fields ; i++ )
        printf( "Obtain_error_fields: before validation\tField %d = <%s>\n", i, err_field[i] );
   #endif
   
   // Test severity code and if invalid set to default = SEV 4
   if ( ( err_field[SEVCODE_FIELD][0] < SEV1 ) || ( err_field[SEVCODE_FIELD][0] > SEV4 ) )
		strcpy( err_field[SEVCODE_FIELD],  SEVCODE_DEFAULT ); // if Severity code is incorrect, set severity code to default
		
	// Test Subsystem LEN and cut of if needed
	if ( strlen( err_field[SUBSYSTEM_FIELD] ) > SUBSYSTEM_LEN )
			err_field[SUBSYSTEM_FIELD][SUBSYSTEM_LEN] = '\0';
	// If no subsystem name is given, then set to default name
	if ( err_field[SUBSYSTEM_FIELD][0] == '\0' )
			strcpy( err_field[SUBSYSTEM_FIELD], SUBSYSTEM_DEFAULT ); 	
			
	// Test ERR_CODE and cut of if needed
	if ( strlen( err_field[ERRCODE_FIELD] ) != ERRCODE_LEN )
			strcpy( err_field[ERRCODE_FIELD], ERRCODE_DEFAULT ); 			
			
	// Test ERR_PARAM and cut of if needed
	if ( strlen( err_field[PARAM_FIELD] ) > ERRPARAM_LEN )
			err_field[PARAM_FIELD][ERRPARAM_LEN] = '\0';		
   
   #ifdef DEBUG
   for( i = 0 ; i < nr_fields ; i++ )
        printf( "Obtain_error_fields: after validation\tField %d = <%s>\n", i, err_field[i] );
   #endif
   
   
   return( 1 );
}  

// This function formats the outgoing error message    
int format_error_msg( char *line, char *error_out ) {
    char timestamp[ TIMESTR_LEN ]           = "";
    char err_field[][ ERRMSG_LEN ] = {"", "", "", "" };
	int  nr_err_fields = sizeof(err_field) / sizeof(err_field[0]);
    
    char err_sev_code[ SEVCODE_LEN + 1           ]    = "";
    char err_msg[      ERRMSG_LEN                ]    = "";
    char err_tmp_msg[  ERRMSG_LEN + ERRPARAM_LEN ]    = "";
    int  sevc = 0;
    
    strcpy( error_out, "" );
    #ifdef DEBUG
        printf( "format_error_msg: START\tline = <%s>\terror_out = <%s>\n", line, error_out );
    #endif
    
    obtain_err_fields( line, err_field, nr_err_fields );
    format_time( timestamp );
        
    // set Sev Code as char value
    sevc = atoi( err_field[SEVCODE_FIELD] );
    
    if ( ( sevc >= 1 ) && ( sevc <= 4 ) ) {
        sprintf( err_sev_code, "SEV %1d", sevc );
    }    
    else {
        strcpy( err_sev_code, "SEV 4" ); // Default SEV 4
    }    
     
    // search for specific error code and store in error message
    current = head;
    if ( search_list( &current, err_field[ERRCODE_FIELD] ) == 0 )
        strcpy( err_msg, ERRMSG_DEFAULT ); // fill in a default error msg
    else   
        strcpy( err_msg, current->error_msg ); 
     
    // Format err_code + err_ms + err_param
    if ( contains_param( err_msg ) ) {
         sprintf( err_tmp_msg, err_msg, err_field[PARAM_FIELD] );
    }
    else {
         sprintf( err_tmp_msg, "%s", err_msg );
    }   
     
    // format message
    sprintf( error_out, "%s;%s;%s;%s;%s", timestamp, err_sev_code, err_field[SUBSYSTEM_FIELD], err_field[ERRCODE_FIELD], err_tmp_msg );
    
    #ifdef DEBUG
        printf( "format_error_msg: End line: <%s>\terror_out: <%s>\n", line, error_out ); 
    #endif
        
    return( 1 );
}   

// This function is called upon when a message is successfully delivered through mqtt
void delivered(void *context, MQTTClient_deliveryToken dt) {
    #ifdef DEBUG
        printf("Message with token value %d delivery confirmed\n", dt);
        printf( "-----------------------------------------------\n" );
    #endif    
    deliveredtoken = dt;
}

// This function is called upon when an incoming message from mqtt is arrived
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char *error_in = message->payload;
    char  error_out[ ERR_OUT_LEN ] = "";
    
    printf( "Msg in : <%s>\n", error_in );
    
    #ifdef DEBUG
        printf( "msgarrvd: error_in: <%s>\n", error_in );
    #endif    
    
    format_error_msg( error_in, error_out );
    
    #ifdef DEBUG
        printf( "msgarrvd: error_out: <%s>\n", error_out );
    #endif    

    // Create a new client to publish the message
    MQTTClient client = (MQTTClient)context;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload = error_out;
    pubmsg.payloadlen = strlen( error_out );
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_publishMessage(client, PUB_TOPIC, &pubmsg, &token);
    #ifdef DEBUG
        printf("Publishing to topic %s\n", PUB_TOPIC);
    #endif
    
    #ifdef DEBUG
        int rc = MQTTClient_waitForCompletion(client, token, TIMEOUT );
        printf("Message with delivery token %d delivered, rc=%d\n", token, rc);
    #endif
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    
    printf( "Msg out:\t<%s>\n", error_out );
    
    return 1;
}

// This function is called upon when the connection to the mqtt-broker is lost
void connlost(void *context, char *cause) {
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

// This is the main function:
// A infenite loop doing:
// (1) using the startup parameter to set the language file name to be read
// (2) read the error file and put the error code & error message in a linked list
// (3) Open MQTT Client for listening
//     Set MQTT msgarrvd function => This function is called upon when a message arrives
//     In this function, read the incoming message, format the outgoing message and send it out
// 
int main(int argc, char* argv[]) {
    char filename[ FILENAME_LEN ] = "";
    
    // Set Error Table to specified language
    if ( argc == 1 )
         sprintf( filename, "%sENE.txt", FILENAME ); // default language
    else
         sprintf( filename, "%s%s.txt", FILENAME, argv[1] );
   
   // Read Error Table and put in linked list
   if( read_errorfile( filename ) == 0 ) {
      printf( "Fatal error\n" );
      exit( EXIT_FAILURE );
   }   
   
   // print list
   #ifdef DEBUG
        printList();  
   #endif
   
   // Open MQTT client for listening
    
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    MQTTClient_setCallbacks(client, client, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    #ifdef DEBUG
        printf("Subscribing to topic %s for client %s using QoS%d\n\n", SUB_TOPIC, CLIENTID, QOS);
    #endif
    MQTTClient_subscribe(client, SUB_TOPIC, QOS);
    

    // Keep the program running to continue receiving and publishing messages
    for(;;) {
        delay(1);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;
}
