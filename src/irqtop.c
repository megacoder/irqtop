#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

static	char *		me = "irqtop";
static	char *		ofile;

static	void
log_entry(
	int		e,
	char const *	fmt,
	...
)
{
	va_list		ap;

	fprintf( stderr, "%s: ", me );
	va_start( ap, fmt );
	vfprintf(
		stderr,
		fmt,
		ap
	);
	va_end( ap );
	if( e )	{
		fprintf(
			stderr,
			"; errno=%d (%s)",
			e,
			strerror( e )
		);
	}
	fprintf( stderr, ".\n" );
}

static	int
parse_args(
	int		argc,
	char * *	argv
)
{
	int		retval;

	opterr = 0;			/* I'll handle the error reporting */
	retval = -1;
	do	{
		int	c;

		while( (c = getopt( argc, argv, "o:" )) != EOF )	{
			switch( c )	{
			default:
				log_entry(
					0,
					"unknown switch '%c'",
					isalnum( c ) ? c : '_'
				);
				goto	Fini;
			case 'o':
				ofile = optarg;
				break;
			}
		}
		retval = 0;
	} while( 0 );
Fini:
	return( retval );
}

int
main(
	int		argc,
	char * *	argv
) {
	int		retval;

	retval = EXIT_FAILURE;
	do	{
		if( parse_args( argc, argv ) )	{
			log_entry(
				0,
				"illegal parameters; exiting"
			);
			break;
		}
		if( ofile )	{
			if( freopen( ofile, "wt", stdout ) != stdout )	{
				log_entry(
					errno,
					"could not redirect to '%s'",
					ofile
				);
				break;
			}
			(void) ftruncate( fileno( stdout ), 0 );
		}
		retval = EXIT_SUCCESS;
	} while( 0 );
	return( retval );
}
