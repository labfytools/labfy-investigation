#include "core/person_ocr_projection_mapping.h"
static const PersonOcrProjectionMapping mappings[] = {
    {"surname", "surname", "Nom déclaré"},
    {"birth_name", "surname", "Nom déclaré"},
    {"given_names", "given_names", "Prénoms déclarés"},
    {"birth_date", "birth_date", "Date de naissance déclarée"},
    {"birth_place", "birth_place", "Lieu de naissance déclaré"},
    {"nationality", "nationality", "Nationalité déclarée"},
    {"sex_as_printed", "sex_as_printed", "Sexe imprimé"},
    {"address_as_printed", "address_as_printed", "Adresse imprimée"}};
const PersonOcrProjectionMapping *
person_ocr_projection_mapping_for(const char *c) {
  for (guint i = 0; i < G_N_ELEMENTS(mappings); i++)
    if (g_strcmp0(c, mappings[i].ocr_code) == 0)
      return &mappings[i];
  return NULL;
}
gboolean person_ocr_projection_mapping_is_compatible(const char *c,
                                                     const char *t) {
  const PersonOcrProjectionMapping *m = person_ocr_projection_mapping_for(c);
  return m && g_strcmp0(m->person_field, t) == 0;
}
