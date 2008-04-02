#include <stdlib.h>
#include "check-modifications.h"
#include "../modifications.h"

// also from modifications
int generate_peptide_mod_list_TESTER(
 PEPTIDE_MOD_T*** peptide_mod_list,
 AA_MOD_T** aa_mod_list,
 int num_aa_mods);

// declare things to set up
AA_MOD_T *amod1, *amod2, *amod3;
AA_MOD_T* amod_list[3];
PEPTIDE_MOD_T *pmod1, *pmod2;

void mod_setup(){
  amod1 = new_aa_mod(0);
  amod2 = new_aa_mod(1);
  amod3 = new_aa_mod(2);

  amod_list[0] = amod1;
  amod_list[1] = amod2;
  amod_list[2] = amod3;

  pmod1 = new_peptide_mod();
  pmod2 = new_peptide_mod();
}


void mod_teardown(){
  free_aa_mod(amod1); 
  free_aa_mod(amod2); 
  free_aa_mod(amod3); 
  free_peptide_mod(pmod1); 
  free_peptide_mod(pmod2); 
}

/*
START_TEST(test_aa_list){
  // get pointer to aa list
// test some default values
  // change some values
  // look for changes
}
END_TEST
*/

START_TEST(test_create_aa){
  // check default values

  fail_unless( aa_mod_get_mass_change(amod1) == 0, 
               "amod1 should have had mass change 0 but had %.2f",
               aa_mod_get_mass_change(amod1));

  fail_unless( aa_mod_get_max_per_peptide(amod1) == 0, 
               "amod1 should have had max per peptide 0 but had %d",
               aa_mod_get_max_per_peptide(amod1));

  fail_unless( aa_mod_get_max_distance(amod1) == 40000,//MAX_PROTEIN_SEQ_LENGTH
               "amod1 should have had mass change 40000 but had %d",
               aa_mod_get_max_distance(amod1));

  fail_unless( aa_mod_get_position(amod1) == ANY_POSITION, 
               "amod1 should have had position %d but had %.2f",
               ANY_POSITION, aa_mod_get_position(amod1));

  fail_unless( aa_mod_get_symbol(amod1) == '*', 
               "amod1 should have had symbol * but had %c",
               aa_mod_get_symbol(amod1));
  // FIXME: when these actually get used
  fail_unless( aa_mod_get_identifier(amod1) == 1, 
               "amod1 should have had identifier 1 but had %d",
               aa_mod_get_identifier(amod1));

  fail_unless( aa_mod_get_symbol(amod2) == '@', 
               "amod2 should have had symbol @ but had %c",
               aa_mod_get_symbol(amod2));
  // FIXME: when these actually get used
  fail_unless( aa_mod_get_identifier(amod2) == 2, 
               "amod2 should have had identifier 2 but had %d",
               aa_mod_get_identifier(amod2));
  fail_unless( aa_mod_get_symbol(amod3) == '#', 
               "amod3 should have had symbol # but had %c",
               aa_mod_get_symbol(amod3));
  // FIXME: when these actually get used
  fail_unless( aa_mod_get_identifier(amod3) == 3, 
               "amod3 should have had identifier 3 but had %d",
               aa_mod_get_identifier(amod3));
}
END_TEST

START_TEST(test_set_aa){
  // check default values
  aa_mod_set_mass_change(amod1, 45.6);
  fail_unless( aa_mod_get_mass_change(amod1) == 45.6, 
               "amod1 should have had mass change 45.6 but had %.2f",
               aa_mod_get_mass_change(amod1));

  aa_mod_set_max_per_peptide(amod1, 3);
  fail_unless( aa_mod_get_max_per_peptide(amod1) == 3, 
               "amod1 should have had max per peptide 3 but had %d",
               aa_mod_get_max_per_peptide(amod1));

  aa_mod_set_max_distance(amod1, 1);
  fail_unless( aa_mod_get_max_distance(amod1) == 1,
               "amod1 should have had mass change 1 but had %d",
               aa_mod_get_max_distance(amod1));

  aa_mod_set_max_distance(amod1, -1);
  fail_unless( aa_mod_get_max_distance(amod1) == 40000,//MAX_PROTEIN_SEQ_LENGTH
               "amod1 should have had mass change 40000 but had %d",
               aa_mod_get_max_distance(amod1));

  aa_mod_set_position(amod1, C_TERM);
  fail_unless( aa_mod_get_position(amod1) == C_TERM, 
               "amod1 should have had position %d but had %.2f",
               C_TERM, aa_mod_get_position(amod1));
}
END_TEST

