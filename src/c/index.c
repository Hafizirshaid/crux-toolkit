/*****************************************************************************
 * \file index.c
 * $Revision: 1.8 $
 * \brief: Object for representing an index of a database
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "utils.h"
#include "crux-utils.h"
#include "peptide.h"
#include "protein.h"
#include "index.h"
#include "carp.h"
#include "objects.h"
#include "peptide_constraint.h"
#include "database.h"

//maximum proteins the index can handle
#define MAX_PROTEIN 30000
#define MAX_INDEX_FILES 30000
#define MAX_FILE_NAME_LENGTH 30

/* 
 * How does the create_index (the routine) work?
 *
 * - Creates a database object from a the fasta file member variable.
 *
 * - Depending on the size of the database, determines how many passes it
 *   will need to make over the database peptides (first implementation
 *   will only make one pass, later implementation can make multiple passes
 *   with multiple iterators)
 *
 * - From the directory and mass resolution member variables, 
 *   creates the list of filenames necessary for storing the peptides 
 *   
 * - Creates a database_peptide_iterator object from the database 
 *   and peptide constraint objects
 *
 * - Then starts iterating through peptides 
 *
 *    - From the peptide mass determines which file to place it in, and
 *      then calls the serialize_peptide method
 *
 *      - The serialize peptide method is an object method specifically
 *        designed for this purpose, and writes a peptide in a format from
 *        which it can be reconstructed
 *
 *        - Serialize peptide needs to serialize the peptide_src objects. To
 *        do that, it needs the idx of the peptide_src's protein objects in
 *        the database. This is retrieved from the protein idx member
 *        variable (i.e. this protein is the 0th protein, 1st protein etc.)
 *        which is set at database creation. Note, that this won't have to 
 *        change when we move to light proteins.
 *
 * LATER At some point we will index each of the peptide files too (in a TOC), 
 * so that we can retrieve them rapidly later. 
 *
 * LATER We implement light proteins and possibly an 
 * LATER create_index for protein locations in the database allowing rapid
 * creation of light protein objects.
 */

/* 
 * How does generate_peptides (the executable) with --from-index work?
 *
 * - Given a fasta file, looks for the corresponding index on disk. Fail if it
 *   can't find the corresponding index files.
 *
 * - Instantiates an index object from the fasta file with new_index.
 *
 * - Attempts to parse_index the index.
 *
 * - Instantiates an index_peptide_iterator from the index, according to
 *   constraints passed on the command line to generate_peptides.
 *
 * - Then starts iterating through peptides, which are being loaded from
 *   disk, and outputs them as before
 * 
 * LATER We implement light proteins.
 * LATER use an index for protein offsets in the database allowing rapid
 * creation of light protein objects.
 *
 * LATER Develop a conversion from light to heavy and heavy to light protein
 * objects to avoid excessive memory use.
 */

/**
 * \struct index
 * \brief A index of a database
 */
struct index{
  DATABASE_T* database; ///< The database that has been indexed.
  char* directory; ///< The directory containing the indexed files
  //char* filenames[INDEX_MAX_FILES]; ///< The files that contain the peptides
  PEPTIDE_CONSTRAINT_T* constraint; ///< Constraint which these peptides satisfy
  BOOLEAN_T on_disk; ///< Does this index exist on disk yet?
  float mass_range;  ///< the range of mass that each index file should be partitioned into
  unsigned int max_size;  ///< maximum limit of each index file
};    

/**
 * \struct index files
 * \brief A struct that contains the information of each file
 */
struct index_file{
  char* filename;  ///< The file name that contain the peptides
  float start_mass; ///< the start mass limit in this file
  float interval;   ///< the interval of the peptides in this file
};
typedef struct index_file INDEX_FILE_T;


/**
 * \struct index_peptide_iterator
 * \brief An iterator to iterate over the peptides in a database
 */
