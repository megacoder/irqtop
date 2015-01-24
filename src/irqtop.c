#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>

#define	NCPU	256			/* Max CPU's we support		 */
#define	NIRQ	256			/* Max IRQ we support		 */

typedef	unsigned long long	sample_t;
#define	SFMT	"%15llu"		/* Exactly 15 characters wide	 */
#define	TFMT	"%15s"			/* Exactly 15 characters wide	 */
#define	CVT	strtoull		/* Text-to-sample converter	 */

static	char *		me = "irqtop";
static	char *		ofile;
static	char *		titles[ NCPU+1 ]; /* CPU{n} we have active	 */
static	size_t		ncpu;		/* Number of CPU's we found	 */
static	char *		irq_names[ NIRQ+1 ]; /* Spelling of IRQ names	 */
static	size_t		nirq;		/* Counts IRQ names we have	 */
static	char *		proc_interrupts = "/proc/interrupts";
static	size_t		debug_level;

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
			"; %s (errno=%d)",
			strerror( e ),
			e
		);
	}
	fprintf( stderr, ".\n" );
}

static	inline	int
on_debug(
	int		level
)
{
	return(
		(level <= debug_level)
	);
}

static	inline	void
debug(
	int		level,
	char const *	fmt,
	...
)
{
	if( on_debug( level ) )	{
		va_list		ap;

		va_start( ap, fmt );
		vfprintf(
			stderr,
			fmt,
			ap
		);
		va_end( ap );
		fprintf(
			stderr,
			".\n"
		);
	}
}

static	inline	void *
xmalloc(
	size_t	n
)
{
	void *	retval = malloc( n );
	if( !retval )	{
		log_entry(
			errno,
			"cannot allocate %d bytes",
			n
		);
		exit( 1 );
		/* NOTREACHED						 */
	}
	return( retval );
}

static	inline	char *
xstrdup(
	const char *	s
)
{
	char * const	retval = strdup( s );

	if( !retval )	{
		log_entry(
			errno,
			"string allocation failure"
		);
		exit( 1 );
	}
	return( retval );
}

static	size_t
discover_irq_setup(
	void
)
{
	size_t		retval;

	retval = 0;
	do	{
		FILE *		f;
		char		buf[ BUFSIZ + 1 ];
		char *		tokens[ 1+((BUFSIZ+1)/2) ];
		char *		token;
		char *		bp;

		f = fopen( proc_interrupts, "rt" );
		if( !f )	{
			log_entry(
				errno,
				"cannot open '%s' for reading.",
				proc_interrupts
			);
			break;
		}
		errno = 0;
		if( !fgets( buf, sizeof( buf ), f ) )	{
			log_entry(
				errno,
				"cannot read '%s'",
				proc_interrupts
			);
			break;
		}
		ncpu = 0;
		for(
			bp = buf;
			(token = strtok( bp, " \t\n" )) != NULL;
			bp = NULL
		)	{
			titles[ ncpu++ ] = xstrdup( token );
			if( ncpu >= NCPU )	{
				break;
			}
		}
		titles[ ncpu ] = NULL;
		/*							 */
		nirq = 0;
		while( fgets( buf, sizeof(buf), f ) )	{
			char * const	irq_name = strtok( buf, " \t\n:" );

			irq_names[ nirq++ ] = xstrdup( irq_name );
			if( nirq >= NIRQ )	{
				break;
			}
			/*
			 * We could skip the ncpu-count tokens and then
			 * take the rest of the line as the interrupt
			 * routing description, but maybe in another update.
			 */
			/* Ignore rest of lien				 */
		}
		irq_names[ nirq ] = NULL;
		if( fclose( f ) )	{
			log_entry(
				errno,
				"cannot close '%s'",
				proc_interrupts
			);
			break;
		}
		/* retval = FIXME */
	} while( 0 );
	return( retval );
}

