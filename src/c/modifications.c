/**
 * \file modifications.c
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
 */

#include "modifications.h"

/* Private constants */
enum { MAX_PROTEIN_SEQ_LENGTH = 40000 };

/* Private global variables */
char mod_sqt_symbols[MAX_AA_MODS] = {'*', '@', '#', '^', '~', '%', 
                                     '$', '&', '!', '?', '+'};
// FIXME: these need to be changed to bitmasks
int mod_id_masks[MAX_AA_MODS] = {1,2,3,4,5,6,7,8,9,10,11};

/* Private data types, typedefed in objects.h */

/**
 * \struct _aa_mod
 * 
 *  Modification at the amino acid level.  A single mass change that can
 *  occur on any of the residues listed.  This information is given by
 *  the user in the parameter file.  Also stores a character symbol
 *  assigned at runtime to be used in the sqt result file and an
 *  integer bitmask to be used to give each aa_mod a unique identifier.
 */
struct _aa_mod{ 
  double mass_change;  ///< the amount by which the mass of the residue changes
  BOOLEAN_T* aa_list;  ///< an array indexed by AA, true if can be modified
  int max_per_peptide; ///< the maximum number of mods per peptide
  MOD_POSITION_T position; ///< where the mod can occur in the pep/prot
  int max_distance;        ///< the max distance from the protein terminus
  char symbol;         ///< the character to represent the mod in sqt files
  int identifier;      ///< the offset/bitmask assigned to this mod for unique
                       //identification, used with MODIFIED_AA
};

/**
 * \struct _peptide_mod
 *  A collection of aa modifications that can occur on a single
 *  peptide.  Includes the list of aa_mods and the net mass
 *  change of all mods.  Each aa_mod is included in the list as many
 *  times at it appears in the collection. There is no information about
 *  which residues in a peptide are modified.
 */
struct _peptide_mod{
  double mass_change;     ///< the net mass change for the peptide
  int num_mods;           ///< the number of items in the list_of_mods
  LINKED_LIST_T* list_of_mods; ///< the list of aa_mods in this peptide
};

/* Definitions of public methods */

/**
 * \brief Allocate an AA_MOD, including the aa_list and initialize all
 * fields to default values.  Symbol and unique identifier are set
 * according to index.  
 * \returns A heap allocated AA_MOD with default values.
 */
AA_MOD_T* new_aa_mod(int mod_idx){

  assert(mod_idx > -1 && mod_idx < MAX_AA_MODS );

  AA_MOD_T* mod = (AA_MOD_T*)mymalloc(sizeof(AA_MOD_T));

  mod->mass_change = 0;
  mod->max_per_peptide = 0;
  mod->position = ANY_POSITION;
  mod->max_distance = MAX_PROTEIN_SEQ_LENGTH;
  mod->symbol = mod_sqt_symbols[mod_idx];
  mod->identifier = mod_id_masks[mod_idx];
  
  // allocate the aa lists for mods 
  mod->aa_list =                        // all 0's?
    (BOOLEAN_T*)mycalloc(AA_LIST_LENGTH, sizeof(AA_MOD_T)); 
  
  // initialize to FALSE
  int aa_idx = 0;
  for(aa_idx=0; aa_idx < AA_LIST_LENGTH; aa_idx++){
    mod->aa_list[aa_idx] = FALSE;
  }
  return mod;
}

/**
 * \brief Free the memory for an AA_MOD including the aa_list.
 */
void free_aa_mod(AA_MOD_T* mod){
  if( mod ){
    if( mod->aa_list ){free(mod->aa_list);}
    free(mod);
  }
}


/**
 * \brief Allocate a PEPTIDE_MOD and set all fields to default values
 * (i.e. no modifications).
 * \returns A heap allocated PEPTIDE_MOD with default values.
 */
PEPTIDE_MOD_T* new_peptide_mod(){
  PEPTIDE_MOD_T* new_mod = (PEPTIDE_MOD_T*)mymalloc(sizeof(PEPTIDE_MOD_T));
  new_mod->mass_change = 0;
  new_mod->num_mods = 0;
  new_mod->list_of_mods = NULL;
  return new_mod;
}

/**
 * \brief Allocate a new peptide mod and copy contents of given mod
 * into it.
 * \returns A pointer to a new peptide mod which is a copy of the
 * given one.
 */
