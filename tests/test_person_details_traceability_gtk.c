#include "widgets/entity_details_panel.h"
#include "models/evidence_record.h"
#include "models/identity_traceability.h"
#include <gtk/gtk.h>
#define PERSON "22000000-0000-4000-8000-000000000020"
#define EVIDENCE "12000000-0000-4000-8000-000000000020"
#define RELATION "72000000-0000-4000-8000-000000000020"
#define AT "2026-08-02T10:00:00Z"
static void collect_text(GtkWidget*w,GString*out){if(GTK_IS_LABEL(w))g_string_append(out,gtk_label_get_text(GTK_LABEL(w)));for(GtkWidget*c=gtk_widget_get_first_child(w);c;c=gtk_widget_get_next_sibling(c)){g_string_append_c(out,'\n');collect_text(c,out);}}
static void test_person_details(void)
{
 GError*error=NULL;EntityDetailsPanel*panel=entity_details_panel_new();g_assert_nonnull(panel);GtkWidget*root=entity_details_panel_get_widget(panel);g_object_ref_sink(root);
 EntityRecord*person=entity_record_new(PERSON,"person","Personne SPECIMEN","Personne SPECIMEN",NULL,0,AT,AT,ENTITY_STATUS_ACTIVE,&error);g_assert_no_error(error);g_assert_nonnull(person);entity_details_panel_set_entity(panel,person);
 EvidenceRecord*evidence=evidence_record_new(EVIDENCE,"preuve-specimen.png","preuve-specimen.png","preuve-specimen.png","2",8,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",AT,NULL,NULL,NULL,EVIDENCE_INTEGRITY_STATUS_UNKNOWN,&error);g_assert_no_error(error);g_assert_nonnull(evidence);
 GPtrArray*evidence_records=g_ptr_array_new();g_ptr_array_add(evidence_records,evidence);PersonEvidenceFactualRelation*relation=person_evidence_factual_relation_new(RELATION,PERSON,EVIDENCE,NULL,"identity_observed_in","Note factuelle SPECIMEN",AT,TRUE);g_assert_nonnull(relation);GPtrArray*relations=g_ptr_array_new();g_ptr_array_add(relations,relation);
 GHashTable*fields=g_hash_table_new(g_str_hash,g_str_equal);g_hash_table_insert(fields,"surname","NOM SPECIMEN");g_hash_table_insert(fields,"birth_date","2000-01-01");
 entity_details_panel_set_person_profile_fields(panel,fields);entity_details_panel_set_person_factual_relations(panel,relations,evidence_records);
	 GString*text=g_string_new(NULL);collect_text(root,text);g_assert_nonnull(strstr(text->str,"Valeurs confirmées du profil"));g_assert_nonnull(strstr(text->str,"NOM SPECIMEN"));g_assert_nonnull(strstr(text->str,"Relations factuelles"));g_assert_nonnull(strstr(text->str,"Identité observée dans la preuve"));g_assert_nonnull(strstr(text->str,"Note factuelle SPECIMEN"));
	 g_string_free(text,TRUE);g_hash_table_unref(fields);g_ptr_array_unref(relations);g_ptr_array_unref(evidence_records);person_evidence_factual_relation_free(relation);evidence_record_free(evidence);entity_record_free(person);entity_details_panel_free(panel);g_object_unref(root);
}
int main(int argc,char**argv){gtk_init();g_test_init(&argc,&argv,NULL);g_test_add_func("/person-details/traceability",test_person_details);return g_test_run();}