struct index_peptide_iterator{
  INDEX_T* index; ///< The index object which we are iterating over
  //PEPTIDE_CONSTRAINT_T* constraint; ///< The constraints which peptides should satisfy
  DATABASE_PROTEIN_ITERATOR_T* db_protein_iterator; ///< used to access the protein array in database
  INDEX_FILE_T* index_files[MAX_INDEX_FILES]; ///< the index file array that contain information of each index file 
  int total_index_files; ///< the total count of index_files
  int current_index_file; ///< the current working index_file
  //unsigned int peptide_idx; ///< The (non-object) index of the current peptide.
  //char* current_index_filename; ///< The current filename that we are reading from
  FILE* index_file; ///< The current file stream that we are reading from
  BOOLEAN_T has_next; ///< Is there another peptide?
  PEPTIDE_T* peptide; ///< the next peptide to return
};    

/************
 * index
 ************/

/**
 * \returns An (empty) index object.
 */
INDEX_T* allocate_index(void){
  INDEX_T* index = (INDEX_T*)mycalloc(1, sizeof(INDEX_T));
  return index;
}

/**
 * given a fasta_file name it returns the index file directory name
 * format: myfasta_crux_index
 * \returns A heap allocated index file directory name of the given fasta file
 */
char* generate_directory_name(
  char* fasta_filename
  )
{
  int len = strlen(fasta_filename);
  int end_idx = len;
  int end_path = len;  //index of where the "." is located in the file
  char* dir_name = NULL;
  char* dir_name_tag =  "_crux_index";
  
  //cut off the ".fasta" if needed
  for(; end_idx > 0; --end_idx){
    if(strcmp(&fasta_filename[end_idx - 1], ".fasta") == 0){
      end_path = end_idx - 1;
      break;
    }
  }
  
  dir_name = (char*)mycalloc(end_path + strlen(dir_name_tag) + 1, sizeof(char));
  strncpy(dir_name, fasta_filename, end_path);
  strcat(dir_name, dir_name_tag);
  return dir_name;
}
  


/**
 * Assumes that the fasta file is always in the ??? directory
 * \returns A new index object.
 */
INDEX_T* new_index(
  char* fasta_filename,  ///< The fasta file
  PEPTIDE_CONSTRAINT_T* constraint,  ///< Constraint which these peptides satisfy
  float mass_range,  ///< the range of mass that each index file should be partitioned into
  unsigned int max_size  ///< maximum limit of each index file
  )
{
  char** filename_and_path = NULL;
  char* working_dir = NULL;
  INDEX_T* index = allocate_index();
  DATABASE_T* database = new_database(fasta_filename);
    
  filename_and_path = parse_filename_path(fasta_filename);
  working_dir = generate_directory_name(filename_and_path[0]);
  DIR* check_dir = NULL;

  //check if the index files are on disk
  //are we currently in the crux dircetory
  if(filename_and_path[1] == NULL){
    if((check_dir =  opendir(working_dir)) != NULL){
      set_index_on_disk(index, TRUE);
    }
    else{
      set_index_on_disk(index, FALSE);
    }
  }
  else{//we are not in crux directory
    char* full_path = cat_string(filename_and_path[1],  working_dir);
    if((check_dir = opendir(full_path)) != NULL){
      set_index_on_disk(index, TRUE);
    }
    else{
      set_index_on_disk(index, FALSE);
    }
    free(full_path);
  }
  
  //set each field
  set_index_directory(index, working_dir);
  set_index_constraint(index, constraint);
  set_index_database(index, database);
  set_index_mass_range(index, mass_range);
  set_index_max_size(index, max_size);

  //free filename and path string array
  free(check_dir);
  free(working_dir);
  free(filename_and_path[0]);
  free(filename_and_path[1]);
  free(filename_and_path);
  
  return index;
}         

/**
 * Frees an allocated index object.
 */
void free_index(
  INDEX_T* index
  )
{
  free_database(index->database);
  free(index->directory);
  free_peptide_constraint(index->constraint);
  free(index);
}

/**
 * write to the file stream various information of the
 * index files created
 */
