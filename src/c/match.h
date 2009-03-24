/**
 * \file match.h
 * $Revision: 1.19.4.1 $ 
 * \brief Object for given a peptide and a spectrum, generate a preliminary score(ex, Sp)
 ****************************************************************************/
#ifndef MATCH_H
#define MATCH_H

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <unistd.h>
#include "carp.h"
#include "parse_arguments.h"
#include "spectrum.h"
#include "spectrum_collection.h"
#include "ion.h"
#include "ion_series.h"
#include "crux-utils.h"
#include "objects.h"
#include "parameter.h"
#include "scorer.h"

/* Global variables */
#define NOT_SCORED FLT_MIN
#define P_VALUE_NA -1.0

/**
 * \returns a new memory allocated match
 */
MATCH_T* new_match(void);

/**
 * free the memory allocated match
 */
void free_match(
  MATCH_T* match ///< the match to free -in
  );

/**
 * shuffle the matches in the array between index start and end-1
 */
void shuffle_matches(
  MATCH_T** match_array, ///< the match array to shuffle  
  int start_idx,         ///< index of first element to shuffle
  int end_index          ///< index AFTER the last element to shuffle
  );

/**
 * sort the match array with the corresponding compare method
 */
void qsort_match(
  MATCH_T** match_array, ///< the match array to sort -in  
  int match_total,  ///< the total number of match objects -in
  void* compare_method ///< the compare method to use -in
  );

/**
 * compare two matches, used for qsort
 * \returns the difference between sp score in match_a and match_b
 */
