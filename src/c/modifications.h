/**
 * \file modifications.h
 * \brief Datatypes and methods for peptide modifications
 *
 * Two data structures define modifications.  The AA_MOD_T is the most
 * basic type.  It is the information provided by the user: mass
 * change caused by this mod, amino acids which may be modified in
 * this way, and the maximum number of this type of modification which
 * may occur on one peptide.  A collection of AA_MODs that may occur
 * on some peptide are represented as a PEPTIDE_MOD_T.  This stores
 * a list of AA_MODS and the net mass change experienced by the
 * peptide.  AA_MODs are instantiated once as given by the user.  All
 * possible PEPTIDE_MODs are calcualted once and reused for each
 * spectrum search.  One PEPTIDE_MOD corresponds to one mass window
 * that must be searched.
 * 
 * $Revision: 1.1.2.5 $
 */
#ifndef MODIFICATION_FILE_H
#define MODIFICATION_FILE_H

#include <assert.h>
#include "utils.h"
#include "linked_list.h"
#include "objects.h"
#include "parameter.h"

/* Public constants */
enum {MAX_AA_MODS = 11};

enum {AA_LIST_LENGTH = 26}; // A-Z

typedef short MODIFIED_AA_T; ///< letters in the expanded peptide
                             ///alphabet, bits for mod1 mod2...aa
/*
   e.g. There are three aa_mods specified in this run and they are
   given the masks mod1  1000_0000_0000_0000
                   mod2  0100_0000_0000_0000
                   mod3  0010_0000_0000_0000
   The amino acid Y has the value
                      Y  0000_0000_0001_0011

   Suppose you set the variable aa to Y.  Do stuff.  To ask "is aa
  modified by mod2", do this. answer = mod2 AND aa; answer == 0
   To change aa to be modified by modcalle2, aa = mod2 NOR aa
 (i.e. !(mod2 || aa) )  now aa == 0100_0000_0001_0011
   If we ask again, answer == 0100_0000_0000_0000 
 */

/**
 * \brief Allocate an AA_MOD, including the aa_list and initialize all
 * fields to default values.  Symbol and unique identifier are set
 * according to index.  
 * \returns A heap allocated AA_MOD with default values.
 */
AA_MOD_T* new_aa_mod(int mod_idx);

/**
 * \brief Free the memory for an AA_MOD including the aa_list.
 */
void free_aa_mod(AA_MOD_T*);

/**
 * \brief Allocate a PEPTIDE_MOD and set all fields to default values
 * (i.e. no modifications).
 * \returns A heap allocated PEPTIDE_MOD with default values.
 */
PEPTIDE_MOD_T* new_peptide_mod();

/**
 * \brief Allocate a new peptide mod and copy contents of given mod
 * into it.
 * \returns A pointer to a new peptide mod which is a copy of the
 * given one.
 */
PEPTIDE_MOD_T* copy_peptide_mod(PEPTIDE_MOD_T* original);

/**
 * \brief Free the memory for a PEPTIDE_MOD including the aa_list.
 */
void free_peptide_mod(PEPTIDE_MOD_T* mod);

/**
 * \brief Generate a list of all PEPTIDE_MODs that can be considered
 * given the list of AA_MODs provided by the user and found in parameter.c.
 *
 * This only needs to be called once for a search.  The list of
 * PEPTIDE_MODs can be reused for each spectrum.
 *
 * \returns The number of peptide_mods returned in the
 * peptide_mod_list argument.
 */
int generate_peptide_mod_list(
 PEPTIDE_MOD_T*** peptide_mod_list ///< return here the list of peptide_mods
);

/**
 * \brief Check a peptide sequence to see if the aa_mods in
 * peptide_mod can be applied. 
 *
 * Assumes that an amino acid can be modified by more than one aa_mod,
 * but not more than once by a single aa_mod as defined in modifiable().
 * \returns TRUE if the sequence can be modified, else FALSE
 */
BOOLEAN_T is_peptide_modifiable( PEPTIDE_T* peptide,
                            PEPTIDE_MOD_T* peptide_mod);