BOOLEAN_T write_header(
  INDEX_T* index, ///< the working index -in
  FILE* file ///< out put stream for crux_index_map -in
  )
{
  time_t hold_time;
  hold_time = time(0);

  fprintf(file, "#\tCRUX index directory: %s\n", index->directory);
  fprintf(file, "#\ttime created: %s",  ctime(&hold_time)); 
  fprintf(file, "#\tmaximum size of each index file: %d\n", index->max_size);
  fprintf(file, "#\ttarget mass range for index file: %.2f\n", index->mass_range);
  fprintf(file, "#\tcopyright: %s\n", "William Noble");
  return TRUE;
}

/**
 * The main index method. Does all the heavy lifting, creating files
 * serializing peptides, etc. The index directory itself should have 
 * a standard suffix (e.g. cruxidx), so that a given fasta file will have
 * an obvious index location.
 *
 * assumes that the current directory is the crux directory where the fasta file is located
 *
 * Note: create an index .info file as to map masses to files and to store 
 * all information that was used to create this index.
 * \returns TRUE if success. FALSE if failure.
 */
BOOLEAN_T create_index(
  INDEX_T* index ///< An allocated index -in
  )
{
  FILE* output = NULL; // the file stream for each index file
  FILE* info_out = NULL; // the file stream where the index creation infomation is sent
  DATABASE_SORTED_PEPTIDE_ITERATOR_T* sorted_iterator = NULL;
  PEPTIDE_T* peptide = NULL;
  unsigned int num_peptides = 0; //current number of peptides index file 
  int num_file = 1; //the ith number of index file created
  float current_mass_limit = index->mass_range;
  char* filename_tag = "crux_index_";
  char* file_num = NULL;

  //check if already created index
  if(index->on_disk){
    carp(CARP_INFO, "index already been created on disk");
    return TRUE;
  }
  //create temporary directory
  if(mkdir("crux_temp", S_IRWXU) !=0){
    carp(CARP_WARNING, "cannot create temporary directory");
    return FALSE;
  }
  
  //create peptide iterator
  sorted_iterator = 
    new_database_sorted_peptide_iterator(index->database, index->constraint, MASS, TRUE);
     
  //check if any peptides are found
  if(!database_sorted_peptide_iterator_has_next(sorted_iterator)){
    carp(CARP_WARNING, "no matches found");
    return FALSE;
  }
  
  //move into temporary directory
  if(chdir("crux_temp") != 0){
    carp(CARP_WARNING, "cannot enter temporary directory");
    return FALSE;
  }
  
  //create the index map & info
  info_out = fopen("crux_index_map", "w");
  write_header(index, info_out);

  do{ 
    char* filename;

    //open the next index file
    if(num_peptides == 0){
      file_num = int_to_char(num_file);
      filename = cat_string(filename_tag, file_num);
      output = fopen(filename, "w" );
      fprintf(info_out, "%s\t%.2f\t", filename, 0.00);
      free(file_num);
      free(filename);
    }
    
    peptide = database_sorted_peptide_iterator_next(sorted_iterator);
    
    //set the index file to the correct interval
    while(get_peptide_peptide_mass(peptide) > current_mass_limit ||
          num_peptides > index->max_size-1){
      fclose(output);
      ++num_file;
      num_peptides = 0;
      file_num = int_to_char(num_file);
      filename = cat_string(filename_tag, file_num);
      output = fopen(filename, "w");
     
      //reset mass limit and update crux_index_map
      if(get_peptide_peptide_mass(peptide) > current_mass_limit){
        fprintf(info_out, "%.2f\n", index->mass_range);
        fprintf(info_out, "%s\t%.2f\t", filename, current_mass_limit+0.01);//!!boarder conditions
        current_mass_limit += index->mass_range;
      }
      else{ //num_peptides > index->max_size
        fprintf(info_out, "%.2f\n",
                index->mass_range - 
                (current_mass_limit - get_peptide_peptide_mass(peptide))-0.01);
        fprintf(info_out, "%s\t%.2f\t", filename, get_peptide_peptide_mass(peptide));
        current_mass_limit = index->mass_range + get_peptide_peptide_mass(peptide);
      }

      free(file_num);
      free(filename);
    }

    serialize_peptide(peptide, output);
    free_peptide(peptide);
    
    ++num_peptides;
  } //serialize the peptides into index files
  while(database_sorted_peptide_iterator_has_next(sorted_iterator));
  
  //print land line in crux index map
  fprintf(info_out, "%.2f\n", index->mass_range);
  
  //close last index file and crux_index_map file
  fclose(info_out);
  fclose(output);

  //free iterator
  free_database_sorted_peptide_iterator(sorted_iterator);

  chdir("..");
  //rename crux_temp to final directory name
  if(rename("crux_temp", index->directory) != 0){
    carp(CARP_WARNING, "cannot rename directory");
    return FALSE;
  }

  index->on_disk = TRUE;
  return TRUE;
}

