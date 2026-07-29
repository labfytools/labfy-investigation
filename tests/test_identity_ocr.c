#include "core/identity_field_extractor.h"
#include "core/mrz_parser.h"
#include "core/ocr_analysis.h"
#include "core/ocr_region_geometry.h"
#include "models/identity_ocr.h"
static void test_models_review(void)
{
 IdentitySourceBox box={.page=1,.x=10,.y=20,.width=80,.height=16,
  .image_width=800,.image_height=500,.available=TRUE};
 IdentityFieldObservation*f=identity_field_observation_new("surname",
  "SPÉCIMEN-D'ESSAI",87.5,&box,0);g_assert_nonnull(f);
 g_assert_cmpint(identity_field_observation_get_status(f),==,IDENTITY_REVIEW_PROPOSED);
 g_assert_true(identity_field_observation_accept(f));
 g_assert_true(identity_field_observation_modify(f,"SPÉCIMEN-D'ESSAI","revue"));
 g_assert_cmpstr(identity_field_observation_get_raw_value(f),==,"SPÉCIMEN-D'ESSAI");
 g_assert_cmpint(identity_field_observation_get_status(f),==,IDENTITY_REVIEW_MODIFIED);
 g_assert_true(identity_field_observation_modify(f,"SPÉCIMEN-D’ESSAI","seconde revue"));
 g_assert_cmpstr(identity_field_observation_get_raw_value(f),==,"SPÉCIMEN-D'ESSAI");
 g_assert_true(identity_field_observation_restore_raw(f));
 g_assert_null(identity_field_observation_get_corrected_value(f));
 g_assert_cmpint(identity_field_observation_get_status(f),==,IDENTITY_REVIEW_PROPOSED);
 g_assert_true(identity_field_observation_modify(f,"SPÉCIMEN-D’ESSAI","revue finale"));
 IdentityFieldObservation*c=identity_field_observation_copy(f);
 identity_field_observation_reject(c);
 g_assert_cmpint(identity_field_observation_get_status(c),==,IDENTITY_REVIEW_REJECTED);
 identity_field_observation_free(c);identity_field_observation_free(f);
 IdentityFieldObservation *manual =
  identity_field_observation_new_manual("nationality","FRANÇAISE",1);
 g_assert_nonnull(manual);
 g_assert_null(identity_field_observation_get_raw_value(manual));
 g_assert_cmpfloat(identity_field_observation_get_confidence(manual),==,-1.0);
 g_assert_cmpstr(identity_field_observation_get_origin(manual),==,"manual_entry");
 g_assert_cmpint(identity_field_observation_get_status(manual),==,
                 IDENTITY_REVIEW_PROPOSED);
 g_assert_true(identity_field_observation_modify(
     manual,"FRANÇAISE","saisie manuelle contrôlée"));
 g_assert_cmpstr(identity_field_observation_get_origin(manual),==,"manual_entry");
 identity_field_observation_free(manual);
 IdentityOcrRun*r=identity_ocr_run_new("11111111-1111-4111-8111-111111111111",
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "identity_card","front",1,"fra+eng","0");
 g_assert_nonnull(r);identity_ocr_run_set_outputs(r,"5.5","fra\neng","params",
  "NOM : SPECIMEN","tsv");
 identity_ocr_run_set_factual_notes(r,
  "Document tronqué. Fragment visible : « publique française ».");
 g_assert_true(identity_ocr_run_set_corrected_transcription(r,
  "NOM : SPÉCIMEN CORRIGÉ","2026-07-29T10:00:00Z"));
 g_assert_true(identity_ocr_run_has_human_transcription(r));
 IdentityOcrRun*copy=identity_ocr_run_copy(r);
 g_assert_cmpstr(identity_ocr_run_get_raw_text(copy),==,"NOM : SPECIMEN");
 g_assert_cmpstr(identity_ocr_run_get_corrected_transcription(copy),==,
  "NOM : SPÉCIMEN CORRIGÉ");
 g_assert_cmpstr(identity_ocr_run_get_transcription_corrected_at(copy),==,
  "2026-07-29T10:00:00Z");
 g_assert_cmpstr(identity_ocr_run_get_factual_notes(copy),==,
  "Document tronqué. Fragment visible : « publique française ».");
 identity_ocr_run_reset_corrected_transcription(copy);
 g_assert_false(identity_ocr_run_has_human_transcription(copy));
 g_assert_null(identity_ocr_run_get_corrected_transcription(copy));
 identity_ocr_run_free(copy);identity_ocr_run_free(r);
}
static void test_extractor(void)
{
 const char*text="NOM : SPÉCIMEN\nPRÉNOMS : ALICE TEST\n"
  "DATE DE NAISSANCE : 01/01/1990\nNUMÉRO DU DOCUMENT : TEST000000\n";
 const char*tsv="level\tpage_num\tblock_num\tpar_num\tline_num\tword_num\tleft\ttop\twidth\theight\tconf\ttext\n"
 "5\t1\t1\t1\t1\t1\t120\t20\t100\t20\t91\tSPÉCIMEN\n";
 GPtrArray*a=identity_field_extractor_extract(text,tsv,800,500,NULL);
 g_assert_cmpuint(a->len,==,4);IdentityFieldObservation*f=g_ptr_array_index(a,0);
 g_assert_true(identity_field_observation_get_box(f)->available);
 g_assert_cmpint(identity_field_observation_get_status(f),==,IDENTITY_REVIEW_PROPOSED);
 g_ptr_array_unref(a);
}
static void test_mrz(void)
{
 g_assert_cmpint(mrz_parser_check_digit("L898902C3"),==,6);
 MrzParseResult*r=mrz_parser_parse(
  "P<UTOERIKSSON<<SPECIMEN<<<<<<<<<<<<<<<<<<<<<\n"
  "L898902C36UTO7408122F1204159ZE184226B<<<<<10\n");
 g_assert_true(r->structure_valid);g_assert_true(r->check_digits_valid);
 g_assert_cmpstr(r->document_number,==,"L898902C3");
 mrz_parse_result_free(r);
}
static void test_geometry(void)
{
 OcrDisplayRegion r;
 g_assert_true(ocr_region_geometry_transform(1000,500,100,50,200,100,
  500,500,OCR_REGION_FIT_CONTAIN,&r));
 g_assert_cmpfloat_with_epsilon(r.x,50,0.01);
 g_assert_cmpfloat_with_epsilon(r.y,150,0.01);
 g_assert_cmpfloat_with_epsilon(r.width,100,0.01);
 g_assert_true(ocr_region_geometry_transform(500,1000,-10,900,100,200,
  1000,500,OCR_REGION_FIT_CONTAIN,&r));
 g_assert_false(ocr_region_geometry_transform(0,100,0,0,1,1,10,10,
  OCR_REGION_FIT_CONTAIN,&r));
 g_assert_false(ocr_region_geometry_transform(100,100,10,10,0,1,10,10,
  OCR_REGION_FIT_CONTAIN,&r));
}
static void test_languages(void)
{
 GPtrArray*parsed=ocr_analysis_parse_languages(
  "List of available languages in /tmp (4):\neng\nfra\neng\nosd\n");
 g_assert_cmpuint(parsed->len,==,3);
 GPtrArray*choices=ocr_analysis_build_language_choices(parsed);
 g_assert_cmpuint(choices->len,==,4);
 g_assert_cmpstr(g_ptr_array_index(choices,3),==,"fra+eng");
 g_ptr_array_unref(choices);g_ptr_array_unref(parsed);
 parsed=ocr_analysis_parse_languages(NULL);
 choices=ocr_analysis_build_language_choices(parsed);
 g_assert_cmpuint(choices->len,==,0);
 g_ptr_array_unref(choices);g_ptr_array_unref(parsed);
}
int main(int argc,char**argv){g_test_init(&argc,&argv,NULL);
 g_test_add_func("/identity-ocr/models-review",test_models_review);
 g_test_add_func("/identity-ocr/extractor",test_extractor);
 g_test_add_func("/identity-ocr/mrz",test_mrz);
 g_test_add_func("/identity-ocr/geometry",test_geometry);
 g_test_add_func("/identity-ocr/languages",test_languages);
 return g_test_run();}