/**
 * \brief Take a peptide and a peptide_mod and return a list of
 * modified peptides.
 *
 * The peptide_mod should be guaranteed to be able to be applied to
 * the peptide at least once.  A single amino acid can be modified
 * multiple times by different aa_mods, but only once for a given
 * aa_mod as defined in modifiable().  
 * 
 * \returns The number of modified peptides in the array pointed to by
 * modified_peptides. 
 */
int modify_peptide(PEPTIDE_T* peptide,
                   PEPTIDE_MOD_T* peptide_mod,
                   PEPTIDE_T** modified_peptides);

/**
 * The new definition of a PEPTIDE_T object.
 * 
 */
struct peptide{
  unsigned char length; ///< The length of the peptide
  float peptide_mass;   ///< The peptide's mass with any modifications
  PEPTIDE_SRC_T* peptide_src; ///< a linklist of peptide_src
  BOOLEAN_T is_modified;   ///< if true sequence != NULL
  MODIFIED_AA_T* sequence; ///< sequence with modifications
};

/**
 * \brief checks to see if an amino acid is modified by a given mod
 * \returns TRUE if aa is modified by mod
 */
BOOLEAN_T is_aa_modified(MODIFIED_AA_T aa, AA_MOD_T* mod);


/**
 * \brief Finds the list of modifications made to an amino acid.
 *
 * Allocates a list of length(possible_mods) and fills it with pointers
 * to the modifications made to this aa as defined by
 * is_aa_modified().  Returns 0 and sets mod_list to NULL if the amino
 * acid is unmodified
 *
 * \returns The number of modifications made to this amino acid.
 */
int get_aa_mods(MODIFIED_AA_T aa, 
                AA_MOD_T* possible_mods, 
                AA_MOD_T** mod_list);

char modified_aa_to_char(MODIFIED_AA_T aa);
MODIFIED_AA_T char_aa_to_modified(char aa);

// in parameter.c
//global
//enum {MAX_AA_MODS = 11};//instead of #define forces typechecking, obeys scope
//AA_MOD_T* list_of_mods;
//int num_mods;
//AA_MOD_T* position_mods;
//int num_position_mods;

/**
 * \brief Read the paramter file and populate the static parameter
 * list of AA_MODS, inlcuding the list of position mods.
 *
 * Also updates the array of amino_masses.  Dies with an error if the
 * number of mods in the parameter file is greater than MAX_AA_MODS.
 * \returns void
 */
void read_mods_from_file(char* param_file);

/**
 * \brief Get the pointer to the list of AA_MODs requested by the
 * user.
 * \returns The number of mods pointed to by mods
 */
//int get_aa_mod_list(AA_MOD_T*** mods);

/**
 * \brief Check that the list of peptide_modifications from the file of
 * serialized PSMs matches those in the paramter file.
 *
 * If there was no parameter file or if it did not contain any mods,
 * return FALSE.  If the given mod list does not exactly match the
 * mods read from the parameter file (including the order in which
 * they are listed) return FALSE.  If returning false, print a warning
 * with the lines that should be included in the parameter file.
 *
 * \returns TRUE if the given mods are the same as those from the
 * parameter file.
 */
BOOLEAN_T compare_mods(AA_MOD_T* psm_file_mod_list, int num_mods);

/**
 * \brief Compare two mods to see if they are the same, i.e. same mass
 * change, unique identifier, position
 */
BOOLEAN_T compare_two_mods(AA_MOD_T* mod1, AA_MOD_T* mod2);

// in mass.c;
/**
 * \brief Extends the amino_masses table to include all possible
 * modifications.  
 *
 * Gets list of mods from parameter.c.  Should this fill in values for
 * both average and monoisotopic masses? 
 */
void extend_amino_masses(void);

/**
 * print all fields in mod.  For debugging
 */
void print_a_mod(AA_MOD_T* mod);

/**
 * print all fields in peptide mod. For debugging
 */
void print_p_mod(PEPTIDE_MOD_T* mod);

/* Setters and Getters */

/**
 * \brief Set the mass change caused by this modification.
 * \returns void
 */
void aa_mod_set_mass_change(AA_MOD_T* mod, double mass_change);
/**
 * \brief Get the mass change caused by this modification.
 * \returns The mass change caused by this modification.
 */