/**
 * Does this index exist on disk?
 *
 * \returns TRUE if it does. FALSE if it does not.
 */
BOOLEAN_T index_exists(
  INDEX_T* index ///< An allocated index
  )
{
  return index->on_disk;
}

/*
 * Private methods
 */

/*
 * Returns the index filename appropriate for this peptide
 */
char* get_peptide_file_name(
    INDEX_T* index,
    PEPTIDE_T* peptide
    );



/*********************************************
 * set and get methods for the object fields
 *********************************************/

/**
 *\returns the directory of the index
 * returns a heap allocated new copy of the directory
 * user must free the return directory name
 */
char* get_index_directory(
  INDEX_T* index ///< The index -in
  )
{
  return my_copy_string(index->directory);
}

/**
 * sets the directory of the index
 * index->directory must been initiailized
 */
void set_index_directory(
  INDEX_T* index, ///< The index -in
  char* directory ///< the directory to add -in
  )
{
  free(index->directory);
  index->directory = my_copy_string(directory);
}

/**
 *\returns a pointer to the database
 */
DATABASE_T* get_index_database(
  INDEX_T* index ///< The index -in
  )
{
  return index->database;
}

/**
 * sets the database of the index
 */
void set_index_database(
  INDEX_T* index, ///< The index -in
  DATABASE_T* database ///< The database that has been indexed. -in
  )
{
  index->database = database;
}

/**
 *\returns a pointer to the peptides constraint
 */
PEPTIDE_CONSTRAINT_T* get_index_constraint(
  INDEX_T* index ///< The index -in
  )
{
  return index->constraint;
}

/**
 * sets the peptides constraint
 */
void set_index_constraint(
  INDEX_T* index, ///< The index -in
  PEPTIDE_CONSTRAINT_T* constraint ///< Constraint which these peptides satisfy -in
  )
{
  index->constraint = constraint;
}

/**
 *\returns TRUE if index files are on disk else FALSE
 */
BOOLEAN_T get_index_on_disk(
  INDEX_T* index ///< The index -in
  )
{
  return index->on_disk;
}

/**
 * sets the on disk field of index
 */
void set_index_on_disk(
  INDEX_T* index, ///< The index -in
  BOOLEAN_T on_disk ///< Does this index exist on disk yet? -in
  )
{
  index->on_disk = on_disk;
}

/**
 *\returns the range of mass that each index file should be partitioned into
 */
float get_index_mass_range(
  INDEX_T* index ///< The index -in
  )
{
  return index->mass_range;
}

/**
 * sets the mass_range field of index
 */
void set_index_mass_range(
  INDEX_T* index, ///< The index -in
  float mass_range  ///< the range of mass that each index file should be partitioned into -in
  )
{
  index->mass_range = mass_range;
}

/**
 *\returns maximum limit of each index file
 */
unsigned int get_index_max_size(
  INDEX_T* index ///< The index -in
  )
{
  return index->max_size;
}

/**
 * sets the maximum limit of each index file for the index object
 */
void set_index_max_size(
  INDEX_T* index, ///< The index -in
  unsigned int max_size  ///< maximum limit of each index file -in
  )
{
  index->max_size = max_size;
}

/**************************
 * Index file
 **************************/

//FIXME see if filename is a heap allocated
/**
 *\returns a new heap allocated index file object
 */
INDEX_FILE_T* new_index_file(
  char* filename,  ///< the filename to add -in
  float start_mass,  ///< the start mass of the index file  -in
  float range  ///< the mass range of the index file  -in
  )
{
  INDEX_FILE_T* index_file =
    (INDEX_FILE_T*)mycalloc(1, sizeof(INDEX_FILE_T));
  
  index_file->filename = filename;
  index_file->start_mass = start_mass;
  index_file->interval = range;

  return index_file;
}

