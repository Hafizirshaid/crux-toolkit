#ifndef QVALUE_H
#define QVALUE_H

/**
 * \file q-value.h
 * AUTHOR: Barbara Frewen
 * CREATE DATE: November 24, 2008
 ****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include "carp.h"
#include "crux-utils.h"
#include "objects.h"
#include "parameter.h"
#include "protein.h"
#include "peptide.h"
#include "Spectrum.h"
#include "parse_arguments.h" 
#include "SpectrumCollection.h"
#include "generate_peptides_iterator.h"
#include "scorer.h"
#include "match.h"
#include "match_collection.h"
#include "OutputFiles.h"

FLOAT_T* compute_decoy_qvalues(
  FLOAT_T* target_scores,
  int      num_targets,
  FLOAT_T* decoy_scores,
  int      num_decoys,
  FLOAT_T  pi_zero);

FLOAT_T* compute_qvalues_from_pvalues(
  FLOAT_T* pvalues, 
  int      num_pvals,
  FLOAT_T  pi_zero);

MATCH_COLLECTION_T* run_qvalue(
  char* psm_result_folder, 
  char* fasta_file,
  OutputFiles& output );

#endif //QVALUE_H