PEPTIDE_MOD_T* copy_peptide_mod(PEPTIDE_MOD_T* original){
  PEPTIDE_MOD_T* copy = new_peptide_mod();
  copy->mass_change = original->mass_change;
  copy->num_mods = original->num_mods;
  copy->list_of_mods = copy_list(original->list_of_mods);

  return copy;
}

/**
 * \brief Free the memory for a PEPTIDE_MOD including the aa_list.
 */
void free_peptide_mod(PEPTIDE_MOD_T* mod){
  if(mod){
    if( mod->list_of_mods ){free(mod->list_of_mods);}
    free(mod);
  }
}

/* see below */
int generate_peptide_mod_list_TESTER(
 PEPTIDE_MOD_T*** peptide_mod_list,
 AA_MOD_T** aa_mod_list,
 int num_aa_mods);

/**
 * \brief Generate a list of all PEPTIDE_MODs that can be considered
 * given the list of AA_MODs and POSITION_MODs provided by the user
 * via the parameter file.
 *
 * This only needs to be called once for a search.  The list of
 * PEPTIDE_MODs can be reused for each spectrum.  Allows at most one
 * n-term and one c-term mod and at most mod->max_per_peptide
 * modifications of one type per peptide.
 *
 * The actual work is done in generate_peptide_mod_list_TESTER() so
 * that it can be tested without going through parameter.c.
 *
 * \returns The number of peptide_mods listed in the
 * peptide_mod_list argument.
 */
int generate_peptide_mod_list(
 PEPTIDE_MOD_T*** peptide_mod_list ///< return here the list of peptide_mods
 ){
  AA_MOD_T** aa_mod_list = NULL;
  int num_aa_mods = get_all_aa_mod_list(&aa_mod_list);

  // continued below
  int count = generate_peptide_mod_list_TESTER( peptide_mod_list, 
                                                aa_mod_list, num_aa_mods);
  return count; 
}

int generate_peptide_mod_list_TESTER(
 PEPTIDE_MOD_T*** peptide_mod_list,
 AA_MOD_T** aa_mod_list,
 int num_aa_mods)
{

  // initialize list of peptide mods with one unmodified peptide
  LINKED_LIST_T* final_list = new_list( new_peptide_mod() );
  int final_counter = 1;

  // for each aa_mod
  //AA_MOD_T* cur_aa_mod = NULL;
  int mod_list_idx = 0;
  for(mod_list_idx=0; mod_list_idx < num_aa_mods; mod_list_idx++){

    AA_MOD_T* cur_aa_mod = aa_mod_list[mod_list_idx];
    int cur_mod_max = cur_aa_mod->max_per_peptide;
    char cur_mod_id = cur_aa_mod->symbol;
    carp(CARP_DETAILED_DEBUG, "cur max %d, id %c", cur_mod_max, cur_mod_id);

    // initialize temp list and pointer to end (to speed up adds)
    LINKED_LIST_T* temp_list = NULL;
    LINKED_LIST_T* temp_list_end = NULL;
    int temp_counter = 0;
    //PEPTIDE_MOD_T* cur_pep_mod = NULL;

    int copies = 1;
    for(copies = 1; copies < cur_mod_max + 1; copies++){

      // for each pep_mod in final list
      LINKED_LIST_T* final_ptr = final_list;
      while( final_ptr != NULL ){
        // get the current peptide mod
        PEPTIDE_MOD_T* cur_pep_mod = 
          (PEPTIDE_MOD_T*)get_data_linked_list(final_ptr);
        carp(CARP_DETAILED_DEBUG, "cur pep_mod has %d mods", 
             peptide_mod_get_num_aa_mods(cur_pep_mod)); 

        // copy the peptide_mod and add to it i copies of cur_mod
        PEPTIDE_MOD_T* mod_cpy = copy_peptide_mod(cur_pep_mod);
        peptide_mod_add_aa_mod(mod_cpy, cur_aa_mod, copies);
        carp(CARP_DETAILED_DEBUG, "adding %d %c's to temp", 
             copies, cur_mod_id);
        //print_p_mod(mod_cpy);

        // add to temp list
        temp_list_end = add_linked_list(temp_list_end, mod_cpy);
        // if this is the first added, set the head of temp list
        temp_list = (temp_list==NULL) ? temp_list_end : temp_list;
        temp_counter++;
        final_ptr = get_next_linked_list(final_ptr);
      }// last in final list

    }// max copies

    // append temp list to final list
    carp(CARP_DETAILED_DEBUG, "adding temp list (%d) to final (%d)",
         temp_counter, final_counter);
    combine_lists(final_list, temp_list);
    final_counter += temp_counter;
  }// last aa_mod
  carp(CARP_INFO, "Created %d peptide mods", final_counter);

  // allocate an array of PEPTIDE_MOD_T* to return
  PEPTIDE_MOD_T** final_array = (PEPTIDE_MOD_T**)mycalloc(final_counter, 
                                                   sizeof(PEPTIDE_MOD_T*));
  //fill in the array and delete the list
  LINKED_LIST_T* final_list_ptr = final_list;
  int pep_idx = 0;
  while(final_list != NULL){
    assert(pep_idx < final_counter);

    final_array[pep_idx] = (PEPTIDE_MOD_T*)get_data_linked_list(final_list);  
    final_list = get_next_linked_list(final_list);
    pep_idx++;
  }

  delete_linked_list(final_list_ptr);

  // sort list
  qsort(final_array, final_counter, sizeof(PEPTIDE_MOD_T*),
        (void*)compare_peptide_mod_num_aa_mods);

  // set return value
  //peptide_mod_list = &final_array;
  *peptide_mod_list = final_array;
  return final_counter;
  /*
    get the number of items in final list

    init final_list to have one unmodified pep
    init temp_list to empty

    for each mod in aa_mod_list
      for i 1 to mod_max
        for each pep_mod in final_list
          copy the pep_mod and add i aa_mods
          add to the temp_list
        end of final_list
      end i==max
      add the temp_list to the final_list
    last mod

    sort by num-mods, fewest to most
   */
}