/**
 * adds a new index_file object to the index_file
 * \returns TRUE if successfully added the new index_file
 */
BOOLEAN_T add_new_index_file(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator,  ///< the index_peptide_iterator to add file -out
  char* filename_parsed,  ///< the filename to add -in
  float start_mass,  ///< the start mass of the index file  -in
  float range  ///< the mass range of the index file  -in
  )
{
  char* filename = my_copy_string(filename_parsed);
  
  //check if total index files exceed MAX limit
  if(index_peptide_iterator->total_index_files > MAX_INDEX_FILES-1){
    carp(CARP_WARNING, "too many index files to read");
    return FALSE;
  }
  //create new index_file
  index_peptide_iterator->index_files[index_peptide_iterator->total_index_files] =
    new_index_file(filename, start_mass, range);
  
  ++index_peptide_iterator->total_index_files;
  return TRUE;
}

/**
 * frees the index file
 */
void free_index_file(
  INDEX_FILE_T* index_file  ///< the index file object to free -in
  )
{
  free(index_file->filename);
  free(index_file);
}

/*************************************
 * index_peptide iterator subroutines
 *************************************/

/**
 * parses the "crux_index_map" file that contains the mapping between
 * each crun_index_* file and mass range
 * Adds all crux_index_* files that are with in the peptide constraint mass range
 * \returns TRUE if successfully parses crux_index_map
 */
BOOLEAN_T parse_crux_index_map(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator
  )
{
  FILE* file = NULL;
  
  //used to parse each line from file
  char* new_line = NULL;
  int line_length;
  size_t buf_length = 0;
  
  //used to parse with in a line
  char filename[MAX_FILE_NAME_LENGTH] = "";
  float start_mass;
  float range;
  BOOLEAN_T start_file = FALSE;
  float min_mass = 
    get_peptide_constraint_min_mass(index_peptide_iterator->index->constraint);
  float max_mass = 
    get_peptide_constraint_max_mass(index_peptide_iterator->index->constraint);

  //move into the dir crux_files
  chdir(index_peptide_iterator->index->directory);
  
  //open crux_index_file
  file = fopen("crux_index_map", "r");
  if(file == NULL){
    carp(CARP_WARNING, "cannot open crux_index_map file");
    return FALSE;
  }
  
  while((line_length =  getline(&new_line, &buf_length, file)) != -1){
    //skip header lines
    if(new_line[0] == '#'){
      continue;
    }
    //is it a line for a crux_index_*
    else if(new_line[0] == 'c' && new_line[1] == 'r'){
      //read the crux_index_file information
      if(sscanf(new_line,"%s %f %f", 
                filename, &start_mass, &range) < 3){
        free(new_line);
        carp(CARP_WARNING, "incorrect file format");
        return FALSE;
      }
      //find the first index file with in mass range
      else if(!start_file){
        if(min_mass > start_mass + range){
          continue;
        }
        else{
          start_file = TRUE;
          if(!add_new_index_file(index_peptide_iterator, filename, start_mass, range)){
            carp(CARP_WARNING, "failed to add index file");
            return FALSE;
          }
          continue;
        }
      }
      //add all index_files that are with in peptide constraint mass interval
      else if(min_mass < (start_mass + range) + 0.01){
        if(!add_new_index_file(index_peptide_iterator, filename, start_mass, range)){
          carp(CARP_WARNING, "failed to add index file");
          return FALSE;
        }
        continue;
      }
      //out of mass range
      break;
    }
  }
  free(new_line);
  
  return TRUE;
}


/**
 * fast forward the index file pointer to the beginning of the
 * first peptide that would meet the peptide constraint, reads in
 * the length, mass then poistions the pointer at the start of peptide src count.
 * \returns TRUE if successfully finds the beginning of a peptide that meets the constraints
 */
