/*****************************************************************************
 * \file create_index
 * AUTHOR: Chris Park
 * CREATE DATE: August 10 2006
 * DESCRIPTION: Given a protein fasta sequence database as input, generate crux_index files
 *              that contain list of peptides in 
 *              the database that meet certain criteria (e.g. mass, length, trypticity) as output.
 * REVISION: 
 ****************************************************************************/
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include "carp.h"
#include "peptide.h"
#include "peptide_src.h"
#include "protein.h"
#include "database.h"
#include "index.h"
#include "protein_index.h"
#include "parameter.h"

#define NUM_INDEX_OPTIONS 10
#define NUM_INDEX_ARGS 1

      
int main(int argc, char** argv){

  /* Declarations */
  int verbosity;
  int min_length;
  int max_length;
  double min_mass;
  double max_mass;
  MASS_TYPE_T mass_type;
  PEPTIDE_TYPE_T peptide_type;
  int missed_cleavages; 
  //where is the unique vs redundant variable?

  double mass_range;
  PEPTIDE_CONSTRAINT_T* constraint;
  char* in_file = NULL;
  INDEX_T* crux_index;
  char* binary_fasta_file = NULL;

  /* Define optional command line arguments */ 
  int num_options = NUM_INDEX_OPTIONS;
  char* option_list[NUM_INDEX_OPTIONS] = { 
    "verbosity",
    "parameter-file", 
    "min-length", 
    "max-length", 
    "min-mass", 
    "max-mass", 
    "isotopic-mass",
    "cleavages", 
    "missed-cleavages",
    "unique-peptides"
  };

  /* Define required command line arguments */ 
  // TODO add index name
  int num_arguments = NUM_INDEX_ARGS;
  char* argument_list[NUM_INDEX_ARGS] = { "protein fasta file" };


  /* For debugging of parameter processing */
  // TODO make this dependant on a compile flag 
  //set_verbosity_level(CARP_DETAILED_DEBUG);  
  carp(CARP_DETAILED_DEBUG, "Starting create_index");

  /* connect various signals to our clean-up function */
  signal( SIGTERM, clean_up );
  signal( SIGINT, clean_up );
  signal( SIGQUIT, clean_up );
  signal( SIGHUP, clean_up ); 

  /* set up parameters and their defaults in parameter.c */
  initialize_parameters();

  /* Define optional and required command line arguments */
  select_cmd_line_options( option_list, num_options );
  select_cmd_line_arguments( argument_list, num_arguments);

  /* Parse the command line, including the optional params file */
  /* does sytnax, type, bounds checking and dies if neccessessary */
  parse_cmd_line_into_params_hash(argc, argv, "crux-create-index");

  /* Set verbosity */
  //TODO move this to parameter.c?
  verbosity = get_int_parameter("verbosity");
  set_verbosity_level(verbosity);
    
  /* Get parameter values */
  min_mass = get_double_parameter("min-mass");
  max_mass = get_double_parameter("max-mass");
  //check that min < max
  mass_range = (max_mass - min_mass)/MAX_INDEX_FILES;

  min_length = get_int_parameter("min-length");
  max_length = get_int_parameter("max-length");
  //check that min < max

  missed_cleavages = get_boolean_parameter("missed-cleavages");
  peptide_type = get_peptide_type_parameter("cleavages");
  mass_type = get_mass_type_parameter("isotopic-mass");

  /* create peptide constraint */
  constraint = new_peptide_constraint(peptide_type, min_mass, max_mass, 
				      min_length, max_length, 
				      missed_cleavages, mass_type);
  
  /* check if input file exist */
  in_file = get_string_parameter("protein fasta file");
  carp(CARP_DETAILED_DEBUG,"Input file name is '%s'\n", in_file);
  if(access(in_file, F_OK)){
    carp(CARP_FATAL, "The file \"%s\" does not exist " 
	 "(or is not readable or is empty).", in_file);
    exit(1);
  }
  
  /* check if output name already exists */
  // TODO when required name arg implemented

  /* create new index object */
  crux_index = 
    new_index(in_file,
	      constraint,
	      mass_range
	      );
  
  /* create crux_index files */
  if(!create_index(crux_index)){
    die("Failed to create index");
  }
  
  /* free index(frees constraint together) */
  free_index(crux_index);     
  free(binary_fasta_file);
  free_parameters();

  /* successfull exit message */
  carp(CARP_INFO, "crux-create-index finished.");

  exit(0);
}