/**
 * \brief Check a peptide sequence to see if the aa_mods in
 * peptide_mod can be applied. 
 *
 * Assumes that an amino acid can be modified by more than one aa_mod,
 * but not more than once by a sligle aa_mod.
 * \returns TRUE if the sequence can be modified, else FALSE
 */
//BOOLEAN_T is_peptide_modifiable( PEPTIDE_T* peptide,
//                               PEPTIDE_MOD_T* peptide_mod){


  /* 

  for each aa_mod in peptide_mod
    initialize a counter to 0
  last aa_mod

  for each aa in peptide sequence
    for each aa_mod
      counter[aa_mod] += aa_mod.aa_list[aa]
    last aa_mod 
  last aa

  bool success = true
  for each aa_mod in peptide_mod
    success && (counter[aa_mod] >= peptide_mod.occurs[aa_mod])
  last aa_mod

  return success
  */
//}

/**
 * \brief Take a peptide and a peptide_mod and return a list of
 * modified peptides.
 *
 * The peptide_mod should be guaranteed to be applicable to
 * the peptide at least once.  A single amino acid can be modified
 * multiple times by different aa_mods, but only once for a given
 * aa_mod (as in defined by modifiable()).  
 * 
 * \returns The number of modified peptides in the array pointed to by
 * modified_peptides. 
 */
/*
int modify_peptide(
  PEPTIDE_T* peptide,             ///< the peptide to modify
  PEPTIDE_MOD_T* peptide_mod,     ///< the set of aa_mods to apply
  PEPTIDE_T** modified_peptides){ ///< the returned modified peptides
*/
  /*
  MODIFIED_AA_T** modified_seqs = NULL
  initialize array to hold the peptide seq with the first aa_mod applied
  and set count to the size of the array
  (ie int count = apply_mods_to_seq(peptide_seq, aa_mod[1], &modified_seqs)

  for each aa_mod in peptide_mod
    count = apply_mods_to_list(&mod_pep_seqs, count, aa_mod)
  last aa_mod

  for each seq in mod_pep_seq
    copy peptide and give it the modified seq
    add to the return array
  last seq

  return count
   */
//}


/**
 * take a list of modified_peptide_seqs, add to it the given
 * modification in all possible permutations
 */
/*
int apply_mods_to_list(
  MODIFIED_AA_T*** mod_seqs, ///< a pointer to an array of arrays
  int count, ///< the number of arrays pointed to by mod_seqs*
  AA_MOD_T* mod_to_apply){  ///< the specific mod to apply
*/
  /*
    init final list
    for each mod_seq
      init added_seqs
      count += apply_mods_to_seq(seq, mod, &added_seqs)
      add added_seqs to final list
    last mod_seq

    replace mod_seqs with final_list
    return count
   */
//}
/*
int apply_mods_to_seq(
  MODIFIED_AA_T* seq, ///< the seq to modify
  AA_MOD_T* mod,      ///< the mod to apply
  MODIFIED_AA_T*** return_list){ ///< the newly modified versions of
*/                               ///seq

  /*
    init return_list
    for each aa in seq
      if modifiable(aa, mod)
      then 
         copy seq
         modify the aa in seq
         add to list
         increment count
      end if
    last aa

    return count
   */
  
