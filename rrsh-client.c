// Logan Blevins
// CS 485
// 4/26/16
// PA5
//

#include <string.h>

#include "csapp.h"

static const int CRED_LEN = 40;
static const int CMD_LEN = 128;
static char *A_RESPONSE = "Login Approved\n";
static const char *COMPLETE = "RRSH COMMAND COMPLETED\n";

void getCreds( char *user, char *pass )
{
    // Get username and password, allowing an extra byte for newline
    //
    fprintf( stderr, "Username: " );
    Fgets( user, CRED_LEN + 1, stdin );
    fprintf( stderr, "Password: " );
    Fgets( pass, CRED_LEN + 1, stdin );
}

void openConnection( char *host, char *port, int *clientFd, rio_t *rio )
{
    *clientFd = Open_clientfd( host, port );
    Rio_readinitb( rio, *clientFd );
}

void readCommands( int clientFd, rio_t *rio )
{
    // Normal disconnects and terminations if: 'EOF' or 'quit'
    //
    char buffer[ CMD_LEN + 1 ];
    fprintf( stderr, "rrsh> " );
    while ( Fgets( buffer, CMD_LEN + 1, stdin ) != NULL && strcmp( buffer, "quit\n" ) != 0 )
    {
        if ( *buffer == '\n' )
        {
            fprintf( stderr, "rrsh> " );
            continue;
        }
        // Send command to server, wait to reply
        //
        Rio_writen( clientFd, buffer, strlen( buffer ) );
        
        // Read command output, if there was any
        //
        size_t n;
        while ( ( n = Rio_readlineb( rio, buffer, MAXLINE ) ) )
        {
            // Stop reading and printing command output once completion response is received
            //
            if ( strcmp( buffer, COMPLETE ) == 0 ) { break; }
            fprintf( stderr, "%s", buffer );
        }
        fprintf( stderr, "\nrrsh> " );
    }
    
    Close( clientFd );
}

void authenticate( char *user, char *pass, int *clientFd, rio_t *rio )
{
    if ( !user || !pass )
    {
        app_error( "Authentication error\n" );
    }
    
    // Send username and password to server
    //
    Rio_writen( *clientFd, user, strlen( user ) );
    Rio_writen( *clientFd, pass, strlen( pass ) );
    
    // Wait for server authentication response
    //
    char buffer[ MAXLINE ];
    Rio_readlineb( rio, buffer, MAXLINE );
    fprintf( stderr, "%s", buffer );
    
    // Immediately disconnect and terminate client if login failed
    //
    if ( strcmp( buffer, A_RESPONSE ) != 0 )
    {
        Close( *clientFd );
        exit( 0 );
    }
    
    // Must have been successful login
    //
    readCommands( *clientFd, rio );
}

void client( char *host, char *port )
{
    rio_t rio;
    
    int clientFd;
    
    // Add on extra byte for to account for NULL terminator
    //
    char user[ CRED_LEN + 1 ];
    char pass[ CRED_LEN + 1 ];

    // Order matters, 'authenticate()' REQUIRES 'getCreds()'
    //
    getCreds( user, pass );
    openConnection( host, port, &clientFd, &rio );
    authenticate( user, pass, &clientFd, &rio );
}

int main( int argc, char **argv )
{
    // Check or invalid usage
    //
    if ( argc != 3 )
    {
        fprintf( stderr, "usage: %s <host> <port>\n", argv[ 0 ] );
        exit( 0 );
    }
    
    client( argv[ 1 ], argv[ 2 ] );
    exit( 0 );
}