BOOLEAN_T fast_forward_index_file(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator, ///< working index_peptide_iterator -in
  FILE* file, ///< the file stream to fast foward -in
  int* peptide_length_out, ///< the length of the peptide to be parsed -out
  float* peptide_mass_out  ///< the mass of the peptide to be parsed -out
  )
{
  //used to parse each line from file
  char* new_line = NULL;
  int line_length;
  size_t buf_length = 0;
  char temp_string[5] = "";
  BOOLEAN_T in_peptide = FALSE;
  BOOLEAN_T is_mass_ok = FALSE;

  //peptide fields
  int peptide_length;
  float peptide_mass;

  //parse each line
  while((line_length =  getline(&new_line, &buf_length, file)) != -1){
    
    //find start of peptide
    if(!in_peptide){
      if(new_line[0] != '*'){
        //skip all lines until the start of peptide sign '*'
        continue;
      }
      else{
        in_peptide = TRUE;
      }
    }
    
    //with in peptide fields
    if(in_peptide){
      
      //check peptide mass
      if(!is_mass_ok){
        //parse peptide mass
        if(sscanf(new_line,"%s %f", 
                  temp_string, &peptide_mass) < 2){                   //FIXME might have to set sring up
          die("crux_index incorrect file format");
          return FALSE;
        }
        
        //check peptide mass within peptide constraint
        if(peptide_mass > get_peptide_constraint_max_mass(index_peptide_iterator->index->constraint) ||
           peptide_mass < get_peptide_constraint_min_mass(index_peptide_iterator->index->constraint)) {
          in_peptide = FALSE;
          continue;
        }
        else{
          is_mass_ok = TRUE;
          continue;
        }
      }
      
      //check length
      if(is_mass_ok){
        //read in the peptide length
        if(sscanf(new_line,"%d", &peptide_length) != 1){
          die("crux_index incorrect file format");
        }
        //check peptide mass within peptide constraint
        if(peptide_length > get_peptide_constraint_max_length(index_peptide_iterator->index->constraint) ||
           peptide_length < get_peptide_constraint_min_length(index_peptide_iterator->index->constraint)) {
          in_peptide = FALSE;
          is_mass_ok = FALSE;
          continue;
        }
        else{
          free(new_line);
          //set file pointer
          index_peptide_iterator->index_file = file;

          //set the peptide mass, length
          *peptide_length_out = peptide_length;
          *peptide_mass_out = peptide_mass;
          
          //found a peptide that meets constraint
          return TRUE;
        }        
      }
    }
  }
  //there's no petide that fits the peptide constraint
  free(new_line);
  return FALSE;
}


/**
 * Assumes that the file* is set at the start of the peptide_src count field
 * creates a new peptide and then adds it to the iterator to return
 * \returns TRUE if successfully parsed the pepdtide from the crux_index_* file
 */
BOOLEAN_T parse_peptide_index_file(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator, 
  float peptide_mass, 
  int peptide_length
  )
{
  PEPTIDE_T* peptide = allocate_peptide();
  FILE* file = index_peptide_iterator->index_file;
  PROTEIN_T* parent_protein = NULL;
  int num_src = -1;
  int current_src = 1;
  int peptide_type = -1;
  //PEPTIDE_TYPE_T peptide_type = -1;
  int start_idx = -1;
  int protein_idx = -1;

  //variables used to parse each line
  char* new_line = NULL;
  int line_length;
  size_t buf_length = 0;


  //set peptide fields
  set_peptide_length( peptide, peptide_length);
  set_peptide_peptide_mass( peptide, peptide_mass);

  //parse each line
  while((line_length =  getline(&new_line, &buf_length, file)) != -1){
    //get total number of peptide src
    if(num_src == -1){
      if(sscanf(new_line,"%d", &num_src) != 1){
        carp(CARP_WARNING, "failed to read number of peptide source, mass: %.2f", peptide_mass);
      }
      continue;
    }
    //get peptide_type
    if(peptide_type == -1){
      if(sscanf(new_line,"%d", &peptide_type) != 1){
        carp(CARP_WARNING, "failed to read peptide_type, mass: %.2f", peptide_mass);
      }
      //peptide_type = new_line[1];
      continue;
    }
    //get start_idx
    if(start_idx == -1){
      if(sscanf(new_line,"%d", &start_idx) != 1){
        carp(CARP_WARNING, "failed to read start_idx, mass: %.2f", peptide_mass);
      }
      //start_idx = new_line[1];
      continue;
    }
    //get protein idx
    if(protein_idx == -1){
      if(sscanf(new_line,"%d", &protein_idx) != 1){
        carp(CARP_WARNING, "failed to read protein_idx, mass: %.2f", peptide_mass);
      }
      //protein_idx = new_line[1];
    }
    
    //set the parent protein
    parent_protein = 
      database_protein_iterator_protein_idx( index_peptide_iterator->db_protein_iterator, 
                                             protein_idx);
    
    //create peptide src, then add it to peptide
    add_peptide_peptide_src(peptide,
                            new_peptide_src( peptide_type, parent_protein, start_idx));
    
    //update current_src
    ++current_src;
  
    //check if there's anymore peptide src to parse
    if(current_src > num_src){
      break;
    }
    //reset variables
    peptide_type = -1;
    start_idx = -1;
    protein_idx = -1;
  }
  //set new peptide in interator
  index_peptide_iterator->peptide = peptide;
  
  return TRUE;   
}