static	sample_t *
take_samples(
	void
)
{
	size_t const	space_required = (nirq * ncpu * sizeof( sample_t ));
	sample_t * const	retval = xmalloc( space_required );
	do	{
		sample_t * const	space = retval;
		FILE *		f;
		char		buf[ BUFSIZ + 1 ];
		sample_t *	sp;

		memset( space, 0, space_required );
		f = fopen( proc_interrupts, "rt" );
		if( !f )	{
			log_entry(
				errno,
				"cannot open '%s' for samples",
				proc_interrupts
			);
			break;
		}
		/* The first line is the CPU<n> label line		 */
		if( !fgets( buf, BUFSIZ, f ) )	{
			log_entry(
				errno,
				"cannot read CPU titles from '%s'",
				proc_interrupts
			);
			(void) fclose( f );
			break;
		}
		/* All remaining lines are interrupt counts		 */
		sp = space;
		while( fgets( buf, BUFSIZ, f ) )	{
			char *		bp;
			size_t		cpu_no;

			/* Cheap insurance				 */
			buf[ BUFSIZ ] = '\0';
			/* Walk down line past line title's ":"		 */
			for( bp = buf; *bp && (*bp++ != ':'); );
			/* Take next 'ncpu' columns as irq counts	 */
			for( cpu_no = 0; cpu_no < ncpu; ++cpu_no )	{
				char *	eos;

				while( *bp && isspace( *bp ) )	{
					++bp;
				}
				*sp++ = CVT( bp, &bp, 10 );
			}
			/* Ignore the rest, it's just irq routing	 */
		}
		/* Clean up after ourselves				 */
		if( fclose( f ) )	{
			log_entry(
				errno,
				"failed to close sample source '%s'",
				proc_interrupts
			);
			break;
		}
	} while( 0 );
	return( retval );
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

		while( (c = getopt( argc, argv, "Do:" )) != EOF )	{
			switch( c )	{
			default:
				log_entry(
					0,
					"unknown switch '%c'",
					isalnum( c ) ? c : '_'
				);
				goto	Fini;
			case 'D':
				++debug_level;
				break;
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
			if( unlink( ofile ) )	{
				if( errno != ENOENT )	{
					log_entry(
						errno,
						"cannot unlink '%s'",
						ofile
					);
					exit( 1 );
					/*NOTREACHED*/
				}
			}
			if( freopen( ofile, "wt", stdout ) != stdout )	{
				log_entry(
					errno,
					"could not redirect to '%s'",
					ofile
				);
				break;
			}
			if( ftruncate( STDOUT_FILENO, 0 ) )	{
				log_entry(
					errno,
					"ignoring truncate '%s' failure",
					proc_interrupts
				);
			}
		}
		discover_irq_setup();
		if( on_debug( 1 ) )	{
			size_t		i;

			for( i = 0; i < ncpu; ++i )	{
				printf( "Title %d: %s\n", i, titles[ i ] );
			}
		}
		if( on_debug( 1 ) )	{
			size_t		i;

			for( i = 0; i < nirq; ++i )	{
				printf( "IRQ %3d: %s\n", i, irq_names[i] );
			}
		}
		if( on_debug( 1 ) )	{
			sample_t *	counts;
			sample_t *	cp;
			size_t		cpu;
			size_t		irq;

			counts = take_samples();
			printf( "    " );
			for( cpu = 0; cpu < ncpu; ++cpu )	{
				printf( TFMT, titles[ cpu ] );
			}
			putchar( '\n' );
			cp = counts;
			for( irq = 0; irq < nirq; ++irq )	{
				printf( "%3s:", irq_names[ irq ] );
				for( cpu = 0; cpu < ncpu; ++cpu )	{
					printf( SFMT, *cp++ );
				}
				putchar( '\n' );
			}
			/* Done						 */
			free( counts );
		}
		retval = EXIT_SUCCESS;
	} while( 0 );
	return( retval );
}
