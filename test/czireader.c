/*
 * ZEISS (czi) zisraw support
 * Yaël
 *
 * Let's use our czi/zisraw functions !
 */

//--- openslide --------------------------------------------------------------
#include <openslide-decode-zisraw.h>
#include <openslide-private.h>
//--- std --------------------------------------------------------------------
#include <stdio.h>                    // FILE
#include <string.h>
#include <stdlib.h>                   // alloc/free (TODO replace with glib)
#include <limits.h>                   // fmin/fmax (TODO change)
#include <glib.h>                     // GError
//--- preprocessing stuff ----------------------------------------------------
#define IS_BIG_ENDIAN (*(uint16_t *)"\0\xff" < 0x100)
//----------------------------------------------------------------------------

int main( int argc, const char **argv )
{
  g_debug( "Is big endian: %i", (int)IS_BIG_ENDIAN );
  GError * err = NULL;

  //--------------------------------------------------------------------------
  // variables
  //--------------------------------------------------------------------------
  char * filename             = NULL;
  char * xmlout               = NULL;
  int    listsegments         = 0;
  int    readsegments         = 0;
  int    readfileheader       = 0;
  int    readdirectory        = 0;
  int    readmetadata         = 0;
  int    computedimensions    = 0;
  int    printfileheader      = 0;
  int    printdirectory       = 0;
  int    printdimensions      = 0;
  int    printmetadata        = 0;
  int    maxblocks            = -1;

  //--------------------------------------------------------------------------
  // read options
  //--------------------------------------------------------------------------
  for( int i=0; i < argc; ++i )
  {
    if( !strcmp( argv[i], "-i" ) )
    {
      ++i;
      filename = malloc( strlen(argv[i]) );
      strcpy( filename, argv[i] );
    }
    else if( !strcmp( argv[i], "--listsegments" ) )
    {
      listsegments = 1;
    }
    else if( !strcmp( argv[i], "--readsegments" ) )
    {
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--readfileheader" ) )
    {
      readfileheader = 1;
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--readdirectory" ) )
    {
      readdirectory = 1;
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--readmetadata" ) )
    {
      readmetadata = 1;
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--computedimensions" ) )
    {
      computedimensions = 1;
      readdirectory = 1;
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--printfileheader" ) )
    {
      printfileheader = 1;
      readfileheader = 1;
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--printdirectory" ) )
    {
      printdirectory = 1;
      readdirectory = 1;
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--printdimensions" ) )
    {
      printdimensions = 1;
      computedimensions = 1;
      readdirectory = 1;
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--printmetadata" ) )
    {
      ++i;
      xmlout = malloc( strlen(argv[i]) );
      strcpy( xmlout, argv[i] );
      printmetadata = 1;
      readmetadata = 1;
      readsegments = 1;
    }
    else if( !strcmp( argv[i], "--maxblocks" ) )
    {
      ++i;
      maxblocks = atoi( argv[i] );
    }
  }

  //--------------------------------------------------------------------------
  // check options
  //--------------------------------------------------------------------------
  if( !filename ) {
    g_warning( "Missing -i option." );
    return EXIT_FAILURE;
  }

  //--------------------------------------------------------------------------
  // list segments
  //--------------------------------------------------------------------------
  if( listsegments )
  {
    FILE * stream = _openslide_fopen( filename, "rb", &err );
    if( stream == NULL || err != NULL )
    {
      if( err != NULL ) {
        gchar * msg = g_strdup( err->message );
        g_warning( "%s", msg );
        g_error_free( err );
        g_free( msg );
      }
      return EXIT_FAILURE;
    }
    czi_segment_header tmpsh;
    while( !feof( stream ) )
    {
      if( !readNextSegmentHeader( stream, &tmpsh, &err ) ) {
        if( err != NULL ) {
          gchar * msg = g_strdup( err->message );
          g_warning( "%s", msg );
          g_error_free( err );
          g_free( msg );
          err = NULL;
        }
        break;
      }
      else
      {
        printf( "%s\n", tmpsh.id );
        if( !skipSegment( stream, &tmpsh, &err ) )
        {
          if( err != NULL ) {
            gchar * msg = g_strdup( err->message );
            g_warning( "%s", msg );
            g_error_free( err );
            g_free( msg );
            err = NULL;
          }
          return EXIT_FAILURE;
        }
      }
    }
    fclose( stream );
  }

  //--------------------------------------------------------------------------
  // check infos
  //-------------------------------------------------------------------------
  if( readsegments )
  {
    FILE * stream = _openslide_fopen( filename, "rb", &err );
    if( stream == NULL || err != NULL )
    {
      if( err != NULL ) {
        gchar * msg = g_strdup( err->message );
        g_warning( "%s", msg );
        g_error_free( err );
        g_free( msg );
        err = NULL;
      }
      return EXIT_FAILURE;
    }

    czi_segment_header             *Temporary      = NULL;
    czi_segment_header             *SegHeadFile    = NULL;
    czi_segment_header             *SegHeadDir     = NULL;
    czi_segment_header             *SegHeadMeta    = NULL;
    czi_segment_header             *SegHeadAttDir  = NULL;
    czi_file_header                *SegFileHeader  = NULL;
    czi_subblock_directory_segment *SegDirectory   = NULL;
    czi_metadata_segment           *SegMetadata    = NULL;
    while( !feof( stream ) )
    {
      if( !Temporary )
        Temporary = (czi_segment_header*) g_malloc0( sizeof(czi_segment_header) );
      if( !readNextSegmentHeader( stream, Temporary, &err ) )
      {
        if( err != NULL ) {
          gchar * msg = g_strdup( err->message );
          g_warning( "%s", msg );
          g_error_free( err );
          g_free( msg );
          err = NULL;
        }
        freeSegmentHeader( Temporary );
        Temporary = NULL;
        fseeko( stream, 0, SEEK_END );
        break;
      }
      if( !strcmp( Temporary->id, CZI_SUBBLOCK ) )
      {
        if( !skipSegment( stream, Temporary, &err ) )
        {
          if( err != NULL ) {
            gchar * msg = g_strdup( err->message );
            g_warning( "%s", msg );
            g_error_free( err );
            g_free( msg );
            err = NULL;
          }
          return EXIT_FAILURE;
        }
      }
      else if( !strcmp( Temporary->id, CZI_FILE ) )
      {
        if( readfileheader )
        {
          SegHeadFile = Temporary;
          Temporary = NULL;
          SegFileHeader = (czi_file_header*) g_malloc0( sizeof(czi_file_header) );
          if( !readFileHeader( stream, SegFileHeader, &err ) )
          {
            if( err != NULL )
            {
              gchar * msg = g_strdup( err->message );
              g_warning( "%s", msg );
              g_error_free( err );
              g_free( msg );
              err = NULL;
            }
            return EXIT_FAILURE;
          }
          if( printfileheader )
            printFileHeader( SegFileHeader );
        } else {
          if( !skipSegment( stream, Temporary, &err ) )
          {
            if( err != NULL ) {
              gchar * msg = g_strdup( err->message );
              g_warning( "%s", msg );
              g_error_free( err );
              g_free( msg );
              err = NULL;
            }
            return EXIT_FAILURE;
          }
        }
      }
      else if( !strcmp( Temporary->id, CZI_DIRECTORY ) )
      {
        if( readdirectory )
        {
          SegHeadDir = Temporary;
          Temporary = NULL;
          SegDirectory = (czi_subblock_directory_segment*) g_malloc0( sizeof(czi_subblock_directory_segment) );
          if( !readSubBlockDirectorySegment( stream, SegDirectory, &err ) )
          {
            if( err != NULL )
            {
              gchar * msg = g_strdup( err->message );
              g_warning( "%s", msg );
              g_error_free( err );
              g_free( msg );
              err = NULL;
            }
            return EXIT_FAILURE;
          }
          if( printdirectory )
            printSubBlockDirectorySegment( SegDirectory, maxblocks );
        } else {
          if( !skipSegment( stream, Temporary, &err ) )
          {
            if( err != NULL ) {
              gchar * msg = g_strdup( err->message );
              g_warning( "%s", msg );
              g_error_free( err );
              g_free( msg );
              err = NULL;
            }
            return EXIT_FAILURE;
          }
        }
      }
      else if( !strcmp( Temporary->id, CZI_METADATA ) )
      {
        if( readmetadata )
        {
          SegHeadMeta = Temporary;
          Temporary = NULL;
          SegMetadata = (czi_metadata_segment*) g_malloc0( sizeof(czi_metadata_segment) );
          if( !readMetadataSegment( stream, SegMetadata, &err ) )
          {
            if( err != NULL )
            {
              gchar * msg = g_strdup( err->message );
              g_warning( "%s", msg );
              g_error_free( err );
              g_free( msg );
              err = NULL;
            }
            return EXIT_FAILURE;
          }
          if( printmetadata )
          {
            FILE * xmlstream = _openslide_fopen( xmlout, "w", NULL );
            fwrite( SegMetadata->xml_buf, sizeof(char), SegMetadata->xml_size, xmlstream );
            fclose( xmlstream );
          }
        }
        else
        {
          if( !skipSegment( stream, Temporary, &err ) )
          {
            if( err != NULL ) {
              gchar * msg = g_strdup( err->message );
              g_warning( "%s", msg );
              g_error_free( err );
              g_free( msg );
              err = NULL;
            }
            return EXIT_FAILURE;
          }
        }
      }
      else if( !strcmp( Temporary->id, CZI_ATTDIR ) )
      {
        if( !skipSegment( stream, Temporary, &err ) )
        {
          if( err != NULL ) {
            gchar * msg = g_strdup( err->message );
            g_warning( "%s", msg );
            g_error_free( err );
            g_free( msg );
            err = NULL;
          }
          return EXIT_FAILURE;
        }
      }
      else if( !strcmp( Temporary->id, CZI_ATTACH ) )
      {
        if( !skipSegment( stream, Temporary, &err ) )
        {
          if( err != NULL ) {
            gchar * msg = g_strdup( err->message );
            g_warning( "%s", msg );
            g_error_free( err );
            g_free( msg );
            err = NULL;
          }
          return EXIT_FAILURE;
        }
      }
      else if( !strcmp( Temporary->id, CZI_DELETED ) )
      {
        if( !skipSegment( stream, Temporary, &err ) )
        {
          if( err != NULL ) {
            gchar * msg = g_strdup( err->message );
            g_warning( "%s", msg );
            g_error_free( err );
            g_free( msg );
            err = NULL;
          }
          return EXIT_FAILURE;
        }
      }
      else
      {
        g_warning( "!! Unknown id %s. There may have been a failure somewhere. Continue.", Temporary->id );
      }
    }
    fclose( stream );

    if( computedimensions )
    {
      czi_list_of_image_descriptor * Descriptor = (czi_list_of_image_descriptor*) g_malloc0( sizeof(czi_list_of_image_descriptor) );
      computeDimensions( SegDirectory, Descriptor, maxblocks );
      if( printdimensions )
        printPyramids( Descriptor );
      freeListOf_ImageDescriptor( Descriptor );
      Descriptor = NULL;
    }

    freeSegmentHeader( SegHeadFile );
    SegHeadFile = NULL;
    freeSegmentHeader( SegHeadDir );
    SegHeadDir = NULL;
    freeSegmentHeader( SegHeadMeta );
    SegHeadMeta = NULL;
    freeSegmentHeader( SegHeadAttDir );
    SegHeadAttDir = NULL;
    freeSegmentHeader( Temporary );
    Temporary = NULL;
    freeFileHeader( SegFileHeader );
    SegFileHeader = NULL;
    freeSubBlockDirectorySegment( SegDirectory );
    SegDirectory = NULL;
    freeMetadataSegment( SegMetadata );
    SegMetadata = NULL;
  }

  return EXIT_SUCCESS;
}