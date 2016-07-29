// Logan Blevins
// CS 485
// 4/26/16
// PA 5
//

#include "csapp.h"
#include "rrsh-server.h"

static char *A_RESPONSE = "Login Approved\n";
static char *F_RESPONSE = "Login Failed\n";
static char *COMPLETE = "RRSH COMMAND COMPLETED\n";

static const char *USER_FILE = "rrshusers.txt";
static const char *CMD_FILE = "rrshcommands.txt";

static const int CRED_LEN = 40;
static const int CMD_LEN = 128;
static const int FILE_LEN = 512;

char* readFile( const char *filename )
{
    int fd;
    char *buffer = Malloc( FILE_LEN );
    
    fd = Open( filename, O_RDONLY, 0 );
    Read( fd, buffer, FILE_LEN );
    Close( fd );
    
    return buffer;
}

void runProcess( char **args, int connFd )
{
    pid_t pid = fork();
    int child_status;
    int fd = 0;
    
    if ( pid == 0 )
    {
        // Setup redirection to socket
        //
        fd = Open( "/dev/null", O_RDONLY, 0 );
        Dup2( fd, STDIN_FILENO );
        Dup2( connFd, STDOUT_FILENO );
        Dup2( connFd, STDERR_FILENO );
        if ( execv( *args, args ) != 0 )
        {
            app_error( "Error executing process.\n" );
        }
    }
    else if ( pid < 0 )
    {
        app_error( "PID error.\n" );
    }
    else
    {
        // Wait for child to die
        //
        do
        {
            waitpid( pid, &child_status, WUNTRACED );
        } while ( !WIFEXITED( child_status ) && !WIFSIGNALED( child_status ) );
        
        if ( child_status != 0 )
        {
            app_error( "Child process failure.\n" );
        }
    }
}

void runCommand( char *buffer, char *user, int connFd )
{
    fprintf( stderr, "User %s sent the command '%s' to be executed.\n", user, buffer );

    struct command *cmd = parse_command( buffer );
    
    int allowed = 0;
    
    char *cmdFile = readFile( CMD_FILE );
    char *token = Malloc( CMD_LEN + 1 );
    
    while ( ( token = strsep( &cmdFile, "\n" ) ) )
    {
        // If the token is any of these, it probably means that any useful value
        // to compare is already used, and what's left is garbage.
        //
        if ( cmd->args[ 0 ] == NULL
            || strcmp( token, "" ) == 0
            || strcmp( token, " " ) == 0
            || strcmp( token, "\n" ) == 0 )
        {
            break;
        }
        if ( token && strcmp( token, cmd->args[ 0 ] ) == 0 )
        {
            // Command must be allowed
            //
            fprintf( stderr, "Executing the command '%s' on behalf of %s.\n", buffer, user );
            runProcess( cmd->args, connFd );
            allowed = 1;
            break;
        }
    }
      
    if ( !allowed )
    {
        char *first = "Cannot execute '";
        char *last = "' on this server\n";
        char *result;

        // If user enters invalid command, the parser will generate 'NULL' args, so we have to check this
        //
        if ( cmd->args[ 0 ] )
        {
            result = Malloc( strlen( first ) + strlen( cmd->args[ 0 ] ) + strlen( last ) + 1 );
            strcpy( result, first );
            strcat( result, cmd->args[ 0 ] );
        }
        else
        {
            result = Malloc( strlen( first ) + strlen( buffer ) + strlen( last ) + 1 );
            strcpy( result, first );
            strcat( result, buffer );
        }
        strcat( result, last );
        
        // Tell the client that the command was not allowed
        //
        Rio_writen( connFd, result, strlen( result ) );
        fprintf( stderr, "The command '%s' is not allowed.\n", buffer );
        if ( result ) { Free( result ); result = NULL; }
    }
    
    // Tell client the command has completed, whether allowed or not
    //
    Rio_writen( connFd, COMPLETE, strlen( COMPLETE ) );

    if ( cmd ) { free_command( cmd ); }
}

void readCommands( char *user, int connFd, rio_t *rio )
{
    size_t n;
    char *buffer = Malloc( CMD_LEN + 1 );
    
    // Be sure to sanitize the command before using it
    //
    while ( ( n = Rio_readlineb( rio, buffer, CMD_LEN + 1 ) ) != 0 )
    {
        if ( buffer[ 0 ] != '\n' )
        {
            buffer = strsep( &buffer, "\n" );
            runCommand( buffer, user, connFd );
            if ( buffer )
            {
                Free( buffer ); buffer = NULL; buffer = Malloc( CMD_LEN + 1 );
            }
        }
    }
    
    fprintf( stderr, "User %s disconnected.\n", user );
}