//}

/**
 * \brief Determine if this modified amino acid can be modified by
 * this modification.
 *
 * Checks the mod list of modifiable residues to see if aa is in the
 * list.  Also checks to see if aa has already been modified by this
 * mod.  
 * \returns TRUE if it can be modified, else FALSE
 */
//bool modifiable(MODIFIED_AA_T* aa, AA_MOD_T* mod){
  // if the residue is in the list of residues that can be modified
  // AND if the residue is not already modified by the same mod
  //    return true
  // else 
  //    return false
//}

BOOLEAN_T is_aa_modified(MODIFIED_AA_T aa, AA_MOD_T* mod){
  int id = mod->identifier;
  if( (aa && id) != 0 ){
    return TRUE;
  } 
  return FALSE;
 }

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
/*
int get_aa_mods(MODIFIED_AA_T aa, 
                int num_possible,
                AA_MOD_T* possible_mods, 
                AA_MOD_T*** mod_list){
*/
  /*
    int count = 0;
    mod_list* = new AA_MOD_T[num_possible]
    for 0 to num_possible
      if( is_aa_modified() ){
        mod_list[count] = this mod
        count += 1
      endif
    last num_possible
    if( count == 0 )
       free mod_list
    return count;
   */
//}


/********
 Existing methods to change

 FILE match_collection.c:

 print_sqt_header()
 serialize_headers()
 print_match_collection_sqt()  Actually, the change may only occur in peptide
 serialize_psm_features()
 extend_match_collection()    This is what reads psm header file info

 FILE peptide.c:

 serialize_peptide() minimum: number of mods, two lists--one of
                              identifiers and one of sequnce indexes
 parse_peptide()     if modified, fill sequence, add id to indexed aa


 FILE mass.c:
 get_mass_amino_acid()  change char to short?, new method?

QUESTIONS
* do we require that the parameter file with the mods be included for
  crux_analyae_matches?

* is there a slicker way to index char aa's other than aa - 'A' ?

create branch, add these files to branch
find cvs branching tutorial, send to AK
 */

/**
 * print all fields in aa mod.  For debugging
 */
void print_a_mod(AA_MOD_T* mod){
  printf("AMOD: mass %.2f, max per %d, max dist %d, symb %c, aa list ",
         mod->mass_change, mod->max_per_peptide, mod->max_distance,
         mod->symbol);

  int i = 0;
  for(i=0; i<AA_LIST_LENGTH; i++){
    if( mod->aa_list[i] == TRUE ){
      printf("%c", (i + 'A'));
    }
  }
  printf("\n");
}

/**
 * print all fields in peptide mod. For debugging
 */
void print_p_mod(PEPTIDE_MOD_T* mod){
  printf("PMOD: mass %.2f, num %d, aa mods\n  ", 
         mod->mass_change, mod->num_mods);

  LINKED_LIST_T* traverse_list = mod->list_of_mods;
  while( traverse_list != NULL ){
    print_a_mod( (AA_MOD_T*)get_data_linked_list(traverse_list) );
    printf("  ");
    traverse_list = get_next_linked_list(traverse_list);
  }
  printf("\n");
}

/* Setters and Getters */

/**
 * \brief Set the mass change caused by this modification.
 * \returns void
 */
void aa_mod_set_mass_change(AA_MOD_T* mod, double mass_change){
  mod->mass_change = mass_change;
}

/**
 * \brief Get the mass change caused by this modification.
 * \returns The mass change caused by this modification.
 */
double aa_mod_get_mass_change(AA_MOD_T* mod){
  return mod->mass_change;
}

/**
 * \brief Access to the aa_list of the AA_MOD_T struct.  This pointer
 * can be used to get or set the list of residues on which this mod
 * can be placed.
 * \returns A pointer to the list of amino acids on which this mod can
 * be placed.
 */
BOOLEAN_T* aa_mod_get_aa_list(AA_MOD_T* mod){
  return mod->aa_list;
}

/**
 * \brief Set the maximum number of times this modification can be
 * placed on one peptide.
 * \returns void
 */
void aa_mod_set_max_per_peptide(AA_MOD_T* mod, int max){
  mod->max_per_peptide = max;
}

/**
 * \brief Get the maximum number of times this modification can be
 * placed on one peptide.  
 * \returns The max times per peptide this mod can be placed.
 */
