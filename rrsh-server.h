#ifndef RRSHSERVER_H_INCLUDED
#define RRSHSERVER_H_INCLUDED

#define SHELL_MAX_ARGS 30

struct command
{
	/* Pointer to an array of pointers to strings, one per argument,
	 * with an additional NULL pointer after the last argument to
	 * mark the end of the argument list.
	 */
	char *args[SHELL_MAX_ARGS + 1];

	/* File to redirect stdin from, or NULL for no redirection. */
	char *in_redir;

	/* File to redirect stdout to, or NULL for no redirection. */
	char *out_redir;
};

/* Free all the data belonging to the command c.  Assumes the command
 * and all the strings to which it points were allocated with malloc,
 * so should only be used on commands returned by parse_command.
 */
void free_command(struct command *c);

/* Parse a command line and return a newly-allocated structure representing
 * that command.  The result should eventually be freed with free_command.
 * Calls exit(127) if there was a syntax error.
 */
struct command *parse_command(const char *cmdline);

#endif /* RRSHSERVER_H_INCLUDED */