void authenResponse( int authenticated, int connFd, char *user, rio_t *rio )
{
    // Tell the client it's authentication status
    //
    char *response = authenticated ? A_RESPONSE : F_RESPONSE;
    Rio_writen( connFd, response, strlen( response ) );
    
    // If failure to authenticate, drop the client
    //
    if ( authenticated )
    {
        fprintf( stderr, "User %s successfully logged in.\n", user );
        readCommands( user, connFd, rio );
    }
    else
    {
        fprintf( stderr, "User %s denied access.\n", user );
    }
}

// See poster 'Joey Adams': http://codereview.stackexchange.com/questions/20897/trim-function-in-c for trimming leading and trailing spaces
// The buffer overflow noted in the comments on the site shouldn't be an issue with this function, since I'm allocating a safe and known
// size for the 'output' buffer, rather than simply using an array on the stack.
//
void remove_spaces( const char *input, char *output )
{
    int i = 0;
    int j = 0;
    for ( i = 0; input[ i ] != '\0'; i++ )
    {
        if ( !isspace( ( unsigned char )input[ i ] ) )
        {
            output[ j++ ] = input[ i ];
        }
    }
    output[ j ] = '\0';
}

void authenticate( int connFd, char *clientHostname, char *clientPort, rio_t *rio )
{
    int authenticated = 0;
    
    char *user = Malloc( CRED_LEN + 1 );
    char *pass = Malloc( CRED_LEN + 1 );
    
    Rio_readlineb( rio, user, CRED_LEN + 1 );
    Rio_readlineb( rio, pass, CRED_LEN + 1 );
    
    // Santize credentials
    //
    user = strsep( &user, "\n" );
    pass = strsep( &pass, "\n" );
    
    fprintf( stderr, "User %s logging in from %s at TCP port %s.\n", user, clientHostname, clientPort );
    
    char *userFile = readFile( USER_FILE );
    char *token;
    
    while ( ( token = strsep( &userFile, "\n" ) ) )
    {
        // If the token is any of these, it probably means that any useful value
        // to compare is already used, and what's left is garbage.
        //
        if ( strcmp( token, "" ) == 0
            || strcmp( token, " " ) == 0
            || strcmp( token, "\n" ) == 0
            || !token )
        {
            break;
        }
        char *actualUser = strsep( &token, " " );
        char *actualPass = Malloc( CRED_LEN + 1 );
        if ( token ) { remove_spaces( token, actualPass ); }
        
        if ( strcmp( user, actualUser ) == 0
            && strcmp( pass, actualPass ) == 0 )
        {
            authenticated = 1;
            break;
        }
        if ( actualPass ) { Free( actualPass ); }
    }
    
    authenResponse( authenticated, connFd, user, rio );
    
    if ( user ) { Free( user ); }
    if ( pass ) { Free( pass ); }
}

void listenOnPort( char *port, int *listenFd )
{
    *listenFd = Open_listenfd( port );
}

void acceptClient( socklen_t *clientLen, int *connFd, int *listenFd, struct sockaddr_storage *clientAddr, char *clientHostname, char *clientPort, rio_t *rio )
{
    *clientLen = sizeof( struct sockaddr_storage );
    *connFd = Accept( *listenFd, ( SA *)&clientAddr, clientLen );
    Getnameinfo( ( SA *)&clientAddr, *clientLen, clientHostname, MAXLINE, clientPort, MAXLINE, 0 );
    Rio_readinitb( rio, *connFd );
}

void server( char *port )
{
    int listenFd;
    int connFd = 0;
    
    rio_t rio;
    socklen_t clientLen;
    struct sockaddr_storage clientAddr;

    char clientHostname[ MAXLINE ];
    char clientPort[ MAXLINE ];
    
    listenOnPort( port, &listenFd );
    
    // Primary control flow: accept, authen, close
    // Keep this simple and delegate other complicated tasks to other functions
    while ( 1 )
    {
        acceptClient( &clientLen, &connFd, &listenFd, &clientAddr, clientHostname, clientPort, &rio );
        authenticate( connFd, clientHostname, clientPort, &rio );
        Close( connFd );
    }
}

int main( int argc, char **argv )
{
    if ( argc != 2 )
    {
        fprintf( stderr, "usage: %s <port>\n", argv[ 0 ] );
        exit( 0 );
    }
    
    server( argv[ 1 ] );
    exit( 0 );
}