START_TEST(test_create_p){
  fail_unless( peptide_mod_get_mass_change(pmod1) == 0,
               "Default value of peptide mod mass change not 0");
  fail_unless( peptide_mod_get_num_aa_mods(pmod1) == 0,
               "Default value of peptide mod num mods not 0");
}
END_TEST

START_TEST(test_set_p){
  // try adding aa_mods
  double aa_mass = aa_mod_get_mass_change(amod1);
  peptide_mod_add_aa_mod(pmod1, amod1, 1);
  fail_unless( peptide_mod_get_num_aa_mods(pmod1) == 1,
               "Adding an aa mod did not change num mods" );
  fail_unless( peptide_mod_get_mass_change(pmod1) == aa_mass,
               "Adding an aa mod did not correctly set the mass change" );

  peptide_mod_add_aa_mod(pmod1, amod1, 1);
  fail_unless( peptide_mod_get_num_aa_mods(pmod1) == 2,
               "Adding an aa mod did not change num mods" );
  fail_unless( peptide_mod_get_mass_change(pmod1) == aa_mass*2,
               "Adding an aa mod did not correctly set the mass change" );

  // try adding multiple copies
  peptide_mod_add_aa_mod(pmod1, amod1, 10);
  fail_unless( peptide_mod_get_num_aa_mods(pmod1) == 12,
               "Adding an aa mod did not change num mods" );
}
END_TEST

START_TEST(test_compare_p){
  // compare m1 < m2
  peptide_mod_add_aa_mod( pmod1, amod1, 1 );
  peptide_mod_add_aa_mod( pmod2, amod1, 2 );
  fail_unless( compare_peptide_mod_num_aa_mods( &pmod1, &pmod2 ) == -1,
               "Incorrectly compared two mods, first fewer aa than second");
  // compare m2 < m1
  fail_unless( compare_peptide_mod_num_aa_mods( &pmod2, &pmod1 ) == 1,
               "Incorrectly compared two mods, first fewer aa than second");
  // compare m1 = m2
  fail_unless( compare_peptide_mod_num_aa_mods( &pmod1, &pmod1 ) == 0,
               "Incorrectly compared two mods, first fewer aa than second");

  // same comparisons where one pmod has no mods
  free(pmod1);
  pmod1 = new_peptide_mod();
  fail_unless( compare_peptide_mod_num_aa_mods( &pmod1, &pmod2 ) < 0,
               "Incorrectly compared two mods, first fewer aa than second");
  // compare m2 < m1
  fail_unless( compare_peptide_mod_num_aa_mods( &pmod2, &pmod1 ) > 0,
               "Incorrectly compared two mods, first fewer aa than second");
  // compare m1 = m2
  fail_unless( compare_peptide_mod_num_aa_mods( &pmod1, &pmod1 ) == 0,
               "Incorrectly compared two mods, first fewer aa than second");
}
END_TEST