/**
 * \returns TRUE if successfully initiallized the index_peptide_iterator
 */
BOOLEAN_T initialize_index_peptide_iterator(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator ///< the index_peptide_iterator to initialize -in
  )
{
  FILE* file = NULL;
  char* filename = NULL;
  int* peptide_length = (int*)mymalloc(sizeof(int));
  float* peptide_mass = (float*)mymalloc(sizeof(float));
  index_peptide_iterator->has_next = FALSE;

  do{
    //no more index_files to search
    if(index_peptide_iterator->current_index_file >= index_peptide_iterator->total_index_files){
      free(peptide_length);
      free(peptide_mass);
      return TRUE;
    }
    
    filename = 
      index_peptide_iterator->index_files[index_peptide_iterator->current_index_file]->filename;
    
    //update current index file
    ++index_peptide_iterator->current_index_file;
    
    //close previous file
    if(file != NULL){
      fclose(file);
    }

    //open next index file
    file = fopen(filename, "r");
    if(file == NULL){
      carp(CARP_WARNING, "cannot open %s file", filename);
      free(peptide_length);
      free(peptide_mass);
      return FALSE;
    }
  } 
  //move file* to the begining of the first peptide that meets the constraint
  while(!fast_forward_index_file(index_peptide_iterator, file, peptide_length, peptide_mass));
  
  //parse the peptide, adds it to the iterator to return
  if(!parse_peptide_index_file(index_peptide_iterator, *peptide_mass, *peptide_length)){
    carp(CARP_WARNING, "failed to parse peptide in file: %s", filename);
    free(peptide_length);
    free(peptide_mass);
    return FALSE;
  }
  
  //set interator to TRUE, yes there's a next peptide to return
  index_peptide_iterator->has_next = TRUE;
  free(peptide_length);
  free(peptide_mass);
  return TRUE;
}


/**
 * \returns TRUE if successfully setup the index_peptide_iterator for the next iteration
 */
BOOLEAN_T setup_index_peptide_iterator(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator ///< the index_peptide_iterator to initialize -in
  )
{
  
  FILE* file = index_peptide_iterator->index_file;
  char* filename = NULL;
  int* peptide_length = (int*)mymalloc(sizeof(int));
  float* peptide_mass = (float*)mymalloc(sizeof(float));
  index_peptide_iterator->has_next = FALSE;

  //move file* to the begining of the first peptide that meets the constraint
  while(!fast_forward_index_file(index_peptide_iterator, file, peptide_length, peptide_mass)){
    
    //no more index_files to search
    if(index_peptide_iterator->current_index_file >= index_peptide_iterator->total_index_files){
      free(peptide_length);
      free(peptide_mass);
      return TRUE;
    }
    
    filename = 
      index_peptide_iterator->index_files[index_peptide_iterator->current_index_file]->filename;
    
    //update current index file
    ++index_peptide_iterator->current_index_file;

    //open index file
    fclose(file);
    file = fopen(filename, "r");
    if(file == NULL){
      carp(CARP_WARNING, "cannot open %s file", filename);
      free(peptide_length);
      free(peptide_mass);
      return FALSE;
    }
  }
  
  //parse the peptide, adds it to the iterator to return
  if(!parse_peptide_index_file(index_peptide_iterator, *peptide_mass, *peptide_length)){
    carp(CARP_WARNING, "failed to parse peptide in file: %s", filename);
    free(peptide_length);
    free(peptide_mass);
    return FALSE;
  }
  
  //successfully parse peptide
  index_peptide_iterator->has_next = TRUE;
  free(peptide_length);
  free(peptide_mass);
  return TRUE;
}