int aa_mod_get_max_per_peptide(AA_MOD_T* mod){
  return mod->max_per_peptide;
}

/**
 * \brief Set the maximum distance from the protein terminus that the
 * modification can be placed.  Which terminus (C or N) is determined
 * by the position type.  To indicate no position restriction, set to
 * MAX_PROTEIN_SEQ_LENGTH. 
 * \returns void
 */
void aa_mod_set_max_distance(AA_MOD_T* mod, int distance){
  mod->max_distance = (distance == -1) ? MAX_PROTEIN_SEQ_LENGTH : 
                                         distance;
}

/**
 * \brief Get the maximum distance from the protein end that the
 * modification can be placed.  Will be MAX_PROTEIN_SEQ_LENGTH if
 * position type is ANY_POSITION.
 * \returns Maximum distance from protein terminus at which mod can be
 * placed. 
 */
int aa_mod_get_max_distance(AA_MOD_T* mod){
  return mod->max_distance;
}

/**
 * \brief Set the position type of an aa_mod.
 * \returns void
 */
void aa_mod_set_position(AA_MOD_T* mod, MOD_POSITION_T position){
  mod->position = position;
}

/**
 * \brief Where in the peptide can the modification be placed.
 * \returns ANY_POSITION for standard mods; C_TERM or N_TERM for those
 * that can only be placed at the ends of the peptide.
 */
MOD_POSITION_T aa_mod_get_position(AA_MOD_T* mod){
  return mod->position;
}

/**
 * \brief The character used to uniquely identify the mod in the sqt file.
 * \returns The character identifier.
 */
char aa_mod_get_symbol(AA_MOD_T* mod){
  return mod->symbol;
}

/**
 * \brief The bitmask used to uniquely identify the mod.
 * \returns The short int bitmask used to identify the mod.
 */
int aa_mod_get_identifier(AA_MOD_T* mod){
  return mod->identifier;
}

/**
 * \brief Add a new aa_mod to the peptide mod.  Updates mass_change,
 * num_mods and list_of_aa_mods.  Does not enforce the copy number of
 * an aa_mod to be less than max_per_peptide.
 * \returns void
 */
void peptide_mod_add_aa_mod(
  PEPTIDE_MOD_T* pep_mod, ///< The peptide mod being added to
  AA_MOD_T* aa_mod,       ///< The aa_mod to add
  int copies ){           ///< How many of the aa_mod to add
  pep_mod->mass_change += aa_mod->mass_change * copies;
  pep_mod->num_mods += copies;

  if( pep_mod->list_of_mods == NULL ){
    pep_mod->list_of_mods = new_list(aa_mod);
  }else{
    add_linked_list(pep_mod->list_of_mods, aa_mod);
  }

  // could speed this up by keeping a pointer to the end
  int added = 0;
  for(added = 1; added < copies; added++){
    add_linked_list(pep_mod->list_of_mods, aa_mod);
  }
}

/**
 * \brief Get the value of the net mass change for this peptide_mod.
 * \returns The mass change for the peptide mod.
 */
double peptide_mod_get_mass_change(PEPTIDE_MOD_T* mod){
  return mod->mass_change;
}

/**
 * \brief Get the number of aa_mods in this peptide_mod.
 * \returns The number of aa_mods in this peptide_mod.
 */
int peptide_mod_get_num_aa_mods(PEPTIDE_MOD_T* mod){
  return mod->num_mods;
}

/**
 * \brief Get a pointer to the linked list of aa_mods for this peptide_mod.
 * The number of elements in the list is given by
 * peptide_mod_get_num_aa_mods. A unique aa_mod may be listed more
 * than once.  There is no particular order to the aa_mods in the
 * list.
 * \returns A pointer to a linked list pointing to AA_MOD_T's.
 */
LINKED_LIST_T* peptide_mod_get_aa_mod_list(PEPTIDE_MOD_T* mod){
  return mod->list_of_mods;
}

/**
 * \brief Compares the number of aa mods in two peptide mods for
 * sorting.
 * \returns Negative int, 0, or positive int if the number of aa_mods
 * in pmod 1 is less than, equal to or greater than the number of
 * aa_mods in pmod2, respectively.
 */
int compare_peptide_mod_num_aa_mods(const void* pmod1, 
                                    const void* pmod2){
  return (*(PEPTIDE_MOD_T**)pmod1)->num_mods 
            - (*(PEPTIDE_MOD_T**)pmod2)->num_mods;
}