int compare_match_sp(
  MATCH_T** match_a, ///< the first match -in  
  MATCH_T** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for qsort
 * \returns the difference between xcorr score in match_a and match_b
 */
int compare_match_xcorr(
  MATCH_T** match_a, ///< the first match -in  
  MATCH_T** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for qsort
 * \returns the difference between xcorr score in match_a and match_b
 */
int compare_match_q_value(
  MATCH_T** match_a, ///< the first match -in  
  MATCH_T** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for PERCOLATOR_SCORE
 * \returns the difference between PERCOLATOR_SCORE score in match_a and match_b
 */
int compare_match_percolator_score(
  MATCH_T** match_a, ///< the first match -in  
  MATCH_T** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and sp score, used for qsort.
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if sp score of match a is less than
 * match b.  1 if scan number and sp are equal, else 0.
 */
int compare_match_spectrum_sp(
  MATCH_T** match_a, ///< the first match -in  
  MATCH_T** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and xcorr, used for qsort.
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compare_match_spectrum_xcorr(
  MATCH_T** match_a, ///< the first match -in  
  MATCH_T** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and q-value, used for qsort.
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compare_match_spectrum_q_value(
  MATCH_T** match_a, ///< the first match -in  
  MATCH_T** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and percolator score,
 * used for qsort. 
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compare_match_spectrum_percolator_score(
  MATCH_T** match_a, ///< the first match -in  
  MATCH_T** match_b  ///< the scond match -in
  );

/**
 * print the information of the match
 */
void print_match(
  MATCH_T* match, ///< the match to print -in  
  FILE* file, ///< output stream -out
  BOOLEAN_T output_sequence, ///< should I output peptide sequence -in
  SCORER_TYPE_T output_mode  ///< the output mode -in
);

/**
 * \brief Print the match information in sqt format to the given file
 *
 * The main score goes in the position usually holding the xcorr.  The other
 * score goes in the position usually holding the preliminary Sp
 * score.  For searches analyzed by percolator, main and other should
 * be discriminant score and qvalue.
 */
void print_match_sqt(
  MATCH_T* match, ///< the match to print -in  
  FILE* file, ///< output stream -out
  SCORER_TYPE_T main_score,  ///< the main score to report -in
  SCORER_TYPE_T other_score  ///< the score to report -in
);

/**
 * \brief Print the match information in tab delimited format to the given file
 *
 */
void print_match_tab(
  MATCH_T* match,             ///< the match to print -in  
  FILE* file,                 ///< output stream -out
  int scan_num,             ///< starting scan number -in
  float spectrum_precursor_mz, ///< m/z of spectrum precursor -in
  float spectrum_mass,       ///< spectrum neutral mass -in
  int num_matches,            ///< num matches in spectrum -in
  int charge,                 ///< charge -in
  SCORER_TYPE_T main_score   ///< the main score to report -in
);

/**
 * serializes the match in binary
 *
 *
 *
 * <PEPTIDE_T: serialize peptide>
 * <float: score><int: ranking>* <--serialize for all score types
 * <SPECTRUM_T: serilize spectrum>
 * <float: b_y ion match ratio for SP>
 * <PEPTIDE_TYPE_T: the peptide type over-all peptide srcs>
 * <BOOLEAN_T: is this a null peptide?>
 *
 */
void serialize_match(
  MATCH_T* match, ///< the match to print -in
  FILE* file ///< output stream -out
  );


/*******************************************
 * match post_process extension
 ******************************************/
/**
 * Constructs the 20 feature array that pass over to percolator registration
 *\returns the feature float array
 */
double* get_match_percolator_features(
  MATCH_T* match, ///< the match to work -in                                          
  MATCH_COLLECTION_T* match_collection ///< the match collection to iterate -in
  );

/**
 *
 *\returns a match object that is parsed from the serialized result file
 */
MATCH_T* parse_match(
  FILE* result_file,  ///< the result file to parse PSMs -in
  DATABASE_T* database ///< the database to which the peptides are created -in
  // int num_top_match  ///< number of top PSMs serialized per spectrum -in
  );

/****************************
 * match get, set methods
 ***************************/

/**
 * Returns a heap allocated peptide sequence of the PSM
 * Sequence may not be the same as for the peptide if this is for a
 * decoy database.
 * User must free the sequence.
 *\returns the match peptide sequence
 */
char* get_match_sequence(
  MATCH_T* match ///< the match to work -in
  );

/**
 * Returns a heap allocated peptide sequence of the PSM formatted with
 * the flanking amino acids and modifiation symbols.
 *
 * Sequence is in the form of X.SEQ.X where X is the flanking amino
 * acid or - if peptide is at the end of the protein.
 * Sequence may not be the same as for the peptide if this is for a
 * decoy database.
 *\returns The sqt-formatted peptide sequence for this match.
 */
char* get_match_sequence_sqt(
  MATCH_T* match ///< the match to work -in
  );

/**
 * \brief Returns a newly allocated modified_aa sequence of the PSM
 * User must free the sequence.
 *\returns the match peptide sequence
 */
MODIFIED_AA_T* get_match_mod_sequence(
  MATCH_T* match ///< the match to work -in
  );

/**
 * \brief Returns a newly allocated string of sequence including any
 * modification characters. 
 * \returns The peptide sequence of the match including modification
 * characters. 
 */
char* get_match_mod_sequence_str(
  MATCH_T* match
  );

/**
 * Must ask for score that has been computed
 *\returns the match_mode score in the match object
 */
float get_match_score(
  MATCH_T* match, ///< the match to work -in  
  SCORER_TYPE_T match_mode ///< the working mode (SP, XCORR) -in
  );

/**
 * sets the match score
 */
void set_match_score(
  MATCH_T* match, ///< the match to work -out
  SCORER_TYPE_T match_mode, ///< the working mode (SP, XCORR) -in
  float match_score ///< the score of the match -in
  );

/**
 * Must ask for score that has been computed
 *\returns the match_mode rank in the match object
 */
int get_match_rank(
  MATCH_T* match, ///< the match to work -in  
  SCORER_TYPE_T match_mode ///< the working mode (SP, XCORR) -in
  );

/**
 * sets the rank of the match
 */
void set_match_rank(
  MATCH_T* match, ///< the match to work -in  
  SCORER_TYPE_T match_mode, ///< the working mode (SP, XCORR) -in
  int match_rank ///< the rank of the match -in
  );

/**
 *\returns the spectrum in the match object
 */
SPECTRUM_T* get_match_spectrum(
  MATCH_T* match ///< the match to work -in  
  );

/**
 * sets the match spectrum
 */
void set_match_spectrum(
  MATCH_T* match, ///< the match to work -out
  SPECTRUM_T* spectrum  ///< the working spectrum -in
  );

/**
 *\returns the peptide in the match object
 */
PEPTIDE_T* get_match_peptide(
  MATCH_T* match ///< the match to work -in  
  );

/**
 * sets the match peptide
 */
void set_match_peptide(
  MATCH_T* match, ///< the match to work -out
  PEPTIDE_T* peptide  ///< the working peptide -in
  );

/**
 * sets the match charge
 */
void set_match_charge(
  MATCH_T* match, ///< the match to work -out
  int charge  ///< the charge of spectrum -in
  );

/**
 * gets the match charge
 */
int get_match_charge(
  MATCH_T* match ///< the match to work -out
  );

/**
 * sets the match delta_cn
 */
void set_match_delta_cn(
  MATCH_T* match, ///< the match to work -out
  float delta_cn  ///< the delta cn value of PSM -in
  );

/**
 * gets the match delta_cn
 */
float get_match_delta_cn(
  MATCH_T* match ///< the match to work -out
  );

/**
 * sets the match ln_delta_cn
 */
void set_match_ln_delta_cn(
  MATCH_T* match, ///< the match to work -out
  float ln_delta_cn  ///< the ln delta cn value of PSM -in
  );

/**
 * gets the match ln_delta_cn
 */
float get_match_ln_delta_cn(
  MATCH_T* match ///< the match to work -out
  );

/**
 * sets the match ln_experiment_size
 */
void set_match_ln_experiment_size(
  MATCH_T* match, ///< the match to work -out
  float ln_experiment_size ///< the ln_experiment_size value of PSM -in
  );

/**
 * gets the match ln_experiment_size
 */
float get_match_ln_experiment_size(
  MATCH_T* match ///< the match to work -out
  );

/**
 *Increments the pointer count to the match object
 */
void increment_match_pointer_count(
  MATCH_T* match ///< the match to work -in  
  );

/**
 * sets the match if it is a null_peptide match
 */
void set_match_null_peptide(
  MATCH_T* match, ///< the match to work -out
  BOOLEAN_T is_null_peptid  ///< is the match a null peptide? -in
  );

/**
 * gets the match if it is a null_peptide match
 *\returns TRUE if match is null peptide, else FALSE
 */
BOOLEAN_T get_match_null_peptide(
  MATCH_T* match ///< the match to work -out
  );

/**
 * sets the match b_y_ion info
 */
void set_match_b_y_ion_info(
  MATCH_T* match, ///< the match to work -out
  SCORER_T* scorer ///< the scorer from which to extract information -in
  );

/**
 * gets the match b_y_ion_match
 */
float get_match_b_y_ion_fraction_matched(
  MATCH_T* match ///< the match to work -out
  );

/**
 * gets the match b_y_ion_matched
 */
int get_match_b_y_ion_matched(
  MATCH_T* match ///< the match to work -out
  );

/**
 * gets the match b_y_ion_possible
 */
int get_match_b_y_ion_possible(
  MATCH_T* match ///< the match to work -out
  );

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
#endif