/***********************************************
 *  The basic index_peptide_iterator functions.
 ***********************************************/


/**
 * Instantiates a new index_peptide_iterator from a index.
 * \returns a new heap allocated index_peptide_iterator object
 */
INDEX_PEPTIDE_ITERATOR_T* new_index_peptide_iterator(
  INDEX_T* index, ///< The index object which we are iterating over -in
  BOOLEAN_T seq ///< output sequence -in
  )
{
  //allocate a new index_peptide_iterator object
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator =
    (INDEX_PEPTIDE_ITERATOR_T*)mycalloc(1, sizeof(INDEX_PEPTIDE_ITERATOR_T));
  
  //set index
  index_peptide_iterator->index = index;
  
  //parse all protein objects from fasta file (the protein array is stored in database object)
  index_peptide_iterator->db_protein_iterator =
    new_database_protein_iterator(index->database);
  
  //parse index_files that are with in peptide_constraint from crux_index_map
  if(!parse_crux_index_map( index_peptide_iterator)){
    //failed to parse crux_index_map
    die("failed to parse crux_index_map file");
  }

  // if no index files to parse, then there's no peptides to return
  // initialize index_file stream at the first new peptide
  if(index_peptide_iterator->total_index_files == 0 ||
     !initialize_index_peptide_iterator(index_peptide_iterator)){
    //no peptides to return
    index_peptide_iterator->has_next = FALSE;
  }
  
  return index_peptide_iterator;
}

/**
 *  The basic iterator functions.
 * \returns The next peptide in the index.
 */
PEPTIDE_T* index_peptide_iterator_next(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator ///< the index_peptide_iterator to initialize -in
  )
{
  PEPTIDE_T* peptide_to_return = index_peptide_iterator->peptide;

  //check if there's actually a peptide to return
  if(!index_peptide_iterator_has_next(index_peptide_iterator) ||
     index_peptide_iterator->peptide == NULL){
    die("index_peptide_iterator, no peptides to return");
  }
  
  //setup the interator for the next peptide, if avaliable
  if(!setup_index_peptide_iterator(index_peptide_iterator)){
    die("failed to setup index_peptide_iterator for next iteration");
  }
 
  return peptide_to_return;
}

/**
 * The basic iterator functions.
 * check to see if the index_peptide_iterator has more peptides to return
 *\returns TRUE if there are additional peptides to iterate over, FALSE if not.
 */
BOOLEAN_T index_peptide_iterator_has_next(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator ///< the index_peptide_iterator to initialize -in
  )
{
  return index_peptide_iterator->has_next; 
}

/**
 * Frees an allocated index_peptide_iterator object.
 */
void free_index_peptide_iterator(
  INDEX_PEPTIDE_ITERATOR_T* index_peptide_iterator ///< the iterator to free -in
  )
{
  int file_idx = 0;
  
  //free database_protein_iterator
  free_database_protein_iterator(index_peptide_iterator->db_protein_iterator);
  //free all index files
  for(; file_idx < index_peptide_iterator->total_index_files; ++file_idx){
    free_index_file(index_peptide_iterator->index_files[file_idx]);
  }
  //close last file
  fclose(index_peptide_iterator->index_file);
  
  //if did not iterate over all peptides, free the last peptide not returned
  if(index_peptide_iterator_has_next(index_peptide_iterator)){
    free_peptide(index_peptide_iterator->peptide);
  }
  free(index_peptide_iterator);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