double aa_mod_get_mass_change(AA_MOD_T* mod);

/**
 * \brief Access to the aa_list of the AA_MOD_T struct.  This pointer
 * can be used to get or set the list of residues on which this mod
 * can be placed.
 * \returns A pointer to the list of amino acids on which this mod can
 * be placed.
 */
BOOLEAN_T* aa_mod_get_aa_list(AA_MOD_T* mod);

/**
 * \brief Set the maximum number of times this modification can be
 * placed on one peptide.
 * \returns void
 */
void aa_mod_set_max_per_peptide(AA_MOD_T* mod, int max);
/**
 * \brief Get the maximum number of times this modification can be
 * placed on one peptide.  
 * \returns The max times per peptide this mod can be placed.
 */
int aa_mod_get_max_per_peptide(AA_MOD_T* mod);

/**
 * \brief Set the maximum distance from the protein terminus that the
 * modification can be placed.  Which terminus (C or N) is determined
 * by the position type.  To indicate no position restriction, set to
 * MAX_PROTEIN_SEQ_LENGTH. 
 * \returns void
 */
void aa_mod_set_max_distance(AA_MOD_T* mod, int distance);
/**
 * \brief Get the maximum distance from the protein end that the
 * modification can be placed.  Will be MAX_PROTEIN_SEQ_LENGTH if
 * position type is ANY_POSITION.
 * \returns Maximum distance from protein terminus at which mod can be
 * placed. 
 */
int aa_mod_get_max_distance(AA_MOD_T* mod);

/**
 * \brief Set the position type of an aa_mod.
 * \returns void
 */
void aa_mod_set_position(AA_MOD_T* mod, MOD_POSITION_T position);

/**
 * \brief Where in the peptide can the modification be placed.
 * \returns ANY_POSITION for standard mods; C_TERM or N_TERM for those
 * that can only be placed at the ends of the peptide.
 */
MOD_POSITION_T aa_mod_get_position(AA_MOD_T* mod);

/**
 * \brief The character used to uniquely identify the mod in the sqt file.
 * \returns The character identifier.
 */
char aa_mod_get_symbol(AA_MOD_T* mod);

/**
 * \brief The bitmask used to uniquely identify the mod.
 * \returns The short int bitmask used to identify the mod.
 */
int aa_mod_get_identifier(AA_MOD_T* mod);

/**
 * \brief Add a new aa_mod to the peptide mod.  Updates mass_change,
 * num_mods and list_of_aa_mods.  Does not enforce the copy number of
 * an aa_mod to be less than max_per_peptide.
 * \returns void
 */
void peptide_mod_add_aa_mod(PEPTIDE_MOD_T* pep_mod,
                            AA_MOD_T* aa_mod,
                            int copies );

/**
 * \brief Get the value of the net mass change for this peptide_mod.
 * \returns The mass change for the peptide mod.
 */
double peptide_mod_get_mass_change(PEPTIDE_MOD_T* mod);

/**
 * \brief Get the number of aa_mods in this peptide_mod.
 * \returns The number of aa_mods in this peptide_mod.
 */
int peptide_mod_get_num_aa_mods(PEPTIDE_MOD_T* mod);

/**
 * \brief Get a pointer to the list of aa_mods in this peptide_mod.
 * The number of elements in the list is given by
 * peptide_mod_get_num_aa_mods. A unique aa_mod may be listed more
 * than once.  There is no particular order to the aa_mods in the
 * list.
 * \returns A pointer to a list of AA_MOD_T pointers.
 */
LINKED_LIST_T* peptide_mod_get_aa_mod_list(PEPTIDE_MOD_T* mod);

/**
 * \brief Compares the number of aa mods in two peptide mods for
 * sorting.
 * \returns Negative int, 0, or positive int if the number of aa_mods
 * in pmod 1 is less than, equal to or greater than the number of
 * aa_mods in pmod2, respectively.
 */
int compare_peptide_mod_num_aa_mods(const void* pmod1, 
                                    const void* pmod2);

#endif //MODIFICATION_FILE_H