START_TEST(test_sort_p){
  // create an array of peptide_mods with differen numbers of mods
  PEPTIDE_MOD_T* array[4];
  array[0] = new_peptide_mod();
  peptide_mod_add_aa_mod( array[0], amod1, 3);
  array[1] = new_peptide_mod();
  peptide_mod_add_aa_mod( array[1], amod1, 0);
  array[2] = new_peptide_mod();
  peptide_mod_add_aa_mod( array[2], amod1, 8);
  array[3] = new_peptide_mod();
  peptide_mod_add_aa_mod( array[3], amod1, 1);

  // sort the array
  qsort( array, 4, sizeof(PEPTIDE_MOD_T*),
         compare_peptide_mod_num_aa_mods);

  // check the order
  fail_unless( peptide_mod_get_num_aa_mods( array[0] ) == 0, 
               "Sort did not put pmod with 0 nmods first.  There are %d",
               peptide_mod_get_num_aa_mods( array[0] ));
  fail_unless( peptide_mod_get_num_aa_mods( array[1] ) == 1, 
               "Sort did not put pmod with 1 nmods second.  There are %d",
               peptide_mod_get_num_aa_mods( array[1] ));
  fail_unless( peptide_mod_get_num_aa_mods( array[3] ) == 8, 
               "Sort did not put pmod with 8 nmods last.  There are %d",
               peptide_mod_get_num_aa_mods( array[3] ));
}
END_TEST

START_TEST(test_pep_list){
  // TODO test the individual pep mods created
  // one mod, 4 max
  aa_mod_set_max_per_peptide(amod_list[0], 4);
  PEPTIDE_MOD_T** pep_list;
  int num_mods = generate_peptide_mod_list_TESTER(&pep_list, amod_list, 1);
  fail_unless( num_mods == 5,
              "Failed to generate 5 pep mods, instead got %d", num_mods);
  //  free(pep_list);

  // two mods, 4 max and 1 max
  aa_mod_set_max_per_peptide(amod_list[1], 1);
  num_mods = generate_peptide_mod_list_TESTER(&pep_list, amod_list, 2);
  fail_unless( num_mods == 10,
              "Failed to generate 10 pep mods, instead got %d", num_mods);
  //free(pep_list);

  // three mods, 1 max, 2 n mods
  aa_mod_set_max_per_peptide(amod_list[0], 1);
  aa_mod_set_max_per_peptide(amod_list[1], 1);
  aa_mod_set_max_per_peptide(amod_list[2], 1);
  aa_mod_set_position(amod_list[1], N_TERM);
  aa_mod_set_position(amod_list[2], N_TERM);
  num_mods = generate_peptide_mod_list_TESTER(&pep_list, amod_list, 3);
  fail_unless( num_mods == 8,
              "Failed to generate 8 pep mods, instead got %d", num_mods);
  //free(pep_list);
}
END_TEST

/* Boundry conditions test suite */
START_TEST(test_too_many_mods){
  /* This fails as it should, but doesn't get caught nicely
  int i =0;
  for(i=0; i< MAX_AA_MODS +2; i++){
    AA_MOD_T* m = new_aa_mod(i);
    fail_unless( m != NULL, "Could not create the %dth aa mod", i);
    free(m);
  }
  */
}
END_TEST

  // test_aa_null

  // test_p_null

Suite* modifications_suite(){
  Suite* s = suite_create("Modifications\n");
  // Test basic features
  TCase *tc_core = tcase_create("Core");
  tcase_add_test(tc_core, test_create_aa);
  tcase_add_test(tc_core, test_set_aa);
  tcase_add_test(tc_core, test_create_p);
  tcase_add_test(tc_core, test_set_p);
  tcase_add_test(tc_core, test_compare_p);
  tcase_add_test(tc_core, test_sort_p);
  tcase_add_test(tc_core, test_pep_list);
  tcase_add_checked_fixture(tc_core, mod_setup, mod_teardown);
  suite_add_tcase(s, tc_core);

  // Test boundry conditions
  TCase *tc_limits = tcase_create("Limits");
  tcase_add_test(tc_limits, test_too_many_mods);
  tcase_add_checked_fixture(tc_limits, mod_setup, mod_teardown);
  suite_add_tcase(s, tc_limits);

  return s;
}













